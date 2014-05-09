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

// File: vogl_hash_map.cpp
#include "vogl_core.h"
#include "vogl_hash_map.h"
#include "vogl_rand.h"

namespace vogl
{
    class counted_obj
    {
    public:
        counted_obj(uint32_t v = 0)
            : m_val(v)
        {
            m_count++;
        }

        counted_obj(const counted_obj &obj)
            : m_val(obj.m_val)
        {
            m_count++;
        }

        ~counted_obj()
        {
            VOGL_ASSERT(m_count > 0);
            m_count--;
        }

        static uint32_t m_count;

        uint32_t m_val;

        operator size_t() const
        {
            return m_val;
        }

        bool operator==(const counted_obj &rhs) const
        {
            return m_val == rhs.m_val;
        }
        bool operator==(const uint32_t rhs) const
        {
            return m_val == rhs;
        }
    };

    uint32_t counted_obj::m_count;

#define VOGL_HASHMAP_VERIFY(x) \
    if (!(x))                    \
        return false;

    bool hash_map_test()
    {
        random r0, r1;

        uint32_t seed = 0;
        for (uint32_t t = 0; t < 2000; t++)
        {
            seed++;

            typedef vogl::hash_map<counted_obj, counted_obj> my_hash_map;
            my_hash_map m;

            const uint32_t n = r0.irand(1, 100000);

            printf("%u\n", n);

            r1.seed(seed);

            vogl::vector<int> q;

            uint32_t count = 0;
            for (uint32_t i = 0; i < n; i++)
            {
                uint32_t v = r1.urand32() & 0x7FFFFFFF;
                my_hash_map::insert_result res = m.insert(counted_obj(v), counted_obj(v ^ 0xdeadbeef));
                if (res.second)
                {
                    count++;
                    q.push_back(v);
                }
            }

            VOGL_HASHMAP_VERIFY(m.size() == count);

            r1.seed(seed);

            my_hash_map cm(m);
            m.clear();
            m = cm;
            cm.reset();

            for (uint32_t i = 0; i < n; i++)
            {
                uint32_t v = r1.urand32() & 0x7FFFFFFF;
                my_hash_map::const_iterator it = m.find(counted_obj(v));
                VOGL_HASHMAP_VERIFY(it != m.end());
                VOGL_HASHMAP_VERIFY(it->first == v);
                VOGL_HASHMAP_VERIFY(it->second == (v ^ 0xdeadbeef));
            }

            for (uint32_t t2 = 0; t2 < 2; t2++)
            {
                const uint32_t nd = r0.irand(1, q.size() + 1);
                for (uint32_t i = 0; i < nd; i++)
                {
                    uint32_t p = r0.irand(0, q.size());

                    int k = q[p];
                    if (k >= 0)
                    {
                        q[p] = -k - 1;

                        bool s = m.erase(counted_obj(k));
                        VOGL_HASHMAP_VERIFY(s);
                    }
                }

                typedef vogl::hash_map<uint32_t, empty_type> uint_hash_set;
                uint_hash_set s;

                for (uint32_t i = 0; i < q.size(); i++)
                {
                    int v = q[i];

                    if (v >= 0)
                    {
                        my_hash_map::const_iterator it = m.find(counted_obj(v));
                        VOGL_HASHMAP_VERIFY(it != m.end());
                        VOGL_HASHMAP_VERIFY(it->first == (uint32_t)v);
                        VOGL_HASHMAP_VERIFY(it->second == ((uint32_t)v ^ 0xdeadbeef));

                        s.insert(v);
                    }
                    else
                    {
                        my_hash_map::const_iterator it = m.find(counted_obj(-v - 1));
                        VOGL_HASHMAP_VERIFY(it == m.end());
                    }
                }

                uint32_t found_count = 0;
                for (my_hash_map::const_iterator it = m.begin(); it != m.end(); ++it)
                {
                    VOGL_HASHMAP_VERIFY(it->second == ((uint32_t)it->first ^ 0xdeadbeef));

                    uint_hash_set::const_iterator fit(s.find((uint32_t)it->first));
                    VOGL_HASHMAP_VERIFY(fit != s.end());

                    VOGL_HASHMAP_VERIFY(fit->first == it->first);

                    found_count++;
                }

                VOGL_HASHMAP_VERIFY(found_count == s.size());
            }

            VOGL_HASHMAP_VERIFY(counted_obj::m_count == m.size() * 2);
        }

        return true;
    }

} // namespace vogl
