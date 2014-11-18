/**************************************************************************
 *
 * Copyright 2014 LunarG, Inc.  All Rights Reserved.
 * Copyright (C) 2011 LunarG, Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This file was ported to pxfmt (from "texcompress_etc.c" and
 * "texcompress_etc_tmp.h" in Mesa3D) by Ian Elliott (ian@lunarg.com).
 *
 **************************************************************************/

/**
 * \file texcompress_etc.c
 * GL_OES_compressed_ETC1_RGB8_texture support.
 * Supported ETC2 texture formats are:
 * GL_COMPRESSED_RGB8_ETC2
 * GL_COMPRESSED_SRGB8_ETC2
 * GL_COMPRESSED_RGBA8_ETC2_EAC
 * GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
 * GL_COMPRESSED_R11_EAC
 * GL_COMPRESSED_RG11_EAC
 * GL_COMPRESSED_SIGNED_R11_EAC
 * GL_COMPRESSED_SIGNED_RG11_EAC
 * MESA_FORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1
 * MESA_FORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1
 */

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



struct etc2_block {
   int distance;
   uint64_t pixel_indices[2];
   const int *modifier_tables[2];
   bool flipped;
   bool opaque;
   bool is_ind_mode;
   bool is_diff_mode;
   bool is_t_mode;
   bool is_h_mode;
   bool is_planar_mode;
   uint8_t base_colors[3][3];
   uint8_t paint_colors[4][3];
   uint8_t base_codeword;
   uint8_t multiplier;
   uint8_t table_index;
};

static const int etc2_distance_table[8] = {
   3, 6, 11, 16, 23, 32, 41, 64 };

static const int etc2_modifier_tables[16][8] = {
   {  -3,   -6,   -9,  -15,   2,   5,   8,   14},
   {  -3,   -7,  -10,  -13,   2,   6,   9,   12},
   {  -2,   -5,   -8,  -13,   1,   4,   7,   12},
   {  -2,   -4,   -6,  -13,   1,   3,   5,   12},
   {  -3,   -6,   -8,  -12,   2,   5,   7,   11},
   {  -3,   -7,   -9,  -11,   2,   6,   8,   10},
   {  -4,   -7,   -8,  -11,   3,   6,   7,   10},
   {  -3,   -5,   -8,  -11,   2,   4,   7,   10},
   {  -2,   -6,   -8,  -10,   1,   5,   7,    9},
   {  -2,   -5,   -8,  -10,   1,   4,   7,    9},
   {  -2,   -4,   -8,  -10,   1,   3,   7,    9},
   {  -2,   -5,   -7,  -10,   1,   4,   6,    9},
   {  -3,   -4,   -7,  -10,   2,   3,   6,    9},
   {  -1,   -2,   -3,  -10,   0,   1,   2,    9},
   {  -4,   -6,   -8,   -9,   3,   5,   7,    8},
   {  -3,   -5,   -7,   -9,   2,   4,   6,    8},
};

static const int etc2_modifier_tables_non_opaque[8][4] = {
   { 0,   8,   0,    -8},
   { 0,   17,  0,   -17},
   { 0,   29,  0,   -29},
   { 0,   42,  0,   -42},
   { 0,   60,  0,   -60},
   { 0,   80,  0,   -80},
   { 0,   106, 0,  -106},
   { 0,   183, 0,  -183}
};

/* define etc1_parse_block and etc. */
#define UINT8_TYPE GLubyte
#define TAG(x) x
#include "texcompress_etc_tmp.h"
#undef TAG
#undef UINT8_TYPE


static uint8_t
etc2_base_color1_t_mode(const uint8_t *in, GLuint index)
{
   uint8_t R1a = 0, x = 0;
   /* base col 1 = extend_4to8bits( (R1a << 2) | R1b, G1, B1) */
   switch(index) {
   case 0:
      R1a = (in[0] >> 3) & 0x3;
      x = ((R1a << 2) | (in[0] & 0x3));
      break;
   case 1:
      x = ((in[1] >> 4) & 0xf);
      break;
   case 2:
      x = (in[1] & 0xf);
      break;
   default:
      /* invalid index */
      break;
   }
   return ((x << 4) | (x & 0xf));
}

