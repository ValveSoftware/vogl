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

/* =====================================================================
   File:	rmalloc.h
   Author:	Rammi
   Date:	11/16/1995

   License:	(This is the open source ISC license, see
             http://en.wikipedia.org/wiki/ISC_license
             for more info)

    Copyright Â© 2010	Andreas M. Rammelt <rammi@hexco.de>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


   Content:	Switching on/off of malloc debug library. This is done
      by preprocessor switch MALLOC_DEBUG.

      If MALLOC_DEBUG is defined, special debug versions for
      the functions of the standard malloc lib are used.
      They try to get hold of the following things:

      * Overwrite borders of allocated block
      * Multiple free of a block
      * free of an unallocated block

      All sources must include this file. No more changes necessary.

      Special macros:
      RM_TEST 	 Force testing of all allocated blocks.

      RM_STAT 	 Show a statistic of allocated blocks.

      RM_RETAG(p)  change position for allocated block to pos of
             this macro

      RM_SET(p, f) Set flag f for block p. f can be a combination of
             the following:
         RM_STATIC Block is considered static. Is only summarized
                      in statistics.

         RM_STRING Block is meant to hold a string.

      The global behaviour can be changed by editing, recompiling and
      linking rmalloc.c. See information there.

   Changes:
            See rmalloc.c

   ===================================================================== */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* =========
	   INCLUDEs:
	   ========= */

#include <stdlib.h>
#include <string.h>

/* ========
	   DEFINEs:
	   ======== */

#define RM_UPPERSTYLE /* this is only used for backward compatibility */

/* once again useful: INT2STRING(prepro_macro_containing_number)-->"number" */
#define FUNCTIONIZE(a, b) a(b)
#define STRINGIZE(a) #a
#define INT2STRING(i) FUNCTIONIZE(STRINGIZE, i)

/* Flags in header (with WITH_FLAGS defined, see rmalloc.c) */
#define RM_STATIC (0x0001 << 0) /* static memory */
#define RM_STRING (0x0001 << 1) /* contains string */

#ifdef MALLOC_DEBUG
/* Useful, to build 1 string from __FILE__ & __LINE__ in compile time: */
#define RM_FILE_POS __FILE__ ":" INT2STRING(__LINE__)

#ifdef RM_UPPERSTYLE
/* Deprecated: Only used for backward compatibility: */
#define MALLOC(s) Rmalloc((s), RM_FILE_POS)
#define CALLOC(n, s) Rcalloc((n), (s), RM_FILE_POS)
#define REALLOC(p, s) Rrealloc((p), (s), RM_FILE_POS)
#define FREE(p) Rfree((p), RM_FILE_POS)
#define FREE0(p) Rfree((p), RM_FILE_POS), (p) = NULL
#define STRDUP(s) Rstrdup((s), RM_FILE_POS)
#define MALLOC_USABLE_SIZE(s) Rmalloc_usable_size(s, RM_FILE_POS)
#else /* !RM_UPPERSTYLE */
/* Wrap with our stuff: */
#ifdef malloc
#undef malloc
#endif
#define malloc(s) Rmalloc((s), RM_FILE_POS)

#ifdef calloc
#undef calloc
#endif
#define calloc(n, s) Rcalloc((n), (s), RM_FILE_POS)

#ifdef realloc
#undef realloc
#endif
#define realloc(p, s) Rrealloc((p), (s), RM_FILE_POS)

#ifdef free
#undef free
#endif
#define free(p) Rfree((p), RM_FILE_POS)

#ifdef free0
#undef free0
#endif
#define free0(p) Rfree((p), RM_FILE_POS), (p) = NULL

#ifdef strdup
#undef strdup
#endif
#define strdup(s) Rstrdup((s), RM_FILE_POS)

#ifdef getcwd
#undef getcwd
#endif
#define getcwd(b, s) Rgetcwd((b), (s), RM_FILE_POS)

#ifdef malloc_usable_size
#undef malloc_usable_size
#endif
#define malloc_usable_size(s) Rmalloc_usable_size((s), RM_FILE_POS)
#endif /* RM_UPPERSTYLE */

#define RM_TEST Rmalloc_test(RM_FILE_POS)
#define RM_STAT Rmalloc_stat(RM_FILE_POS)
#define RM_RETAG(p) Rmalloc_retag((p), RM_FILE_POS)
#define RM_SET(p, f) Rmalloc_set_flags((p), (f), RM_FILE_POS)

#else /* ! MALLOC_DEBUG */

#ifdef RM_UPPERSTYLE
/* The normal stuff (backward compatibility): */
#define MALLOC(s) malloc((s))
#define CALLOC(n, s) calloc((n), (s))
#define REALLOC(p, s) realloc((p), (s))
#define FREE(p) free((p))
#define FREE0(p) free((p)), (p) = NULL
#define STRDUP(s) (char *) strdup((s))
#define MALLOC_USABLE_SIZE(s) malloc_usable_size(s)
#else /* !RM_UPPERSTYLE */
#define free0(p) free((p)), (p) = NULL
#endif /* RM_UPPERSTYLE */
#define RM_TEST
#define RM_STAT
#define RM_RETAG(p) (p)
#define RM_SET(p, f) (p)

#endif /* ! MALLOC_DEBUG */

/* ===========
	   PROTOTYPES:
	   =========== */

#if defined(MALLOC_DEBUG) || defined(RM_NEED_PROTOTYPES)

typedef void *(*rmalloc_malloc_func_t)(size_t size, void *pUser);
typedef void (*rmalloc_free_func_t)(void *ptr, void *pUser);
typedef void *(*rmalloc_realloc_func_t)(void *ptr, size_t size, void *pUser);

void Rmalloc_set_callbacks(rmalloc_malloc_func_t pMalloc, rmalloc_free_func_t pFree, rmalloc_realloc_func_t pRealloc, void *pUser);

void *Rmalloc(size_t size, const char *file);
void *Rcalloc(size_t nelem, size_t size, const char *file);
void *Rrealloc(void *p, size_t size, const char *file);
void Rfree(void *p, const char *file);
char *Rstrdup(const char *str, const char *file);
char *Rgetcwd(char *buffer, size_t size, const char *file);
void Rtest_malloc(const char *file);
void Rmalloc_test(const char *file);
void Rmalloc_stat(const char *file);
void *Rmalloc_retag(void *p, const char *file);
void *Rmalloc_set_flags(void *p, unsigned flags, const char *file);
size_t Rmalloc_usable_size(void *p, const char *file);
#endif /* MALLOC_DEBUG || RM_NEED_PROTOTYPES */

#ifdef __cplusplus
}
#endif

