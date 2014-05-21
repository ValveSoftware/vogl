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

// File: vogl_vector.cpp
#include "vogl_core.h"
#include "vogl_vector.h"
#include "vogl_rand.h"

#include "vogl_color.h"
#include "vogl_vec.h"

namespace vogl
{
    bool elemental_vector::increase_capacity(uint32_t min_new_capacity, bool grow_hint, uint32_t element_size, object_mover pMover, bool nofail)
    {
        VOGL_ASSERT(m_size <= m_capacity);
#ifdef VOGL_64BIT_POINTERS
        VOGL_ASSERT(min_new_capacity < (0x400000000ULL / element_size));
#else
        VOGL_ASSERT(min_new_capacity < (0x7FFF0000U / element_size));
#endif

        if (m_capacity >= min_new_capacity)
            return true;

        size_t new_capacity = min_new_capacity;

        if ((grow_hint) && (!math::is_power_of_2(static_cast<uint64_t>(new_capacity))))
        {
            uint64_t new_capacity64 = math::next_pow2(static_cast<uint64_t>(new_capacity));
            if (new_capacity64 != static_cast<size_t>(new_capacity64))
                new_capacity64 = min_new_capacity;
            new_capacity = static_cast<size_t>(new_capacity64);
        }

        VOGL_ASSERT(new_capacity && (new_capacity > m_capacity));

        uint64_t desired_size64 = element_size * static_cast<uint64_t>(new_capacity);
        if ((desired_size64 != static_cast<size_t>(desired_size64)) || (desired_size64 > VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE))
        {
            if (nofail)
                return false;

            char buf[256];
            sprintf(buf, "Can't increase capacity to %" PRIu64 " items, %" PRIu64 " bytes", static_cast<uint64_t>(new_capacity), desired_size64);
            VOGL_FAIL(buf);
        }

        const size_t desired_size = static_cast<size_t>(desired_size64);

        size_t actual_size;
        if (!pMover)
        {
            void *new_p = vogl_realloc(m_p, desired_size, &actual_size);
            if (!new_p)
            {
                if (nofail)
                    return false;

                char buf[256];
                sprintf(buf, "vogl_realloc() failed allocating %u bytes", (uint32_t)desired_size);
                VOGL_FAIL(buf);
            }
            m_p = new_p;
        }
        else
        {
            void *new_p = vogl_malloc(desired_size, &actual_size);
            if (!new_p)
            {
                if (nofail)
                    return false;

                char buf[256];
                sprintf(buf, "vogl_malloc() failed allocating %u bytes", (uint32_t)desired_size);
                VOGL_FAIL(buf);
            }

            (*pMover)(new_p, m_p, m_size);

            if (m_p)
                vogl_free(m_p);

            m_p = new_p;
        }

        if (actual_size > desired_size)
            m_capacity = static_cast<uint32_t>(actual_size / element_size);
        else
            m_capacity = static_cast<uint32_t>(new_capacity);

        return true;
    }

} // namespace vogl
