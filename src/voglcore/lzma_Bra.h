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

/* Bra.h -- Branch converters for executables
2008-10-04 : Igor Pavlov : Public domain */

#pragma once

#include "lzma_Types.h"

namespace vogl
{

/*
These functions convert relative addresses to absolute addresses
in CALL instructions to increase the compression ratio.

  In:
    data     - data buffer
    size     - size of data
    ip       - current virtual Instruction Pinter (IP) value
    state    - state variable for x86 converter
    encoding - 0 (for decoding), 1 (for encoding)

  Out:
    state    - state variable for x86 converter

  Returns:
    The number of processed bytes. If you call these functions with multiple calls,
    you must start next call with first byte after block of processed bytes.

  Type   Endian  Alignment  LookAhead

  x86    little      1          4
  ARMT   little      2          2
  ARM    little      4          0
  PPC     big        4          0
  SPARC   big        4          0
  IA64   little     16          0

  size must be >= Alignment + LookAhead, if it's not last block.
  If (size < Alignment + LookAhead), converter returns 0.

  Example:

    UInt32 ip = 0;
    for ()
    {
      ; size must be >= Alignment + LookAhead, if it's not last block
      SizeT processed = Convert(data, size, ip, 1);
      data += processed;
      size -= processed;
      ip += processed;
    }
*/

#define x86_Convert_Init(state) \
    {                           \
        state = 0;              \
    }
    SizeT x86_Convert(Byte *data, SizeT size, UInt32 ip, UInt32 *state, int encoding);
    SizeT ARM_Convert(Byte *data, SizeT size, UInt32 ip, int encoding);
    SizeT ARMT_Convert(Byte *data, SizeT size, UInt32 ip, int encoding);
    SizeT PPC_Convert(Byte *data, SizeT size, UInt32 ip, int encoding);
    SizeT SPARC_Convert(Byte *data, SizeT size, UInt32 ip, int encoding);
    SizeT IA64_Convert(Byte *data, SizeT size, UInt32 ip, int encoding);
}

