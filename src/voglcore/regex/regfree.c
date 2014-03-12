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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include "regex.h"
#include "utils.h"
#include "regex2.h"

/*
 - vogl_regfree - free everything
 = extern void regfree(regex_t *);
 */
void
vogl_regfree(preg)
    regex_t *preg;
{
    register struct re_guts *g;

    if (preg->re_magic != MAGIC1) /* oops */
        return;                   /* nice to complain, but hard */

    g = preg->re_g;
    if (g == NULL || g->magic != MAGIC2) /* oops again */
        return;
    preg->re_magic = 0; /* mark it invalid */
    g->magic = 0;       /* mark it invalid */

    if (g->strip != NULL)
        regex_free((char *)g->strip);
    if (g->sets != NULL)
        regex_free((char *)g->sets);
    if (g->setbits != NULL)
        regex_free((char *)g->setbits);
    if (g->must != NULL)
        regex_free(g->must);
    regex_free((char *)g);
}
