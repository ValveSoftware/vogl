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

// File: vogl_map.cpp
#include "vogl_core.h"
#include "vogl_map.h"

#define VOGL_MAP_COMPARATIVE_PERF_TESTING

#ifdef VOGL_MAP_COMPARATIVE_PERF_TESTING
#include <map>
#include "vogl_hash_map.h"
//#include "vogl_treap.h"
//#include "vogl_avltree.h"
#endif

namespace vogl
{
    bool map_test()
    {
        typedef vogl::map<int, vogl::vector<dynamic_string> > int_to_string_vec_map;
        int_to_string_vec_map blah;
        if (blah.contains(0))
            return false;
        for (uint32_t i = 0; i < 100; i++)
        {
            vogl::vector<dynamic_string> v;
            v.push_back(dynamic_string(cVarArg, "hello %u", i));
            v.push_back(dynamic_string(cVarArg, "blah %u", i));
            blah.insert(i, v);
        }

        if (blah.contains(cINT32_MAX))
            return false;

        for (int_to_string_vec_map::const_iterator it = blah.begin(); it != blah.end(); ++it)
        {
            if (!blah.contains(it->first))
                return false;

            printf("%u:\n", it->first);
            for (uint32_t i = 0; i < it->second.size(); i++)
            {
                printf("%s\n", it->second[i].get_ptr());
            }
        }

        int_to_string_vec_map blah2(blah);
        if (blah2 != blah)
            return false;
        blah2 = blah;
        if (!blah2.debug_check())
            return false;
        if (blah2 != blah)
            return false;

        if (!blah.debug_check())
            return false;
        blah.print_debug_info();
        blah.reset();
        if (!blah.debug_check())
            return false;
        blah.print_debug_info();

        fast_random frm;
        frm.seed(5556);

        typedef vogl::map<int> int_map;

        int_map z;

        for (uint32_t i = 0; i < 10000; i++)
            z.insert_multi(frm.irand(0, 5000));

        z.reset();

        for (uint32_t i = 0; i < 10000; i++)
            z.insert_multi(frm.irand(0, 5000));

        int_map z1;

        z1.unite_multi(z);
        if (z1 != z)
            return false;

        int_map z2;
        z1.swap(z2);
        if (z1 == z)
            return false;
        if (z2 != z)
            return false;

        if (!z2.debug_check())
            return false;
        while (z2.size())
        {
            int x = z2.begin()->first;

            z2.erase_all(z2.begin()->first);

            if (z2.size())
            {
                if (z2.begin()->first == x)
                    return false;
            }

            if (!z2.debug_check())
                return false;
        }
        if (z2.size())
            return false;

        for (uint32_t i = 0; i < 1000; i++)
        {
            int l = frm.irand(0, 5000);
            int h = frm.irand(0, 5000);
            if (l > h)
                std::swap(l, h);

            printf("%i %i\n", l, h);

            std::pair<int_map::iterator, int_map::iterator> p(z.equal_range(l));
            uint32_t cnt = z.count(l);
            if (!cnt)
            {
                if ((p.first != z.end()) || (p.second != z.end()))
                    return false;
            }
            else
            {
                uint32_t u = 0;
                int_map::const_iterator it(p.first);
                while (it != p.second)
                {
                    u++;
                    ++it;
                }
                if (u != cnt)
                    return false;
            }

            vogl::vector<int> uk;
            z.get_unique_keys(uk);

            int_map tst;
            for (uint32_t i2 = 0; i2 < uk.size(); i2++)
            {
                if (!tst.insert(uk[i2]).second)
                    return false;
            }
            if (!tst.debug_check())
                return false;

            int_map::iterator nl_it(z.lower_bound(l));
            VOGL_NOTE_UNUSED(nl_it);
            int_map::iterator nh_it(z.upper_bound(h));
            VOGL_NOTE_UNUSED(nh_it);

            int_map::const_iterator l_it(z.lower_bound(l));
            int_map::const_iterator h_it(z.upper_bound(h));

            uint32_t n = 0;
            for (int_map::const_iterator it = l_it; it != h_it; it++)
            {
                if ((it->first < l) || (it->first > h))
                    return false;
                n++;
            }

            uint32_t cn = 0;
            for (int_map::const_iterator it = z.begin(); it != z.end(); ++it)
                if ((it->first >= l) && (it->first <= h))
                    cn++;

            if (n != cn)
                return false;
        }

        int_map set_test;

        for (uint32_t i = 0; i < 1000; i++)
            set_test.insert_multi(frm.irand(0, 100));

        for (int_map::const_iterator it = set_test.begin(); it != set_test.end(); it++)
            printf("%i ", it->first);
        printf("\n");

        if (!set_test.debug_check())
            return false;
        int_map set_test2(set_test);

        if (set_test2 != set_test)
            return false;

        if (!set_test2.debug_check())
            return false;

        for (int_map::const_iterator it = set_test2.begin(); it != set_test2.end(); it++)
            printf("%i ", it->first);
        printf("\n");

        for (int_map::iterator it = set_test2.begin(); it != set_test2.end();)
        {
            if (frm.get_bit())
            {
                int_map::iterator next_it = it;
                ++next_it;

                set_test2.erase(it);

                it = next_it;
            }
            else
                ++it;
        }

        for (int_map::const_iterator it = set_test2.begin(); it != set_test2.end(); it++)
            printf("%i ", it->first);
        printf("\n");

        if (!set_test2.debug_check())
            return false;
        set_test2.print_debug_info();

        typedef vogl::map<int, int> int_to_int_map;

        {
            int_to_int_map blah3(10);

            int_to_int_map::const_iterator it0;
            int_to_int_map::const_iterator it1(blah3.begin());
            int_to_int_map::iterator it2(blah3.begin());

            it1 = it2;
            //it2 = it1;

            bool ignored = it1 != it1;
            VOGL_NOTE_UNUSED(ignored);
            ignored = it2 != it2;
            ignored = it1 == it1;
            ignored = it2 == it2;

            ignored = it1 != it2;
            ignored = it2 != it1;

            int_to_int_map::const_iterator it3(it1);
            VOGL_NOTE_UNUSED(it3);
            int_to_int_map::const_iterator it4(it2);
            VOGL_NOTE_UNUSED(it4);

            blah3.insert_multi(0, 100);
            blah3.insert_multi(0, 101);
            blah3.insert_multi(1, 200);

            if (blah3.count(0) != 2)
                return false;

            vogl::vector<int> iv;
            blah3.get_values(0, iv);
            if (iv.size() != 2)
                return false;
            if ((iv[0] != 101) || (iv[1] != 100))
                return false;

            printf("-----\n");
            for (int_to_int_map::const_iterator it = blah3.begin(); it != blah3.end(); ++it)
                printf("%i %i\n", it->first, it->second);
            printf("-----\n");

            printf("-----\n");
            for (int_to_int_map::iterator it = blah3.begin(); it != blah3.end(); ++it)
                printf("%i %i\n", it->first, it->second);
            printf("-----\n");

            bool status = blah3.debug_check();
            if (!status)
                return false;

            //blah3.erase(0);
            blah3.erase(blah3.begin());
            blah3.erase(1);

            printf("-----\n");
            for (int_to_int_map::const_iterator it = blah3.begin(); it != blah3.end(); ++it)
                printf("%i %i\n", it->first, it->second);
            printf("-----\n");

            status = blah3.debug_check();
            if (!status)
                return false;

            int_to_int_map blah4(blah3);

            status = blah4.debug_check();
            if (!status)
                return false;

            blah4.clear();
            status = blah4.debug_check();
            if (!status)
                return false;

            blah4 = blah4;
            status = blah4.debug_check();
            if (!status)
                return false;

            blah4.print_debug_info();
        }

        {
            typedef map<dynamic_string, dynamic_string> string_to_string_map;
            string_to_string_map s2s(5);
            s2s.insert_multi("Hello", "World");
            s2s.insert_multi("Hello", "2");
            s2s.insert_multi("Hello", "3");
            s2s.insert_multi("Hello", "4");
            s2s.insert_multi("Blah", "x");

            for (string_to_string_map::const_iterator it = s2s.begin(); it != s2s.end(); it++)
            {
                printf("%s %s\n", it->first.get_ptr(), it->second.get_ptr());
            }

            printf("\n");

            if (!s2s.erase("Hello"))
                return false;
            if (!s2s.erase("Blah"))
                return false;
            if (s2s.erase("ssss"))
                return false;

            for (string_to_string_map::const_iterator it = s2s.end(); it != s2s.begin();)
            {
                it--;
                printf("%s %s\n", it->first.get_ptr(), it->second.get_ptr());
            }

            if (!s2s.debug_check())
                return false;

            s2s.print_debug_info();

            s2s.clear();

            if (!s2s.debug_check())
                return false;
        }

        {
            typedef map<dynamic_string, vogl::vector<uint32_t> > string_to_vec_map;
            vogl::vector<uint32_t> k;
            k.push_back(1);
            k.push_back(2);
            k.push_back(3);
            string_to_vec_map s2v(0);
            s2v.set_fixed_map_level(0);
            s2v.insert_multi("Hello", k);

            k.push_back(111);
            k.push_back(333);
            s2v.insert_multi("X", k);

            k.push_back(511);
            k.push_back(733);
            s2v.insert_multi("Y", k);

            for (string_to_vec_map::const_iterator it = s2v.begin(); it != s2v.end(); it++)
            {
                printf("%s %u\n", it->first.get_ptr(), it->second.size());
            }

            if (!s2v.erase("X"))
                return false;

            for (string_to_vec_map::const_iterator it = s2v.end(); it != s2v.begin();)
            {
                it--;
                printf("%s %u\n", it->first.get_ptr(), it->second.size());
            }

            if (!s2v.debug_check())
                return false;

            s2v.print_debug_info();

            s2v.reset(5);
            s2v.insert_multi("Hello", k);
            for (string_to_vec_map::const_iterator it = s2v.begin(); it != s2v.end(); it++)
            {
                printf("%s %u\n", it->first.get_ptr(), it->second.size());
            }

            if (!s2v.debug_check())
                return false;
        }

        vogl::random rm;
        rm.seed(51553);

        for (uint32_t t = 0; t < 100; t++)
        {
            uint32_t n = rm.irand(1, 50000);
            uint32_t k = rm.irand(1, n);

            int_to_int_map m;
            if (rm.get_bit())
            {
                uint32_t max_level = (n < 512) ? rm.irand(0, 5) : rm.irand(5, 20);
                m.set_fixed_map_level(max_level);
                printf("Using fixed max level of %u\n", max_level);
            }
            else
                printf("Auto max level\n");

            printf("n=%u\n", n);

            vogl::vector<int> keys(n);
            for (uint32_t i = 0; i < n; i++)
            {
                uint32_t c = rm.irand_inclusive(0, k);
                keys[i] = c;
                m.insert_multi(c, i);
            }
            printf("max level: %u, max used level: %u, bytes allocated: %" PRIi64 ", avg per node: %f\n",
                   m.get_cur_max_level(), m.get_cur_list_level(), m.get_total_bytes_allocated(), m.get_total_bytes_allocated() / (double)n);

            bool success = m.debug_check();
            if (!success)
                return false;

            m.print_debug_info();

            for (int i = 0; i < static_cast<int>(n); i++)
            {
                const int c = keys[i];
                int_to_int_map::const_iterator it = m.find(c);

                VOGL_VERIFY(it != m.end());
                VOGL_VERIFY(it->first == keys[i]);
                if (it->second != i)
                {
                    // duplicate, the iterator should always point at the beginning of the dup key run
                    int_to_int_map::const_iterator xit(it);

                    if (xit != m.begin())
                    {
                        --xit;

                        if (xit->first == c)
                        {
                            VOGL_VERIFY(0);
                        }

                        VOGL_VERIFY(xit->first < c);
                    }

                    xit = it;

                    for (;;)
                    {
                        ++xit;
                        if (xit == m.end())
                        {
                            VOGL_VERIFY(0);
                            break;
                        }
                        if (xit->first != c)
                        {
                            VOGL_VERIFY(0);
                            break;
                        }
                        if (xit->second == i)
                            break;
                    }
                }
            }

            success = m.debug_check();
            if (!success)
                return false;

            for (uint32_t j = 0; j < keys.size() / 2; j++)
            {
                if (j == keys.size() / 4)
                {
                    success = m.debug_check();
                    if (!success)
                        return false;
                }

                int i = rm.irand(0, keys.size());

                int c = keys[i];
                if (c < 0)
                    continue;

                int_to_int_map::iterator it = m.find(c);

                VOGL_VERIFY(it != m.end());
                VOGL_VERIFY(it->first == c);
                if (it->second != i)
                {
                    // duplicate, the iterator should always point at the beginning of the dup key run
                    int_to_int_map::iterator xit(it);

                    if (xit != m.begin())
                    {
                        --xit;

                        if (xit->first == c)
                        {
                            VOGL_VERIFY(0);
                        }

                        VOGL_VERIFY(xit->first < c);
                    }

                    xit = it;

                    for (;;)
                    {
                        ++xit;
                        if (xit == m.end())
                        {
                            VOGL_VERIFY(0);
                            break;
                        }
                        if (xit->first != c)
                        {
                            VOGL_VERIFY(0);
                            break;
                        }
                        if (xit->second == i)
                            break;
                    }

                    it = xit;
                }

                m.erase(it);

                keys[i] = -1;
            }

            success = m.debug_check();
            if (!success)
                return false;

            while (!m.is_empty())
            {
                if (rm.get_bit())
                    m.erase(m.begin());
                else
                    m.erase(m.begin()->first);
            }

            success = m.debug_check();
            if (!success)
                return false;

            m.print_debug_info();

            printf("------------------\n");
        }

        return true;
    }

#define VOGL_MAP_INSERT_ONLY_SPEED_TEST 0

