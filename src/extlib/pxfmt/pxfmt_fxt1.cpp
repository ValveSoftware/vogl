/**************************************************************************
 *
 * Copyright 2014 LunarG, Inc.  All Rights Reserved.
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file was ported to pxfmt (from "texcompress_fxt1.c" in Mesa3D) by Ian
 * Elliott (ian@lunarg.com).
 *
 **************************************************************************/

#if defined(_WIN32)
#include <windows.h>
#endif

// FIXME: NEED ALL OF THESE HEADERS?
#include <algorithm> // For std::max()
#include <assert.h>

#include "pxfmt_gl.h"
#include "pxfmt.h"

#define PORTED_FROM_MESA
#include "pxfmt_internal.h"



/***************************************************************************\
 * FXT1 decoder
 *
 * The decoder is based on GL_3DFX_texture_compression_FXT1
 * specification and serves as a concept for the encoder.
\***************************************************************************/


/* lookup table for scaling 5 bit colors up to 8 bits */
static const GLubyte _rgb_scale_5[] = {
   0,   8,   16,  25,  33,  41,  49,  58,
   66,  74,  82,  90,  99,  107, 115, 123,
   132, 140, 148, 156, 165, 173, 181, 189,
   197, 206, 214, 222, 230, 239, 247, 255
};

/* lookup table for scaling 6 bit colors up to 8 bits */
static const GLubyte _rgb_scale_6[] = {
   0,   4,   8,   12,  16,  20,  24,  28,
   32,  36,  40,  45,  49,  53,  57,  61,
   65,  69,  73,  77,  81,  85,  89,  93,
   97,  101, 105, 109, 113, 117, 121, 125,
   130, 134, 138, 142, 146, 150, 154, 158,
   162, 166, 170, 174, 178, 182, 186, 190,
   194, 198, 202, 206, 210, 215, 219, 223,
   227, 231, 235, 239, 243, 247, 251, 255
};


#define CC_SEL(cc, which) (((GLuint *)(cc))[(which) / 32] >> ((which) & 31))
#define UP5(c) _rgb_scale_5[(c) & 31]
#define UP6(c, b) _rgb_scale_6[(((c) & 31) << 1) | ((b) & 1)]
#define LERP(n, t, c0, c1) (((n) - (t)) * (c0) + (t) * (c1) + (n) / 2) / (n)


static void
fxt1_decode_1HI (const GLubyte *code, GLint t, GLubyte *rgba)
{
   const GLuint *cc;

   t *= 3;
   cc = (const GLuint *)(code + t / 8);
   t = (cc[0] >> (t & 7)) & 7;

   if (t == 7) {
      rgba[RCOMP] = rgba[GCOMP] = rgba[BCOMP] = rgba[ACOMP] = 0;
   } else {
      GLubyte r, g, b;
      cc = (const GLuint *)(code + 12);
      if (t == 0) {
         b = UP5(CC_SEL(cc, 0));
         g = UP5(CC_SEL(cc, 5));
         r = UP5(CC_SEL(cc, 10));
      } else if (t == 6) {
         b = UP5(CC_SEL(cc, 15));
         g = UP5(CC_SEL(cc, 20));
         r = UP5(CC_SEL(cc, 25));
      } else {
         b = LERP(6, t, UP5(CC_SEL(cc, 0)), UP5(CC_SEL(cc, 15)));
         g = LERP(6, t, UP5(CC_SEL(cc, 5)), UP5(CC_SEL(cc, 20)));
         r = LERP(6, t, UP5(CC_SEL(cc, 10)), UP5(CC_SEL(cc, 25)));
      }
      rgba[RCOMP] = r;
      rgba[GCOMP] = g;
      rgba[BCOMP] = b;
      rgba[ACOMP] = 255;
   }
}


static void
fxt1_decode_1CHROMA (const GLubyte *code, GLint t, GLubyte *rgba)
{
   const GLuint *cc;
   GLuint kk;

   cc = (const GLuint *)code;
   if (t & 16) {
      cc++;
      t &= 15;
   }
   t = (cc[0] >> (t * 2)) & 3;

   t *= 15;
   cc = (const GLuint *)(code + 8 + t / 8);
   kk = cc[0] >> (t & 7);
   rgba[BCOMP] = UP5(kk);
   rgba[GCOMP] = UP5(kk >> 5);
   rgba[RCOMP] = UP5(kk >> 10);
   rgba[ACOMP] = 255;
}