static uint8_t
etc2_base_color2_t_mode(const uint8_t *in, GLuint index)
{
   uint8_t x = 0;
   /*extend 4to8bits(R2, G2, B2)*/
   switch(index) {
   case 0:
      x = ((in[2] >> 4) & 0xf );
      break;
   case 1:
      x = (in[2] & 0xf);
      break;
   case 2:
      x = ((in[3] >> 4) & 0xf);
      break;
   default:
      /* invalid index */
      break;
   }
   return ((x << 4) | (x & 0xf));
}

static uint8_t
etc2_base_color1_h_mode(const uint8_t *in, GLuint index)
{
   uint8_t x = 0;
   /* base col 1 = extend 4to8bits(R1, (G1a << 1) | G1b, (B1a << 3) | B1b) */
   switch(index) {
   case 0:
      x = ((in[0] >> 3) & 0xf);
      break;
   case 1:
      x = (((in[0] & 0x7) << 1) | ((in[1] >> 4) & 0x1));
      break;
   case 2:
      x = ((in[1] & 0x8) |
           (((in[1] & 0x3) << 1) | ((in[2] >> 7) & 0x1)));
      break;
   default:
      /* invalid index */
      break;
   }
   return ((x << 4) | (x & 0xf));
}

static uint8_t
etc2_base_color2_h_mode(const uint8_t *in, GLuint index)
{
   uint8_t x = 0;
   /* base col 2 = extend 4to8bits(R2, G2, B2) */
   switch(index) {
   case 0:
      x = ((in[2] >> 3) & 0xf );
      break;
   case 1:
      x = (((in[2] & 0x7) << 1) | ((in[3] >> 7) & 0x1));
      break;
   case 2:
      x = ((in[3] >> 3) & 0xf);
      break;
   default:
      /* invalid index */
      break;
   }
   return ((x << 4) | (x & 0xf));
}

static uint8_t
etc2_base_color_o_planar(const uint8_t *in, GLuint index)
{
   GLuint tmp;
   switch(index) {
   case 0:
      tmp = ((in[0] >> 1) & 0x3f); /* RO */
      return ((tmp << 2) | (tmp >> 4));
   case 1:
      tmp = (((in[0] & 0x1) << 6) | /* GO1 */
             ((in[1] >> 1) & 0x3f)); /* GO2 */
      return ((tmp << 1) | (tmp >> 6));
   case 2:
      tmp = (((in[1] & 0x1) << 5) | /* BO1 */
             (in[2] & 0x18) | /* BO2 */
             (((in[2] & 0x3) << 1) | ((in[3] >> 7) & 0x1))); /* BO3 */
      return ((tmp << 2) | (tmp >> 4));
    default:
      /* invalid index */
      return 0;
   }
}

static uint8_t
etc2_base_color_h_planar(const uint8_t *in, GLuint index)
{
   GLuint tmp;
   switch(index) {
   case 0:
      tmp = (((in[3] & 0x7c) >> 1) | /* RH1 */
             (in[3] & 0x1));         /* RH2 */
      return ((tmp << 2) | (tmp >> 4));
   case 1:
      tmp = (in[4] >> 1) & 0x7f; /* GH */
      return ((tmp << 1) | (tmp >> 6));
   case 2:
      tmp = (((in[4] & 0x1) << 5) |
             ((in[5] >> 3) & 0x1f)); /* BH */
      return ((tmp << 2) | (tmp >> 4));
   default:
      /* invalid index */
      return 0;
   }
}