    void map_perf_test(uint32_t Q)
    {
        printf("--------------------------------------------------\n");
        printf("map_perf_test int->int Q: %u\n", Q);

        vogl::vector<uint32_t> rand_indices(Q);
        for (uint32_t i = 0; i < Q; i++)
            rand_indices[i] = i;

        random rm;
        rand_indices.shuffle(rm);

        for (uint32_t i = 0; i < 2; i++)
        {
            printf("---------------------------- Run %u\n", i);

#ifdef VOGL_MAP_COMPARATIVE_PERF_TESTING
            typedef std::map<int, int> int_to_int_map;

            {
                printf("forwards std::map:\n");
                timed_scope ts;

                int_to_int_map m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(std::make_pair(t, t * 2));

                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(t);
                    m.erase(t);
                }
            }

            {
                printf("backwards std::map:\n");
                timed_scope ts;

                int_to_int_map m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(std::make_pair(Q - 1 - t, t * 2));

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(Q - 1 - t);
                    m.erase(Q - 1 - t);
                }
#endif
            }

            {
                printf("random std::map:\n");
                timed_scope ts;

                int_to_int_map m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(std::make_pair(rand_indices[t], t * 2));

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(rand_indices[t]);
                    m.erase(rand_indices[t]);
                }
#endif
            }

            printf("-----------------------\n");
            {
                printf("vogl::hash_map:\n");
                timed_scope ts;

                typedef vogl::hash_map<int, int> int_to_int_hash_map;
                int_to_int_hash_map m;

                for (uint32_t t = 0; t < Q; t++)
                    m.insert(t, t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(t);
                    m.erase(t);
                }
#endif
            }

            {
                printf("vogl::hash_map reserved:\n");
                timed_scope ts;

                typedef vogl::hash_map<int, int> int_to_int_hash_map;
                int_to_int_hash_map m;

                m.reserve(Q);

                for (uint32_t t = 0; t < Q; t++)
                    m.insert(t, t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(t);
                    m.erase(t);
                }
#endif
            }

