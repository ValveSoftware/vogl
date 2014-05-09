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

// File: vogl_radix_sort.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    // Returns pointer to sorted array.
    template <typename T>
    T *radix_sort(uint32_t num_vals, T *pBuf0, T *pBuf1, uint32_t key_ofs, uint32_t key_size)
    {
        //VOGL_ASSERT_OPEN_RANGE(key_ofs, 0, sizeof(T));
        VOGL_ASSERT(key_ofs < sizeof(T));
        VOGL_ASSERT_CLOSED_RANGE(key_size, 1, 4);

        uint32_t hist[256 * 4];

        memset(hist, 0, sizeof(hist[0]) * 256 * key_size);

#define VOGL_GET_KEY(p) (*(uint32_t *)((uint8_t *)(p) + key_ofs))

        if (key_size == 4)
        {
            T *p = pBuf0;
            T *q = pBuf0 + num_vals;
            for (; p != q; p++)
            {
                const uint32_t key = VOGL_GET_KEY(p);

                hist[key & 0xFF]++;
                hist[256 + ((key >> 8) & 0xFF)]++;
                hist[512 + ((key >> 16) & 0xFF)]++;
                hist[768 + ((key >> 24) & 0xFF)]++;
            }
        }
        else if (key_size == 3)
        {
            T *p = pBuf0;
            T *q = pBuf0 + num_vals;
            for (; p != q; p++)
            {
                const uint32_t key = VOGL_GET_KEY(p);

                hist[key & 0xFF]++;
                hist[256 + ((key >> 8) & 0xFF)]++;
                hist[512 + ((key >> 16) & 0xFF)]++;
            }
        }
        else if (key_size == 2)
        {
            T *p = pBuf0;
            T *q = pBuf0 + (num_vals >> 1) * 2;

            for (; p != q; p += 2)
            {
                const uint32_t key0 = VOGL_GET_KEY(p);
                const uint32_t key1 = VOGL_GET_KEY(p + 1);

                hist[key0 & 0xFF]++;
                hist[256 + ((key0 >> 8) & 0xFF)]++;

                hist[key1 & 0xFF]++;
                hist[256 + ((key1 >> 8) & 0xFF)]++;
            }

            if (num_vals & 1)
            {
                const uint32_t key = VOGL_GET_KEY(p);

                hist[key & 0xFF]++;
                hist[256 + ((key >> 8) & 0xFF)]++;
            }
        }
        else
        {
            VOGL_ASSERT(key_size == 1);
            if (key_size != 1)
                return NULL;

            T *p = pBuf0;
            T *q = pBuf0 + (num_vals >> 1) * 2;

            for (; p != q; p += 2)
            {
                const uint32_t key0 = VOGL_GET_KEY(p);
                const uint32_t key1 = VOGL_GET_KEY(p + 1);

                hist[key0 & 0xFF]++;
                hist[key1 & 0xFF]++;
            }

            if (num_vals & 1)
            {
                const uint32_t key = VOGL_GET_KEY(p);
                hist[key & 0xFF]++;
            }
        }

        T *pCur = pBuf0;
        T *pNew = pBuf1;

        for (uint32_t pass = 0; pass < key_size; pass++)
        {
            const uint32_t *pHist = &hist[pass << 8];

            uint32_t offsets[256];

            uint32_t cur_ofs = 0;
            for (uint32_t i = 0; i < 256; i += 2)
            {
                offsets[i] = cur_ofs;
                cur_ofs += pHist[i];

                offsets[i + 1] = cur_ofs;
                cur_ofs += pHist[i + 1];
            }

            const uint32_t pass_shift = pass << 3;

            T *p = pCur;
            T *q = pCur + (num_vals >> 1) * 2;

            for (; p != q; p += 2)
            {
                uint32_t c0 = (VOGL_GET_KEY(p) >> pass_shift) & 0xFF;
                uint32_t c1 = (VOGL_GET_KEY(p + 1) >> pass_shift) & 0xFF;

                if (c0 == c1)
                {
                    uint32_t dst_offset0 = offsets[c0];

                    offsets[c0] = dst_offset0 + 2;

                    pNew[dst_offset0] = p[0];
                    pNew[dst_offset0 + 1] = p[1];
                }
                else
                {
                    uint32_t dst_offset0 = offsets[c0]++;
                    uint32_t dst_offset1 = offsets[c1]++;

                    pNew[dst_offset0] = p[0];
                    pNew[dst_offset1] = p[1];
                }
            }

            if (num_vals & 1)
            {
                uint32_t c = (VOGL_GET_KEY(p) >> pass_shift) & 0xFF;

                uint32_t dst_offset = offsets[c];
                offsets[c] = dst_offset + 1;

                pNew[dst_offset] = *p;
            }

            T *t = pCur;
            pCur = pNew;
            pNew = t;
        }

        return pCur;
    }