static uint8_t
etc2_base_color_v_planar(const uint8_t *in, GLuint index)
{
   GLuint tmp;
   switch(index) {
   case 0:
      tmp = (((in[5] & 0x7) << 0x3) |
             ((in[6] >> 5) & 0x7)); /* RV */
      return ((tmp << 2) | (tmp >> 4));
   case 1:
      tmp = (((in[6] & 0x1f) << 2) |
             ((in[7] >> 6) & 0x3)); /* GV */
      return ((tmp << 1) | (tmp >> 6));
   case 2:
      tmp = in[7] & 0x3f; /* BV */
      return ((tmp << 2) | (tmp >> 4));
   default:
      /* invalid index */
      return 0;
   }
}

static GLint
etc2_get_pixel_index(const struct etc2_block *block, int x, int y)
{
   int bit = ((3 - y) + (3 - x) * 4) * 3;
   int idx = (block->pixel_indices[1] >> bit) & 0x7;
   return idx;
}

static uint8_t
etc2_clamp(int color)
{
   /* CLAMP(color, 0, 255) */
   return (uint8_t) CLAMP(color, 0, 255);
}

static GLushort
etc2_clamp2(int color)
{
   /* CLAMP(color, 0, 2047) */
   return (GLushort) CLAMP(color, 0, 2047);
}

static GLshort
etc2_clamp3(int color)
{
   /* CLAMP(color, -1023, 1023) */
   return (GLshort) CLAMP(color, -1023, 1023);
}