#if 0
            printf("-----------------------\n");

            typedef Treap<int, int> int_to_int_treap;

            {
                printf("forwards Treap:\n");
                timed_scope ts;

                int_to_int_treap m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(t, t * 2);

                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(t);
                    m.erase(t);
                }
            }

            {
                printf("backwards Treap:\n");
                timed_scope ts;

                int_to_int_treap m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(Q - 1 - t, t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(Q - 1 - t);
                    m.erase(Q - 1 - t);
                }
#endif
            }

            {
                printf("random Treap:\n");
                timed_scope ts;

                int_to_int_treap m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(rand_indices[t], t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(rand_indices[t]);
                    m.erase(rand_indices[t]);
                }
#endif
            }
#endif

#if 0
            printf("-----------------------\n");

            typedef AVLTree<int, int> int_to_int_avl_tree;

            {
                printf("forwards AVLTree:\n");
                timed_scope ts;

                int_to_int_avl_tree m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(t, t * 2);

                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(t);
                    m.erase(t);
                }
            }

            {
                printf("backwards AVLTree:\n");
                timed_scope ts;

                int_to_int_avl_tree m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(Q - 1 - t, t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(Q - 1 - t);
                    m.erase(Q - 1 - t);
                }
#endif
            }

            {
                printf("random AVLTree:\n");
                timed_scope ts;

                int_to_int_avl_tree m;
                for (uint32_t t = 0; t < Q; t++)
                    m.insert(rand_indices[t], t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(rand_indices[t]);
                    m.erase(rand_indices[t]);
                }
#endif
            }