#undef VOGL_GET_KEY

    // Returns pointer to sorted array.
    template <typename T, typename Q>
    T *indirect_radix_sort(uint32_t num_indices, T *pIndices0, T *pIndices1, const Q *pKeys, uint32_t key_ofs, uint32_t key_size, bool init_indices)
    {
        VOGL_ASSERT(key_ofs < sizeof(T));
        VOGL_ASSERT_CLOSED_RANGE(key_size, 1, 4);

        if (init_indices)
        {
            T *p = pIndices0;
            T *q = pIndices0 + (num_indices >> 1) * 2;
            uint32_t i;
            for (i = 0; p != q; p += 2, i += 2)
            {
                p[0] = static_cast<T>(i);
                p[1] = static_cast<T>(i + 1);
            }

            if (num_indices & 1)
                *p = static_cast<T>(i);
        }

        uint32_t hist[256 * 4];

        memset(hist, 0, sizeof(hist[0]) * 256 * key_size);

#define VOGL_GET_KEY(p) (*(const uint32_t *)((const uint8_t *)(pKeys + *(p)) + key_ofs))
#define VOGL_GET_KEY_FROM_INDEX(i) (*(const uint32_t *)((const uint8_t *)(pKeys + (i)) + key_ofs))

        if (key_size == 4)
        {
            T *p = pIndices0;
            T *q = pIndices0 + num_indices;
            for (; p != q; p++)
            {
                const uint32_t key = VOGL_GET_KEY(p);

                hist[key & 0xFF]++;
                hist[256 + ((key >> 8) & 0xFF)]++;
                hist[512 + ((key >> 16) & 0xFF)]++;
                hist[768 + ((key >> 24) & 0xFF)]++;
            }
        }
        else if (key_size == 3)
        {
            T *p = pIndices0;
            T *q = pIndices0 + num_indices;
            for (; p != q; p++)
            {
                const uint32_t key = VOGL_GET_KEY(p);

                hist[key & 0xFF]++;
                hist[256 + ((key >> 8) & 0xFF)]++;
                hist[512 + ((key >> 16) & 0xFF)]++;
            }
        }
        else if (key_size == 2)
        {
            T *p = pIndices0;
            T *q = pIndices0 + (num_indices >> 1) * 2;

            for (; p != q; p += 2)
            {
                const uint32_t key0 = VOGL_GET_KEY(p);
                const uint32_t key1 = VOGL_GET_KEY(p + 1);

                hist[key0 & 0xFF]++;
                hist[256 + ((key0 >> 8) & 0xFF)]++;

                hist[key1 & 0xFF]++;
                hist[256 + ((key1 >> 8) & 0xFF)]++;
            }

            if (num_indices & 1)
            {
                const uint32_t key = VOGL_GET_KEY(p);

                hist[key & 0xFF]++;
                hist[256 + ((key >> 8) & 0xFF)]++;
            }
        }
        else
        {
            VOGL_ASSERT(key_size == 1);
            if (key_size != 1)
                return NULL;

            T *p = pIndices0;
            T *q = pIndices0 + (num_indices >> 1) * 2;

            for (; p != q; p += 2)
            {
                const uint32_t key0 = VOGL_GET_KEY(p);
                const uint32_t key1 = VOGL_GET_KEY(p + 1);

                hist[key0 & 0xFF]++;
                hist[key1 & 0xFF]++;
            }

            if (num_indices & 1)
            {
                const uint32_t key = VOGL_GET_KEY(p);

                hist[key & 0xFF]++;
            }
        }

        T *pCur = pIndices0;
        T *pNew = pIndices1;

        for (uint32_t pass = 0; pass < key_size; pass++)
        {
            const uint32_t *pHist = &hist[pass << 8];

            uint32_t offsets[256];

            uint32_t cur_ofs = 0;
            for (uint32_t i = 0; i < 256; i += 2)
            {
                offsets[i] = cur_ofs;
                cur_ofs += pHist[i];

                offsets[i + 1] = cur_ofs;
                cur_ofs += pHist[i + 1];
            }

            const uint32_t pass_shift = pass << 3;

            T *p = pCur;
            T *q = pCur + (num_indices >> 1) * 2;

            for (; p != q; p += 2)
            {
                uint32_t index0 = p[0];
                uint32_t index1 = p[1];

                uint32_t c0 = (VOGL_GET_KEY_FROM_INDEX(index0) >> pass_shift) & 0xFF;
                uint32_t c1 = (VOGL_GET_KEY_FROM_INDEX(index1) >> pass_shift) & 0xFF;

                if (c0 == c1)
                {
                    uint32_t dst_offset0 = offsets[c0];

                    offsets[c0] = dst_offset0 + 2;

                    pNew[dst_offset0] = static_cast<T>(index0);
                    pNew[dst_offset0 + 1] = static_cast<T>(index1);
                }
                else
                {
                    uint32_t dst_offset0 = offsets[c0]++;
                    uint32_t dst_offset1 = offsets[c1]++;

                    pNew[dst_offset0] = static_cast<T>(index0);
                    pNew[dst_offset1] = static_cast<T>(index1);
                }
            }

            if (num_indices & 1)
            {
                uint32_t index = *p;
                uint32_t c = (VOGL_GET_KEY_FROM_INDEX(index) >> pass_shift) & 0xFF;

                uint32_t dst_offset = offsets[c];
                offsets[c] = dst_offset + 1;

                pNew[dst_offset] = static_cast<T>(index);
            }

            T *t = pCur;
            pCur = pNew;
            pNew = t;
        }

        return pCur;
    }

#undef VOGL_GET_KEY
#undef VOGL_GET_KEY_FROM_INDEX

} // namespace vogl