static void
etc2_rgb8_parse_block(struct etc2_block *block,
                      const uint8_t *src,
                      GLboolean punchthrough_alpha)
{
   unsigned i;
   GLboolean diffbit = false;
   static const int lookup[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };

   const int R_plus_dR = (src[0] >> 3) + lookup[src[0] & 0x7];
   const int G_plus_dG = (src[1] >> 3) + lookup[src[1] & 0x7];
   const int B_plus_dB = (src[2] >> 3) + lookup[src[2] & 0x7];

   /* Reset the mode flags */
   block->is_ind_mode = false;
   block->is_diff_mode = false;
   block->is_t_mode = false;
   block->is_h_mode = false;
   block->is_planar_mode = false;

   if (punchthrough_alpha)
      block->opaque = src[3] & 0x2;
   else
      diffbit = src[3] & 0x2;

   if (!diffbit && !punchthrough_alpha) {
      /* individual mode */
      block->is_ind_mode = true;

      for (i = 0; i < 3; i++) {
         /* Texture decode algorithm is same for individual mode in etc1
          * & etc2.
          */
         block->base_colors[0][i] = etc1_base_color_ind_hi(src[i]);
         block->base_colors[1][i] = etc1_base_color_ind_lo(src[i]);
      }
   }
   else if (R_plus_dR < 0 || R_plus_dR > 31){
      /* T mode */
      block->is_t_mode = true;

      for(i = 0; i < 3; i++) {
         block->base_colors[0][i] = etc2_base_color1_t_mode(src, i);
         block->base_colors[1][i] = etc2_base_color2_t_mode(src, i);
      }
      /* pick distance */
      block->distance =
         etc2_distance_table[(((src[3] >> 2) & 0x3) << 1) |
                             (src[3] & 0x1)];

      for (i = 0; i < 3; i++) {
         block->paint_colors[0][i] = etc2_clamp(block->base_colors[0][i]);
         block->paint_colors[1][i] = etc2_clamp(block->base_colors[1][i] +
                                                block->distance);
         block->paint_colors[2][i] = etc2_clamp(block->base_colors[1][i]);
         block->paint_colors[3][i] = etc2_clamp(block->base_colors[1][i] -
                                                block->distance);
      }
   }
   else if (G_plus_dG < 0 || G_plus_dG > 31){
      int base_color_1_value, base_color_2_value;

      /* H mode */
      block->is_h_mode = true;

      for(i = 0; i < 3; i++) {
         block->base_colors[0][i] = etc2_base_color1_h_mode(src, i);
         block->base_colors[1][i] = etc2_base_color2_h_mode(src, i);
      }

      base_color_1_value = (block->base_colors[0][0] << 16) +
                           (block->base_colors[0][1] << 8) +
                           block->base_colors[0][2];
      base_color_2_value = (block->base_colors[1][0] << 16) +
                           (block->base_colors[1][1] << 8) +
                           block->base_colors[1][2];
      /* pick distance */
      block->distance =
         etc2_distance_table[(src[3] & 0x4) |
                             ((src[3] & 0x1) << 1) |
                             (base_color_1_value >= base_color_2_value)];

      for (i = 0; i < 3; i++) {
         block->paint_colors[0][i] = etc2_clamp(block->base_colors[0][i] +
                                                block->distance);
         block->paint_colors[1][i] = etc2_clamp(block->base_colors[0][i] -
                                                block->distance);
         block->paint_colors[2][i] = etc2_clamp(block->base_colors[1][i] +
                                                block->distance);
         block->paint_colors[3][i] = etc2_clamp(block->base_colors[1][i] -
                                                block->distance);
      }
   }
   else if (B_plus_dB < 0 || B_plus_dB > 31) {
      /* Planar mode */
      block->is_planar_mode = true;

      /* opaque bit must be set in planar mode */
      block->opaque = true;

      for (i = 0; i < 3; i++) {
         block->base_colors[0][i] = etc2_base_color_o_planar(src, i);
         block->base_colors[1][i] = etc2_base_color_h_planar(src, i);
         block->base_colors[2][i] = etc2_base_color_v_planar(src, i);
      }
   }
   else if (diffbit || punchthrough_alpha) {
      /* differential mode */
      block->is_diff_mode = true;

      for (i = 0; i < 3; i++) {
         /* Texture decode algorithm is same for differential mode in etc1
          * & etc2.
          */
         block->base_colors[0][i] = etc1_base_color_diff_hi(src[i]);
         block->base_colors[1][i] = etc1_base_color_diff_lo(src[i]);
      }
   }

   if (block->is_ind_mode || block->is_diff_mode) {
      int table1_idx = (src[3] >> 5) & 0x7;
      int table2_idx = (src[3] >> 2) & 0x7;

      /* Use same modifier tables as for etc1 textures if opaque bit is set
       * or if non punchthrough texture format
       */
      block->modifier_tables[0] = (block->opaque || !punchthrough_alpha) ?
                                  etc1_modifier_tables[table1_idx] :
                                  etc2_modifier_tables_non_opaque[table1_idx];
      block->modifier_tables[1] = (block->opaque || !punchthrough_alpha) ?
                                  etc1_modifier_tables[table2_idx] :
                                  etc2_modifier_tables_non_opaque[table2_idx];

      block->flipped = (src[3] & 0x1);
   }

   block->pixel_indices[0] =
      (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
}

static void
etc2_rgb8_fetch_texel(const struct etc2_block *block,
                      int x, int y, uint8_t *dst,
                      GLboolean punchthrough_alpha)
{
   const uint8_t *base_color;
   int modifier, bit, idx, blk;

   /* get pixel index */
   bit = y + x * 4;
   idx = ((block->pixel_indices[0] >> (15 + bit)) & 0x2) |
         ((block->pixel_indices[0] >>      (bit)) & 0x1);

   if (block->is_ind_mode || block->is_diff_mode) {
      /* check for punchthrough_alpha format */
      if (punchthrough_alpha) {
         if (!block->opaque && idx == 2) {
            dst[0] = dst[1] = dst[2] = dst[3] = 0;
            return;
         }
         else
            dst[3] = 255;
      }

      /* Use pixel index and subblock to get the modifier */
      blk = (block->flipped) ? (y >= 2) : (x >= 2);
      base_color = block->base_colors[blk];
      modifier = block->modifier_tables[blk][idx];

      dst[0] = etc2_clamp(base_color[0] + modifier);
      dst[1] = etc2_clamp(base_color[1] + modifier);
      dst[2] = etc2_clamp(base_color[2] + modifier);
   }
   else if (block->is_t_mode || block->is_h_mode) {
      /* check for punchthrough_alpha format */
      if (punchthrough_alpha) {
         if (!block->opaque && idx == 2) {
            dst[0] = dst[1] = dst[2] = dst[3] = 0;
            return;
         }
         else
            dst[3] = 255;
      }

      /* Use pixel index to pick one of the paint colors */
      dst[0] = block->paint_colors[idx][0];
      dst[1] = block->paint_colors[idx][1];
      dst[2] = block->paint_colors[idx][2];
   }
   else if (block->is_planar_mode) {
      /* {R(x, y) = clamp255((x × (RH − RO) + y × (RV − RO) + 4 × RO + 2) >> 2)
       * {G(x, y) = clamp255((x × (GH − GO) + y × (GV − GO) + 4 × GO + 2) >> 2)
       * {B(x, y) = clamp255((x × (BH − BO) + y × (BV − BO) + 4 × BO + 2) >> 2)
       */
      int red, green, blue;
      red = (x * (block->base_colors[1][0] - block->base_colors[0][0]) +
             y * (block->base_colors[2][0] - block->base_colors[0][0]) +
             4 * block->base_colors[0][0] + 2) >> 2;

      green = (x * (block->base_colors[1][1] - block->base_colors[0][1]) +
               y * (block->base_colors[2][1] - block->base_colors[0][1]) +
               4 * block->base_colors[0][1] + 2) >> 2;

      blue = (x * (block->base_colors[1][2] - block->base_colors[0][2]) +
              y * (block->base_colors[2][2] - block->base_colors[0][2]) +
              4 * block->base_colors[0][2] + 2) >> 2;

      dst[0] = etc2_clamp(red);
      dst[1] = etc2_clamp(green);
      dst[2] = etc2_clamp(blue);

      /* check for punchthrough_alpha format */
      if (punchthrough_alpha)
         dst[3] = 255;
   }
}

static void
etc2_alpha8_fetch_texel(const struct etc2_block *block,
      int x, int y, uint8_t *dst)
{
   int modifier, alpha, idx;
   /* get pixel index */
   idx = etc2_get_pixel_index(block, x, y);
   modifier = etc2_modifier_tables[block->table_index][idx];
   alpha = block->base_codeword + modifier * block->multiplier;
   dst[3] = etc2_clamp(alpha);
}

static void
etc2_r11_fetch_texel(const struct etc2_block *block,
                     int x, int y, uint8_t *dst)
{
   GLint modifier, idx;
   GLshort color;
   /* Get pixel index */
   idx = etc2_get_pixel_index(block, x, y);
   modifier = etc2_modifier_tables[block->table_index][idx];

   if (block->multiplier != 0)
      /* clamp2(base codeword × 8 + 4 + modifier × multiplier × 8) */
      color = etc2_clamp2(((block->base_codeword << 3) | 0x4)  +
                          ((modifier * block->multiplier) << 3));
   else
      color = etc2_clamp2(((block->base_codeword << 3) | 0x4)  + modifier);

   /* Extend 11 bits color value to 16 bits. OpenGL ES 3.0 specification
    * allows extending the color value to any number of bits. But, an
    * implementation is not allowed to truncate the 11-bit value to less than
    * 11 bits."
    */
   color = (color << 5) | (color >> 6);
   ((GLushort *)dst)[0] = color;
}

static void
etc2_signed_r11_fetch_texel(const struct etc2_block *block,
                            int x, int y, uint8_t *dst)
{
   GLint modifier, idx;
   GLshort color;
   GLbyte base_codeword = (GLbyte) block->base_codeword;

   if (base_codeword == -128)
      base_codeword = -127;

   /* Get pixel index */
   idx = etc2_get_pixel_index(block, x, y);
   modifier = etc2_modifier_tables[block->table_index][idx];

   if (block->multiplier != 0)
      /* clamp3(base codeword × 8 + modifier × multiplier × 8) */
      color = etc2_clamp3((base_codeword << 3)  +
                         ((modifier * block->multiplier) << 3));
   else
      color = etc2_clamp3((base_codeword << 3)  + modifier);

   /* Extend 11 bits color value to 16 bits. OpenGL ES 3.0 specification
    * allows extending the color value to any number of bits. But, an
    * implementation is not allowed to truncate the 11-bit value to less than
    * 11 bits. A negative 11-bit value must first be made positive before bit
    * replication, and then made negative again
    */
   if (color >= 0)
      color = (color << 5) | (color >> 5);
   else {
      color = -color;
      color = (color << 5) | (color >> 5);
      color = -color;
   }
   ((GLshort *)dst)[0] = color;
}

static void
etc2_alpha8_parse_block(struct etc2_block *block, const uint8_t *src)
{
   block->base_codeword = src[0];
   block->multiplier = (src[1] >> 4) & 0xf;
   block->table_index = src[1] & 0xf;
   block->pixel_indices[1] = (((uint64_t)src[2] << 40) |
                              ((uint64_t)src[3] << 32) |
                              ((uint64_t)src[4] << 24) |
                              ((uint64_t)src[5] << 16) |
                              ((uint64_t)src[6] << 8)  |
                              ((uint64_t)src[7]));
}

static void
etc2_r11_parse_block(struct etc2_block *block, const uint8_t *src)
{
   /* Parsing logic remains same as for etc2_alpha8_parse_block */
    etc2_alpha8_parse_block(block, src);
}

static void
etc2_rgba8_parse_block(struct etc2_block *block, const uint8_t *src)
{
   /* RGB component is parsed the same way as for MESA_FORMAT_ETC2_RGB8 */
   etc2_rgb8_parse_block(block, src + 8,
                         false /* punchthrough_alpha */);
   /* Parse Alpha component */
   etc2_alpha8_parse_block(block, src);
}

static void
etc2_rgba8_fetch_texel(const struct etc2_block *block,
      int x, int y, uint8_t *dst)
{
   etc2_rgb8_fetch_texel(block, x, y, dst,
                         false /* punchthrough_alpha */);
   etc2_alpha8_fetch_texel(block, x, y, dst);
}


static void
fetch_etc1_rgb8(const GLubyte *map,
                GLint rowStride, GLint i, GLint j,
                GLfloat *texel)
{
   struct etc1_block block;
   GLubyte dst[3];
   const GLubyte *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 8;

   etc1_parse_block(&block, src);
   etc1_fetch_texel(&block, i % 4, j % 4, dst);

   texel[RCOMP] = UBYTE_TO_FLOAT(dst[0]);
   texel[GCOMP] = UBYTE_TO_FLOAT(dst[1]);
   texel[BCOMP] = UBYTE_TO_FLOAT(dst[2]);
   texel[ACOMP] = 1.0f;
}


static void
fetch_etc2_rgb8(const GLubyte *map,
                GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   uint8_t dst[3];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 8;

   etc2_rgb8_parse_block(&block, src,
                         false /* punchthrough_alpha */);
   etc2_rgb8_fetch_texel(&block, i % 4, j % 4, dst,
                         false /* punchthrough_alpha */);

   texel[RCOMP] = UBYTE_TO_FLOAT(dst[0]);
   texel[GCOMP] = UBYTE_TO_FLOAT(dst[1]);
   texel[BCOMP] = UBYTE_TO_FLOAT(dst[2]);
   texel[ACOMP] = 1.0f;
}

static void
fetch_etc2_srgb8(const GLubyte *map,
                 GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   uint8_t dst[3];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 8;

   etc2_rgb8_parse_block(&block, src,
                         false /* punchthrough_alpha */);
   etc2_rgb8_fetch_texel(&block, i % 4, j % 4, dst,
                         false /* punchthrough_alpha */);

   texel[RCOMP] = _mesa_nonlinear_to_linear(dst[0]);
   texel[GCOMP] = _mesa_nonlinear_to_linear(dst[1]);
   texel[BCOMP] = _mesa_nonlinear_to_linear(dst[2]);
   texel[ACOMP] = 1.0f;
}

static void
fetch_etc2_rgba8_eac(const GLubyte *map,
                     GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   uint8_t dst[4];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 16;

   etc2_rgba8_parse_block(&block, src);
   etc2_rgba8_fetch_texel(&block, i % 4, j % 4, dst);

   texel[RCOMP] = UBYTE_TO_FLOAT(dst[0]);
   texel[GCOMP] = UBYTE_TO_FLOAT(dst[1]);
   texel[BCOMP] = UBYTE_TO_FLOAT(dst[2]);
   texel[ACOMP] = UBYTE_TO_FLOAT(dst[3]);
}

static void
fetch_etc2_srgb8_alpha8_eac(const GLubyte *map,
                            GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   uint8_t dst[4];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 16;

   etc2_rgba8_parse_block(&block, src);
   etc2_rgba8_fetch_texel(&block, i % 4, j % 4, dst);

   texel[RCOMP] = _mesa_nonlinear_to_linear(dst[0]);
   texel[GCOMP] = _mesa_nonlinear_to_linear(dst[1]);
   texel[BCOMP] = _mesa_nonlinear_to_linear(dst[2]);
   texel[ACOMP] = UBYTE_TO_FLOAT(dst[3]);
}

static void
fetch_etc2_r11_eac(const GLubyte *map,
                   GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   GLushort dst;
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 8;

   etc2_r11_parse_block(&block, src);
   etc2_r11_fetch_texel(&block, i % 4, j % 4, (uint8_t *)&dst);

   texel[RCOMP] = USHORT_TO_FLOAT(dst);
   texel[GCOMP] = 0.0f;
   texel[BCOMP] = 0.0f;
   texel[ACOMP] = 1.0f;
}

static void
fetch_etc2_rg11_eac(const GLubyte *map,
                    GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   GLushort dst[2];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 16;

   /* red component */
   etc2_r11_parse_block(&block, src);
   etc2_r11_fetch_texel(&block, i % 4, j % 4, (uint8_t *)dst);

   /* green component */
   etc2_r11_parse_block(&block, src + 8);
   etc2_r11_fetch_texel(&block, i % 4, j % 4, (uint8_t *)(dst + 1));

   texel[RCOMP] = USHORT_TO_FLOAT(dst[0]);
   texel[GCOMP] = USHORT_TO_FLOAT(dst[1]);
   texel[BCOMP] = 0.0f;
   texel[ACOMP] = 1.0f;
}

static void
fetch_etc2_signed_r11_eac(const GLubyte *map,
                          GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   GLushort dst;
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 8;

   etc2_r11_parse_block(&block, src);
   etc2_signed_r11_fetch_texel(&block, i % 4, j % 4, (uint8_t *)&dst);

   texel[RCOMP] = SHORT_TO_FLOAT(dst);
   texel[GCOMP] = 0.0f;
   texel[BCOMP] = 0.0f;
   texel[ACOMP] = 1.0f;
}

static void
fetch_etc2_signed_rg11_eac(const GLubyte *map,
                           GLint rowStride, GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   GLushort dst[2];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 16;

   /* red component */
   etc2_r11_parse_block(&block, src);
   etc2_signed_r11_fetch_texel(&block, i % 4, j % 4, (uint8_t *)dst);

   /* green component */
   etc2_r11_parse_block(&block, src + 8);
   etc2_signed_r11_fetch_texel(&block, i % 4, j % 4, (uint8_t *)(dst + 1));

   texel[RCOMP] = SHORT_TO_FLOAT(dst[0]);
   texel[GCOMP] = SHORT_TO_FLOAT(dst[1]);
   texel[BCOMP] = 0.0f;
   texel[ACOMP] = 1.0f;
}

static void
fetch_etc2_rgb8_punchthrough_alpha1(const GLubyte *map,
                                    GLint rowStride, GLint i, GLint j,
                                    GLfloat *texel)
{
   struct etc2_block block;
   uint8_t dst[4];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 8;

   etc2_rgb8_parse_block(&block, src,
                         true /* punchthrough alpha */);
   etc2_rgb8_fetch_texel(&block, i % 4, j % 4, dst,
                         true /* punchthrough alpha */);
   texel[RCOMP] = UBYTE_TO_FLOAT(dst[0]);
   texel[GCOMP] = UBYTE_TO_FLOAT(dst[1]);
   texel[BCOMP] = UBYTE_TO_FLOAT(dst[2]);
   texel[ACOMP] = UBYTE_TO_FLOAT(dst[3]);
}

static void
fetch_etc2_srgb8_punchthrough_alpha1(const GLubyte *map,
                                     GLint rowStride,
                                     GLint i, GLint j, GLfloat *texel)
{
   struct etc2_block block;
   uint8_t dst[4];
   const uint8_t *src;

   src = map + (((rowStride + 3) / 4) * (j / 4) + (i / 4)) * 8;

   etc2_rgb8_parse_block(&block, src,
                         true /* punchthrough alpha */);
   etc2_rgb8_fetch_texel(&block, i % 4, j % 4, dst,
                         true /* punchthrough alpha */);
   texel[RCOMP] = _mesa_nonlinear_to_linear(dst[0]);
   texel[GCOMP] = _mesa_nonlinear_to_linear(dst[1]);
   texel[BCOMP] = _mesa_nonlinear_to_linear(dst[2]);
   texel[ACOMP] = UBYTE_TO_FLOAT(dst[3]);
}


// This LunarG-written function interfaces the pxfmt code with the Mesa3D code.
void decompress_etc(float *intermediate, const void *pSrc,
                    uint32 row_stride, int x, int y,
                    const pxfmt_sized_format fmt)
{
    switch (fmt)
    {
    case PXFMT_COMPRESSED_RGB8_ETC2:
        fetch_etc2_rgb8((const GLubyte *) pSrc, row_stride,
                        x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_SRGB8_ETC2:
        fetch_etc2_srgb8((const GLubyte *) pSrc, row_stride,
                         x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        fetch_etc2_rgb8_punchthrough_alpha1((const GLubyte *) pSrc, row_stride,
                                            x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        fetch_etc2_srgb8_punchthrough_alpha1((const GLubyte *) pSrc, row_stride,
                                             x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_RGBA8_ETC2_EAC:
        fetch_etc2_rgba8_eac((const GLubyte *) pSrc, row_stride,
                             x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        fetch_etc2_srgb8_alpha8_eac((const GLubyte *) pSrc, row_stride,
                                    x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_R11_EAC:
        fetch_etc2_r11_eac((const GLubyte *) pSrc, row_stride,
                           x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_RG11_EAC:
        fetch_etc2_rg11_eac((const GLubyte *) pSrc, row_stride,
                            x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_SIGNED_R11_EAC:
        fetch_etc2_signed_r11_eac((const GLubyte *) pSrc, row_stride,
                                  x, y, intermediate);
        break;
    case PXFMT_COMPRESSED_SIGNED_RG11_EAC:
        fetch_etc2_signed_rg11_eac((const GLubyte *) pSrc, row_stride,
                                   x, y, intermediate);
        break;
    default:
        // Don't try to handle the non-ETC2 cases
        break;
    }
}