#endif

#endif

            // Skip testing fixed max levels 0-4, they are just too slow
            const int skip_level = 5;

            printf("-----------------------\n");
            for (int l = -1; l <= 11; l++)
            {
                if ((l >= 0) && (l < skip_level))
                    continue;

                printf("forwards vogl::map max_level: %i\n", l);

                timed_scope ts;

                typedef vogl::map<int, int> int_to_int_map;
                int_to_int_map m((l < 0) ? int_to_int_map::cDefaultMaxLevel : l);
                if (l >= 0)
                    m.set_fixed_map_level(l);

                for (uint32_t t = 0; t < Q; t++)
                    m.insert(t, t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(t);
                    m.erase(t);
                }
#endif
            }

            printf("-----------------------\n");
            for (int l = -1; l <= 11; l++)
            {
                if ((l >= 0) && (l < skip_level))
                    continue;

                printf("backwards vogl::map max_level: %i\n", l);

                timed_scope ts;

                typedef vogl::map<int, int> int_to_int_map;
                int_to_int_map m((l < 0) ? int_to_int_map::cDefaultMaxLevel : l);
                if (l >= 0)
                    m.set_fixed_map_level(l);

                for (uint32_t t = 0; t < Q; t++)
                    m.insert(Q - 1 - t, t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(Q - 1 - t);
                    m.erase(Q - 1 - t);
                }
#endif
            }

            printf("-----------------------\n");
            for (int l = -1; l <= 11; l++)
            {
                if ((l >= 0) && (l < skip_level))
                    continue;

                printf("random vogl::map max_level: %i\n", l);

                timed_scope ts;

                typedef vogl::map<int, int> int_to_int_map;
                int_to_int_map m((l < 0) ? int_to_int_map::cDefaultMaxLevel : l);
                if (l >= 0)
                    m.set_fixed_map_level(l);

                for (uint32_t t = 0; t < Q; t++)
                    m.insert(rand_indices[t], t * 2);

#if !VOGL_MAP_INSERT_ONLY_SPEED_TEST
                for (uint32_t t = 0; t < Q; t++)
                {
                    m.find(rand_indices[t]);
                    m.erase(rand_indices[t]);
                }
#endif
            }
        }
    }

} // namespace vogl