static void
fxt1_decode_1MIXED (const GLubyte *code, GLint t, GLubyte *rgba)
{
   const GLuint *cc;
   GLuint col[2][3];
   GLint glsb, selb;

   cc = (const GLuint *)code;
   if (t & 16) {
      t &= 15;
      t = (cc[1] >> (t * 2)) & 3;
      /* col 2 */
      col[0][BCOMP] = (*(const GLuint *)(code + 11)) >> 6;
      col[0][GCOMP] = CC_SEL(cc, 99);
      col[0][RCOMP] = CC_SEL(cc, 104);
      /* col 3 */
      col[1][BCOMP] = CC_SEL(cc, 109);
      col[1][GCOMP] = CC_SEL(cc, 114);
      col[1][RCOMP] = CC_SEL(cc, 119);
      glsb = CC_SEL(cc, 126);
      selb = CC_SEL(cc, 33);
   } else {
      t = (cc[0] >> (t * 2)) & 3;
      /* col 0 */
      col[0][BCOMP] = CC_SEL(cc, 64);
      col[0][GCOMP] = CC_SEL(cc, 69);
      col[0][RCOMP] = CC_SEL(cc, 74);
      /* col 1 */
      col[1][BCOMP] = CC_SEL(cc, 79);
      col[1][GCOMP] = CC_SEL(cc, 84);
      col[1][RCOMP] = CC_SEL(cc, 89);
      glsb = CC_SEL(cc, 125);
      selb = CC_SEL(cc, 1);
   }

   if (CC_SEL(cc, 124) & 1) {
      /* alpha[0] == 1 */

      if (t == 3) {
         /* zero */
         rgba[RCOMP] = rgba[BCOMP] = rgba[GCOMP] = rgba[ACOMP] = 0;
      } else {
         GLubyte r, g, b;
         if (t == 0) {
            b = UP5(col[0][BCOMP]);
            g = UP5(col[0][GCOMP]);
            r = UP5(col[0][RCOMP]);
         } else if (t == 2) {
            b = UP5(col[1][BCOMP]);
            g = UP6(col[1][GCOMP], glsb);
            r = UP5(col[1][RCOMP]);
         } else {
            b = (UP5(col[0][BCOMP]) + UP5(col[1][BCOMP])) / 2;
            g = (UP5(col[0][GCOMP]) + UP6(col[1][GCOMP], glsb)) / 2;
            r = (UP5(col[0][RCOMP]) + UP5(col[1][RCOMP])) / 2;
         }
         rgba[RCOMP] = r;
         rgba[GCOMP] = g;
         rgba[BCOMP] = b;
         rgba[ACOMP] = 255;
      }
   } else {
      /* alpha[0] == 0 */
      GLubyte r, g, b;
      if (t == 0) {
         b = UP5(col[0][BCOMP]);
         g = UP6(col[0][GCOMP], glsb ^ selb);
         r = UP5(col[0][RCOMP]);
      } else if (t == 3) {
         b = UP5(col[1][BCOMP]);
         g = UP6(col[1][GCOMP], glsb);
         r = UP5(col[1][RCOMP]);
      } else {
         b = LERP(3, t, UP5(col[0][BCOMP]), UP5(col[1][BCOMP]));
         g = LERP(3, t, UP6(col[0][GCOMP], glsb ^ selb),
                        UP6(col[1][GCOMP], glsb));
         r = LERP(3, t, UP5(col[0][RCOMP]), UP5(col[1][RCOMP]));
      }
      rgba[RCOMP] = r;
      rgba[GCOMP] = g;
      rgba[BCOMP] = b;
      rgba[ACOMP] = 255;
   }
}


