/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#ifndef D_FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

/*
 * the outer shell of vogl_regexec()
 *
 * This file includes engine.c *twice*, after muchos fiddling with the
 * macros that code uses.  This lets the same code operate on two different
 * representations for state sets.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "regex.h"

#include "utils.h"
#include "regex2.h"

// static int vogl_nope = 0;		/* for use in asserts; shuts lint up */

/* macros for manipulating states, small version */
#define states unsigned
#define states1 unsigned /* for later use in vogl_regexec() decision */
#define CLEAR(v) ((v) = 0)
#define SET0(v, n) ((v) &= ~((unsigned)1 << (n)))
#define SET1(v, n) ((v) |= (unsigned)1 << (n))
#define ISSET(v, n) ((v) & ((unsigned)1 << (n)))
#define ASSIGN(d, s) ((d) = (s))
#define EQ(a, b) ((a) == (b))
#define STATEVARS int dummy /* dummy version */
#define STATESETUP(m, n)    /* nothing */
#define STATETEARDOWN(m)    /* nothing */
#define SETUP(v) ((v) = 0)
#define onestate unsigned
#define INIT(o, n) ((o) = (unsigned)1 << (n))
#define INC(o) ((o) <<= 1)
#define ISSTATEIN(v, o) ((v) & (o))
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define FWD(dst, src, n) ((dst) |= ((unsigned)(src) & (here)) << (n))
#define BACK(dst, src, n) ((dst) |= ((unsigned)(src) & (here)) >> (n))
#define ISSETBACK(v, n) ((v) & ((unsigned)here >> (n)))
/* function names */
#define SNAMES /* engine.c looks after details */

#include "engine.c"

/* now undo things */
#undef states
#undef CLEAR
#undef SET0
#undef SET1
#undef ISSET
#undef ASSIGN
#undef EQ
#undef STATEVARS
#undef STATESETUP
#undef STATETEARDOWN
#undef SETUP
#undef onestate
#undef INIT
#undef INC
#undef ISSTATEIN
#undef FWD
#undef BACK
#undef ISSETBACK
#undef SNAMES

/* macros for manipulating states, large version */
#define states char *
#define CLEAR(v) memset(v, 0, m->g->nstates)
#define SET0(v, n) ((v)[n] = 0)
#define SET1(v, n) ((v)[n] = 1)
#define ISSET(v, n) ((v)[n])
#define ASSIGN(d, s) memcpy(d, s, m->g->nstates)
#define EQ(a, b) (memcmp(a, b, m->g->nstates) == 0)
#define STATEVARS \
    int vn;       \
    char *space
#define STATESETUP(m, nv)                                  \
    {                                                      \
        (m)->space = regex_malloc((nv) * (m)->g->nstates); \
        if ((m)->space == NULL)                            \
            return (REG_ESPACE);                           \
        (m)->vn = 0;                                       \
    }
#define STATETEARDOWN(m)        \
    {                           \
        regex_free((m)->space); \
    }
#define SETUP(v) ((v) = &m->space[m->vn++ * m->g->nstates])
#define onestate int
#define INIT(o, n) ((o) = (n))
#define INC(o) ((o)++)
#define ISSTATEIN(v, o) ((v)[o])
/* some abbreviations; note that some of these know variable names! */
/* do "if I'm here, I can also be there" etc without branches */
#define FWD(dst, src, n) ((dst)[here + (n)] |= (src)[here])
#define BACK(dst, src, n) ((dst)[here - (n)] |= (src)[here])
#define ISSETBACK(v, n) ((v)[here - (n)])
/* function names */
#define LNAMES /* flag */

#include "engine.c"

#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate, __LINE__, file)

#define _impl_PASTE(a, b) a##b
#define _impl_CASSERT_LINE(predicate, line, file) \
    typedef char _impl_PASTE(assertion_failed_##file##_, line)[2 * !!(predicate) - 1];

/*
 - vogl_regexec - interface for matching
 = extern int vogl_regexec(const regex_t *, const char *, size_t, \
 =					regmatch_t [], int);
 = #define	REG_NOTBOL	00001
 = #define	REG_NOTEOL	00002
 = #define	REG_STARTEND	00004
 = #define	REG_TRACE	00400	// tracing of execution
 = #define	REG_LARGE	01000	// force large representation
 = #define	REG_BACKR	02000	// force use of backref code
 *
 * We put this here so we can exploit knowledge of the state representation
 * when choosing which matcher to call.  Also, by this point the matchers
 * have been prototyped.
 */
int /* 0 success, REG_NOMATCH failure */
    vogl_regexec(preg, string, nmatch, pmatch, eflags)
    const regex_t *preg;
	const char *string;
	size_t nmatch;
	regmatch_t pmatch[];
	int eflags;
{
    register struct re_guts *g = preg->re_g;
#ifdef REDEBUG
#define GOODFLAGS(f) (f)
#else
#define GOODFLAGS(f) ((f) & (REG_NOTBOL | REG_NOTEOL | REG_STARTEND))
#endif

    if (preg->re_magic != MAGIC1 || g->magic != MAGIC2)
        return (REG_BADPAT);
    assert(!(g->iflags & BAD));
    if (g->iflags & BAD) /* backstop for no-debug case */
        return (REG_BADPAT);
    eflags = GOODFLAGS(eflags);

    if (g->nstates <= (long)(CHAR_BIT * sizeof(states1)) && !(eflags & REG_LARGE))
        return (smatcher(g, (char *)string, nmatch, pmatch, eflags));
    else
        return (lmatcher(g, (char *)string, nmatch, pmatch, eflags));
}
