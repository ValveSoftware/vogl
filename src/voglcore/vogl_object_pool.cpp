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

// File: vogl_object_pool.cpp
#include "vogl_object_pool.h"

namespace vogl
{

#define CHECK(x) \
    if (!(x))    \
        return false;

bool object_pool_test()
{
    object_pool<uint32_t, object_pool_spinlock_locking_policy> pool;

    bool s = pool.check();
    CHECK(s);

    uint32_t *q = pool.alloc();

    s = pool.check();
    CHECK(s);

    pool.destroy(q);

    s = pool.check();
    CHECK(s);

    pool.clear();
    CHECK(s);

    vogl::random rm;

    vogl::vector<uint32_t *> ptrs;
    uint32_t z = 0;
    for (uint32_t t = 0; t < 10000; t++, z++)
//    for (uint32_t t = 0; t < 1000000; t++, z++)
    {
        printf("%u %" PRIu64 " %" PRIu64 "\n", t, (uint64_t)pool.get_total_blocks(), (uint64_t)pool.get_total_heap_bytes());

        uint32_t n = rm.irand(0, 1000);
        for (uint32_t i = 0; i < n; i++)
        {
            ptrs.push_back(pool.alloc());

            s = pool.is_valid_ptr(ptrs.back(), true);
            CHECK(s);

            *ptrs.back() = ptrs.size() - 1;
        }

        s = pool.check();
        CHECK(s);

        object_pool<uint32_t, object_pool_spinlock_locking_policy> other_pool;

        other_pool.swap(pool);
        s = other_pool.check();
        CHECK(s);
        s = pool.check();
        CHECK(s);

        pool.swap(other_pool);
        s = other_pool.check();
        CHECK(s);
        s = pool.check();
        CHECK(s);

        pool.swap(other_pool);
        s = other_pool.check();
        CHECK(s);
        s = pool.check();
        CHECK(s);

        pool.swap(other_pool);
        s = other_pool.check();
        CHECK(s);
        s = pool.check();
        CHECK(s);

        uint32_t d = rm.irand(0, 1100); //(z < 400) ? 100 : 1200);
        for (uint32_t i = 0; i < d; i++)
        {
            if (ptrs.is_empty())
                break;

            uint32_t k = rm.irand(0, ptrs.size());

            uint32_t *p = ptrs[k];
            VOGL_ASSERT(*p == k);

            s = pool.is_valid_ptr(p);
            CHECK(s);

            pool.destroy(p);

            s = pool.is_valid_ptr(p, false);
            CHECK(s);

            if (k != ptrs.size() - 1)
            {
                ptrs[k] = ptrs.back();
                *ptrs[k] = k;
            }

            ptrs.pop_back();
        }

        s = pool.check();
        CHECK(s);

        if (!rm.irand(0, 4000))
        {
            for (uint32_t i = 0; i < ptrs.size(); i++)
                pool.destroy(ptrs[i]);

            ptrs.clear();
            z = 0;
        }

        size_t l = pool.free_unused_blocks();
        printf("Freed %" PRIu64 " bytes of unused blocks\n", (uint64_t)l);

        s = pool.check();
        CHECK(s);

        if (!rm.irand(0, 4000))
        {
            printf("reset 0\n");

            for (uint32_t i = 0; i < ptrs.size(); i++)
                pool.destroy(ptrs[i]);

            ptrs.clear();
            z = 0;
            pool.clear();

            s = pool.check();
            CHECK(s);
        }
        if (!rm.irand(0, 4000))
        {
            printf("reset 1\n");

            ptrs.clear();
            z = 0;
            pool.clear();

            s = pool.check();
            CHECK(s);
        }
    }

    return true;
}

} // namespace vogl