static void
fxt1_decode_1ALPHA (const GLubyte *code, GLint t, GLubyte *rgba)
{
   const GLuint *cc;
   GLubyte r, g, b, a;

   cc = (const GLuint *)code;
   if (CC_SEL(cc, 124) & 1) {
      /* lerp == 1 */
      GLuint col0[4];

      if (t & 16) {
         t &= 15;
         t = (cc[1] >> (t * 2)) & 3;
         /* col 2 */
         col0[BCOMP] = (*(const GLuint *)(code + 11)) >> 6;
         col0[GCOMP] = CC_SEL(cc, 99);
         col0[RCOMP] = CC_SEL(cc, 104);
         col0[ACOMP] = CC_SEL(cc, 119);
      } else {
         t = (cc[0] >> (t * 2)) & 3;
         /* col 0 */
         col0[BCOMP] = CC_SEL(cc, 64);
         col0[GCOMP] = CC_SEL(cc, 69);
         col0[RCOMP] = CC_SEL(cc, 74);
         col0[ACOMP] = CC_SEL(cc, 109);
      }

      if (t == 0) {
         b = UP5(col0[BCOMP]);
         g = UP5(col0[GCOMP]);
         r = UP5(col0[RCOMP]);
         a = UP5(col0[ACOMP]);
      } else if (t == 3) {
         b = UP5(CC_SEL(cc, 79));
         g = UP5(CC_SEL(cc, 84));
         r = UP5(CC_SEL(cc, 89));
         a = UP5(CC_SEL(cc, 114));
      } else {
         b = LERP(3, t, UP5(col0[BCOMP]), UP5(CC_SEL(cc, 79)));
         g = LERP(3, t, UP5(col0[GCOMP]), UP5(CC_SEL(cc, 84)));
         r = LERP(3, t, UP5(col0[RCOMP]), UP5(CC_SEL(cc, 89)));
         a = LERP(3, t, UP5(col0[ACOMP]), UP5(CC_SEL(cc, 114)));
      }
   } else {
      /* lerp == 0 */

      if (t & 16) {
         cc++;
         t &= 15;
      }
      t = (cc[0] >> (t * 2)) & 3;

      if (t == 3) {
         /* zero */
         r = g = b = a = 0;
      } else {
         GLuint kk;
         cc = (const GLuint *)code;
         a = UP5(cc[3] >> (t * 5 + 13));
         t *= 15;
         cc = (const GLuint *)(code + 8 + t / 8);
         kk = cc[0] >> (t & 7);
         b = UP5(kk);
         g = UP5(kk >> 5);
         r = UP5(kk >> 10);
      }
   }
   rgba[RCOMP] = r;
   rgba[GCOMP] = g;
   rgba[BCOMP] = b;
   rgba[ACOMP] = a;
}


static void
fxt1_decode_1 (const void *texture, GLint stride, /* in pixels */
               GLint i, GLint j, GLubyte *rgba)
{
   static void (*decode_1[]) (const GLubyte *, GLint, GLubyte *) = {
      fxt1_decode_1HI,     /* cc-high   = "00?" */
      fxt1_decode_1HI,     /* cc-high   = "00?" */
      fxt1_decode_1CHROMA, /* cc-chroma = "010" */
      fxt1_decode_1ALPHA,  /* alpha     = "011" */
      fxt1_decode_1MIXED,  /* mixed     = "1??" */
      fxt1_decode_1MIXED,  /* mixed     = "1??" */
      fxt1_decode_1MIXED,  /* mixed     = "1??" */
      fxt1_decode_1MIXED   /* mixed     = "1??" */
   };

   const GLubyte *code = (const GLubyte *)texture +
                         ((j / 4) * (stride / 8) + (i / 8)) * 16;
   GLint mode = CC_SEL(code, 125);
   GLint t = i & 7;

   if (t & 4) {
      t += 12;
   }
   t += (j & 3) * 4;

   decode_1[mode](code, t, rgba);
}




static void
fetch_rgb_fxt1(const GLubyte *map,
               GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   GLubyte rgba[4];
   fxt1_decode_1(map, rowStride, i, j, rgba);
   texel[RCOMP] = UBYTE_TO_FLOAT(rgba[RCOMP]);
   texel[GCOMP] = UBYTE_TO_FLOAT(rgba[GCOMP]);
   texel[BCOMP] = UBYTE_TO_FLOAT(rgba[BCOMP]);
   texel[ACOMP] = 1.0F;
}


static void
fetch_rgba_fxt1(const GLubyte *map,
                GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   GLubyte rgba[4];
   fxt1_decode_1(map, rowStride, i, j, rgba);
   texel[RCOMP] = UBYTE_TO_FLOAT(rgba[RCOMP]);
   texel[GCOMP] = UBYTE_TO_FLOAT(rgba[GCOMP]);
   texel[BCOMP] = UBYTE_TO_FLOAT(rgba[BCOMP]);
   texel[ACOMP] = UBYTE_TO_FLOAT(rgba[ACOMP]);
}


// This LunarG-written function interfaces the pxfmt code with the Mesa3D code.
void decompress_fxt1(float *intermediate, const void *pSrc,
                     uint32 row_stride, int x, int y,
                     const pxfmt_sized_format fmt)
{
    switch (fmt)
    {
    case PXFMT_COMPRESSED_RGB_FXT1:
        fetch_rgb_fxt1((const GLubyte *) pSrc, row_stride, x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_RGBA_FXT1:
        fetch_rgba_fxt1((const GLubyte *) pSrc, row_stride, x, y, intermediate);
        break;
    default:
        // Don't try to handle the non-FXT1 cases
        break;
    }
}
