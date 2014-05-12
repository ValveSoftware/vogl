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

// File: vogl_utils.cpp
#include "vogl_core.h"
#include "vogl_utils.h"
#include "vogl_file_utils.h"

namespace vogl
{
    namespace utils
    {
        const int64_t s_signed_mins[9] = { 0, cINT8_MIN, cINT16_MIN, static_cast<int64_t>(0xFFFFFFFFFF800000), cINT32_MIN, static_cast<int64_t>(0xFFFFFF8000000000), static_cast<int64_t>(0xFFFF800000000000), static_cast<int64_t>(0xFF80000000000000), cINT64_MIN };
        const int64_t s_signed_maxs[9] = { 0, cINT8_MAX, cINT16_MAX, 0x7FFFFFLL, cINT32_MAX, 0x7FFFFFFFFFLL, 0x7FFFFFFFFFFFLL, 0x7FFFFFFFFFFFFFLL, cINT64_MAX };
        const uint64_t s_unsigned_maxes[9] = { 0, cUINT8_MAX, cUINT16_MAX, 0xFFFFFFULL, cUINT32_MAX, 0xFFFFFFFFFFULL, 0xFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFULL, cUINT64_MAX };

#if defined(VOGL_USE_LINUX_API)
        int g_reliable_rdtsc = -1;
#endif
        bool init_rdtsc()
        {
#if defined(VOGL_USE_LINUX_API)
            if (g_reliable_rdtsc == -1)
            {
                g_reliable_rdtsc = 0;

                FILE *file = fopen("/sys/devices/system/clocksource/clocksource0/current_clocksource", "r");
                if (file)
                {
                    char buf[64];

                    if (fgets(buf, sizeof(buf), file))
                    {
                        if (buf[0] == 't' && buf[1] == 's' && buf[2] == 'c')
                            g_reliable_rdtsc = 1;
                    }

                    fclose(file);
                }
            }

            // return true for reliable rdtsc.
            return !!g_reliable_rdtsc;
#else
            return true;
#endif
        }

        void endian_switch_words(uint16_t *p, uint32_t num)
        {
            uint16_t *p_end = p + num;
            while (p != p_end)
            {
                uint16_t k = *p;
                *p++ = swap16(k);
            }
        }

        void endian_switch_dwords(uint32_t *p, uint32_t num)
        {
            uint32_t *p_end = p + num;
            while (p != p_end)
            {
                uint32_t k = *p;
                *p++ = swap32(k);
            }
        }

        void copy_words(uint16_t *pDst, const uint16_t *pSrc, uint32_t num, bool endian_switch)
        {
            if (!endian_switch)
                memcpy(pDst, pSrc, num << 1U);
            else
            {
                uint16_t *pDst_end = pDst + num;
                while (pDst != pDst_end)
                    *pDst++ = swap16(*pSrc++);
            }
        }

        void copy_dwords(uint32_t *pDst, const uint32_t *pSrc, uint32_t num, bool endian_switch)
        {
            if (!endian_switch)
                memcpy(pDst, pSrc, num << 2U);
            else
            {
                uint32_t *pDst_end = pDst + num;
                while (pDst != pDst_end)
                    *pDst++ = swap32(*pSrc++);
            }
        }

        uint32_t compute_max_mips(uint32_t width, uint32_t height, uint32_t min_width, uint32_t min_height)
        {
            if ((width | height) == 0)
                return 0;

            uint32_t total_mips = 1;

            while ((width > min_width) || (height > min_height))
            {
                width >>= 1U;
                height >>= 1U;
                total_mips++;
            }

            return total_mips;
        }

        uint32_t compute_max_mips3D(uint32_t width, uint32_t height, uint32_t depth, uint32_t min_width, uint32_t min_height, uint32_t min_depth)
        {
            if ((width | height | depth) == 0)
                return 0;

            uint32_t total_mips = 1;

            while ((width > min_width) || (height > min_height) || (depth > min_depth))
            {
                width >>= 1U;
                height >>= 1U;
                depth >>= 1U;
                total_mips++;
            }

            return total_mips;
        }

        bool is_buffer_printable(const void *pBuf, uint32_t buf_size, bool allow_crlf, bool expect_null_terminator)
        {
            if (!pBuf)
                return false;
            if (!buf_size)
                return true;

            uint32_t scan_size = expect_null_terminator ? (buf_size - 1) : buf_size;

            for (uint32_t i = 0; i < scan_size; i++)
            {
                uint8_t c = reinterpret_cast<const uint8_t *>(pBuf)[i];
                if (allow_crlf)
                {
                    if ((c == 0x0A) || (c == 0x0D))
                        continue;
                }

                if ((c < 32) || (c > 127))
                    return false;
            }

            if (expect_null_terminator)
            {
                if (reinterpret_cast<const uint8_t *>(pBuf)[buf_size - 1] != 0)
                    return false;
            }

            return true;
        }

    } // namespace utils

} // namespace vogl
