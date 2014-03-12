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

/*
 * First, the stuff that ends up in the outside-world include file
 = typedef off_t regoff_t;
 = typedef struct {
 = 	int re_magic;
 = 	size_t re_nsub;		// number of parenthesized subexpressions
 = 	const char *re_endp;	// end pointer for REG_PEND
 = 	struct re_guts *re_g;	// none of your business :-)
 = } regex_t;
 = typedef struct {
 = 	regoff_t rm_so;		// start of match
 = 	regoff_t rm_eo;		// end of match
 = } regmatch_t;
 */
/*
 * internals of regex_t
 */
#define MAGIC1 ((('r' ^ 0200) << 8) | 'e')

/*
 * The internal representation is a *strip*, a sequence of
 * operators ending with an endmarker.  (Some terminology etc. is a
 * historical relic of earlier versions which used multiple strips.)
 * Certain oddities in the representation are there to permit running
 * the machinery backwards; in particular, any deviation from sequential
 * flow must be marked at both its source and its destination.  Some
 * fine points:
 *
 * - OPLUS_ and O_PLUS are *inside* the loop they create.
 * - OQUEST_ and O_QUEST are *outside* the bypass they create.
 * - OCH_ and O_CH are *outside* the multi-way branch they create, while
 *   OOR1 and OOR2 are respectively the end and the beginning of one of
 *   the branches.  Note that there is an implicit OOR2 following OCH_
 *   and an implicit OOR1 preceding O_CH.
 *
 * In state representations, an operator's bit is on to signify a state
 * immediately *preceding* "execution" of that operator.
 */
typedef long sop; /* strip operator */
typedef long sopno;
#define OPRMASK 0x7c000000
#define OPDMASK 0x03ffffff
#define OPSHIFT (26)
#define OP(n) ((n) & OPRMASK)
#define OPND(n) ((n) & OPDMASK)
#define SOP(op, opnd) ((op) | (opnd))
/* operators			   meaning	operand			*/
/*						(back, fwd are offsets)	*/
#define OEND (1 << OPSHIFT)     /* endmarker	-			*/
#define OCHAR (2 << OPSHIFT)    /* character	unsigned char		*/
#define OBOL (3 << OPSHIFT)     /* left anchor	-			*/
#define OEOL (4 << OPSHIFT)     /* right anchor	-			*/
#define OANY (5 << OPSHIFT)     /* .		-			*/
#define OANYOF (6 << OPSHIFT)   /* [...]	set number		*/
#define OBACK_ (7 << OPSHIFT)   /* begin \d	paren number		*/
#define O_BACK (8 << OPSHIFT)   /* end \d	paren number		*/
#define OPLUS_ (9 << OPSHIFT)   /* + prefix	fwd to suffix		*/
#define O_PLUS (10 << OPSHIFT)  /* + suffix	back to prefix		*/
#define OQUEST_ (11 << OPSHIFT) /* ? prefix	fwd to suffix		*/
#define O_QUEST (12 << OPSHIFT) /* ? suffix	back to prefix		*/
#define OLPAREN (13 << OPSHIFT) /* (		fwd to )		*/
#define ORPAREN (14 << OPSHIFT) /* )		back to (		*/
#define OCH_ (15 << OPSHIFT)    /* begin choice	fwd to OOR2		*/
#define OOR1 (16 << OPSHIFT)    /* | pt. 1	back to OOR1 or OCH_	*/
#define OOR2 (17 << OPSHIFT)    /* | pt. 2	fwd to OOR2 or O_CH	*/
#define O_CH (18 << OPSHIFT)    /* end choice	back to OOR1		*/
#define OBOW (19 << OPSHIFT)    /* begin word	-			*/
#define OEOW (20 << OPSHIFT)    /* end word	-			*/

/*
 * Structure for [] character-set representation.  Character sets are
 * done as bit vectors, grouped 8 to a byte vector for compactness.
 * The individual set therefore has both a pointer to the byte vector
 * and a mask to pick out the relevant bit of each byte.  A hash code
 * simplifies testing whether two sets could be identical.
 *
 * This will get trickier for multicharacter collating elements.  As
 * preliminary hooks for dealing with such things, we also carry along
 * a string of multi-character elements, and decide the size of the
 * vectors at run time.
 */
typedef struct
{
    uch *ptr; /* -> uch [csetsize] */
    uch mask; /* bit within array */
    uch hash; /* hash code */
    size_t smultis;
    char *multis; /* -> char[smulti]  ab\0cd\0ef\0\0 */
} cset;
/* note that CHadd and CHsub are unsafe, and CHIN doesn't yield 0/1 */
#define CHadd(cs, c) ((cs)->ptr[(uch)(c)] |= (cs)->mask, (cs)->hash += (c))
#define CHsub(cs, c) ((cs)->ptr[(uch)(c)] &= ~(cs)->mask, (cs)->hash -= (c))
#define CHIN(cs, c) ((cs)->ptr[(uch)(c)] & (cs)->mask)
#define MCadd(p, cs, cp) vogl_mcadd(p, cs, cp) /* regcomp() internal fns */
#define MCsub(p, cs, cp) vogl_mcsub(p, cs, cp)
#define MCin(p, cs, cp) vogl_mcin(p, cs, cp)

/* stuff for character categories */
typedef unsigned char cat_t;

/*
 * main compiled-expression structure
 */
struct re_guts
{
    int magic;
#define MAGIC2 ((('R' ^ 0200) << 8) | 'E')
    sop *strip;        /* malloced area for strip */
    int csetsize;      /* number of bits in a cset vector */
    int ncsets;        /* number of csets in use */
    cset *sets;        /* -> cset [ncsets] */
    uch *setbits;      /* -> uch[csetsize][ncsets/CHAR_BIT] */
    int cflags;        /* copy of regcomp() cflags argument */
    sopno nstates;     /* = number of sops */
    sopno firststate;  /* the initial OEND (normally 0) */
    sopno laststate;   /* the final OEND */
    int iflags;        /* internal flags */
#define USEBOL 01      /* used ^ */
#define USEEOL 02      /* used $ */
#define BAD 04         /* something wrong */
    int nbol;          /* number of ^ used */
    int neol;          /* number of $ used */
    int ncategories;   /* how many character categories */
    cat_t *categories; /* ->catspace[-CHAR_MIN] */
    char *must;        /* match must contain this string */
    int mlen;          /* length of must */
    size_t nsub;       /* copy of re_nsub */
    int backrefs;      /* does it use back references? */
    sopno nplus;       /* how deep does it nest +s? */
    /* catspace must be last */
    cat_t catspace[1]; /* actually [NC] */
};

/* misc utilities */
#define OUT (CHAR_MAX + 1) /* a non-character value */
#define ISWORD(c) (isalnum(c) || (c) == '_')

#define REGEX_FUNCTIONIZE(a, b) a(b)
#define REGEX_STRINGIZE(a) #a
#define REGEX_INT2STRING(i) REGEX_FUNCTIONIZE(REGEX_STRINGIZE, i)
#define REGEX_FILE_POS_STRING __FILE__ ":" REGEX_INT2STRING(__LINE__)

extern void *vogl_realloc(const char *pFile_line, void *p, size_t new_size);

#define regex_malloc(size) vogl_realloc(REGEX_FILE_POS_STRING, NULL, (size))
#define regex_free(ptr) vogl_realloc(REGEX_FILE_POS_STRING, (ptr), 0)
#define regex_realloc(ptr, newsize) vogl_realloc(REGEX_FILE_POS_STRING, (ptr), (newsize))
