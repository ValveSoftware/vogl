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

// File: vogl_map.h
// See https://en.wikipedia.org/wiki/Skip_list
// Inspired by Qt4's QMap and http://www.keithschwarz.com/interesting/code/?dir=skiplist
//
// Notes:
// vogl::map is a associative container implemented using a skip-list.
// Its interface and performance is roughly similar to std::map, std::multi_map, std::set, and std::multi_set.
//
// Perf:
// insert/erase perf on int->int test containers using prob .25 vs. glibc x64 is a mixed bag:
// If the entire container fits into the CPU cache, this class is up to 20-50% faster.
// For large int->int containers, for random insertion/erases this class is 25-35% slower, but for forward/reverse insertion/erases it's around 2x faster.
//
// Properties:
// Always 1 malloc per vogl::map instance (for the heap node), allocated at construction time.
// 1 additional malloc per inserted object, objects never move, objects are always traversable in sorted order forwards or backwards in constant time per iteration.
// iterators remain valid after inserting/erasing.
// key is stored immediately before value in memory, followed by the element pointers.
// Sets (using an empty Value struct) aren't as efficient as they could be, because empty_struct is 1 byte which gets aligned up to 4 bytes.
// In debug builds each node has a 16-bit marker to detect bogus iterators/overruns/etc.
//
// Duplicate key behavior:
//  - find() always finds the first key in a run of duplicate keys.
//  - insert() never overwrites duplicate keys.
//  - insert_multi() always inserts before the first key in a run of duplicate keys (i.e. duplicate keys are ordered by how recently they are inserted).
//  - erase() by key always erases the first key in a run of duplicate keys
//  - erase() by iterator may be slow within very large runs of duplicate keys.
//
// By default, the container will automatically nudge up the max pointer level as it grows, unless the fixed max level is enabled (not recommended).
// The max pointer level is never reduced, even after calling clear(). Use reset() instead if this is important.
// This implementation is hard coded to use a list level probability of 1/4. A max pointer level of 8 should be reasonable for 64k elements.
// Each container uses its own 32-bit PRNG with a constant seed.
//
// Item memory overhead: ~13 bytes avg overhead in 32-bit builds, ~26 bytes for 64-bit builds.
// Min overhead is sizeof(uint32_t)+sizeof(void*)*2 per element.
#pragma once

#include "vogl_core.h"
#include "vogl_rand.h"
#include "vogl_vector.h"
#include "vogl_console.h"

// If VOGL_MAP_USE_POINT_25_PROB is 1, the skip list uses prob = .25, otherwise it uses .5
#define VOGL_MAP_USE_POINT_25_PROB 1

namespace vogl
{
    enum
    {
        cMapNodeHeadDebugMarker = 0xE138,
        cMapListNodeItemDebugMarker = 0xE137,
        cMapNodeFreeDebugMarker = 0xFEFE
    };

    template <typename Key, typename Value>
    struct map_node;

    template <typename Key, typename Value, uint32_t N>
    struct map_node_ptrs
    {
#ifdef VOGL_ASSERTS_ENABLED
        uint16_t m_debug_marker;
#endif

        // m_num_next_ptrs is not needed in a basic implementation, but it can simplify erasing (especially with multiple equal elements) and helps debugging.
        uint8_t m_num_next_ptrs;

        map_node<Key, Value> *m_pPrev;

        // m_pNext[] must be last
        map_node<Key, Value> *m_pNext[N];
    };

    template <typename Key, typename Value>
    struct map_node : std::pair<const Key, Value>
    {
        map_node_ptrs<Key, Value, 1> m_ptrs;

        inline uint32_t get_num_next_ptrs() const
        {
            return m_ptrs.m_num_next_ptrs;
        }
        inline void set_num_next_ptrs(uint32_t n)
        {
            m_ptrs.m_num_next_ptrs = n;
        }

        inline void set_prev_ptr(map_node *pPrev)
        {
            m_ptrs.m_pPrev = pPrev;
        }
        inline map_node *get_prev_ptr() const
        {
            return m_ptrs.m_pPrev;
        }

        inline map_node *get_forward_ptr(uint32_t index) const
        {
            VOGL_ASSERT(index < m_ptrs.m_num_next_ptrs);
            return m_ptrs.m_pNext[index];
        }

        inline void set_forward_ptr(uint32_t index, map_node *p)
        {
            VOGL_ASSERT(index < m_ptrs.m_num_next_ptrs);
            m_ptrs.m_pNext[index] = p;
        }

        inline uint32_t get_allocated_size() const
        {
            return sizeof(*this) + (get_num_next_ptrs() - 1) * sizeof(map_node *);
        }

#ifdef VOGL_ASSERTS_ENABLED
        inline uint16_t get_debug_marker() const
        {
            return m_ptrs.m_debug_marker;
        }
        inline void set_debug_marker(uint16_t val)
        {
            m_ptrs.m_debug_marker = val;
        }
#endif

        inline void check_item_debug_marker() const
        {
            VOGL_ASSERT(m_ptrs.m_debug_marker == cMapListNodeItemDebugMarker);
        }
        inline void check_head_debug_marker() const
        {
            VOGL_ASSERT(m_ptrs.m_debug_marker == cMapNodeHeadDebugMarker);
        }
    };

    enum
    {
        cMapMaxPossibleLevels = VOGL_MAP_USE_POINT_25_PROB ? 16 : 32
    };

    // Using default template options, Key must support operator <.
    // If Key supports operator== and if operator< is complex, consider setting EqualComp to equal_to for a potential perf gain.
    template <typename Key, typename Value = empty_type,
              typename LessComp = less_than<Key>,
              typename EqualComp = equal_to_using_less_than<Key>,
              uint32_t MaxLevels = cMapMaxPossibleLevels>
    class map
    {
        friend class iterator_base;
        friend class iterator;
        friend class const_iterator;

        typedef map_node<const Key, Value> node_type;

    public:
        typedef map<Key, Value, LessComp, EqualComp, MaxLevels> map_type;
        typedef Key key_type;
        typedef Value referent_type;
        typedef std::pair<const Key, Value> value_type;
        typedef LessComp less_comp_type;
        typedef EqualComp equal_comp_type;

        enum
        {
            cMaxLevels = MaxLevels
        };

        template <typename DerivedType, typename Pointer, typename Reference>
        class iterator_base
        {
        protected:
            node_type *m_pNode;

            template <typename DerivedType2, typename Pointer2, typename Reference2>
            friend class iterator_base;

            inline iterator_base()
                : m_pNode(NULL)
            {
            }

            inline iterator_base(node_type *pNode)
                : m_pNode(pNode)
            {
            }

        public:
            // post-increment
            inline DerivedType operator++(int)
            {
                DerivedType result(static_cast<DerivedType &>(*this));
                ++*this;
                return result;
            }

            // pre-increment
            inline DerivedType &operator++()
            {
                VOGL_ASSERT(m_pNode);
                if (m_pNode)
                    m_pNode = m_pNode->get_forward_ptr(0);
                return static_cast<DerivedType &>(*this);
            }

            // post-decrement
            inline DerivedType operator--(int)
            {
                DerivedType result(static_cast<DerivedType &>(*this));
                --*this;
                return result;
            }

            // pre-decrement
            inline DerivedType &operator--()
            {
                VOGL_ASSERT(m_pNode);
                if (m_pNode)
                    m_pNode = m_pNode->get_prev_ptr();
                return static_cast<DerivedType &>(*this);
            }

            inline Reference operator*() const
            {
                VOGL_ASSERT(m_pNode);
                VOGL_ASSERT(m_pNode->get_debug_marker() == cMapListNodeItemDebugMarker);
                return *m_pNode;
            }

            inline Pointer operator->() const
            {
                VOGL_ASSERT(m_pNode->get_debug_marker() == cMapListNodeItemDebugMarker);
                return m_pNode;
            }

            template <typename DerivedType2, typename Pointer2, typename Reference2>
            inline bool operator==(const iterator_base<DerivedType2, Pointer2, Reference2> &rhs) const
            {
                return (m_pNode == rhs.m_pNode);
            }

            template <typename DerivedType2, typename Pointer2, typename Reference2>
            inline bool operator!=(const iterator_base<DerivedType2, Pointer2, Reference2> &rhs) const
            {
                return (m_pNode != rhs.m_pNode);
            }
        };

        class iterator : public iterator_base<iterator, std::pair<const Key, Value> *, std::pair<const Key, Value> &>
        {
            friend class map;
            friend class const_iterator;

            inline iterator(node_type *pNode)
                : iterator_base<iterator, std::pair<const Key, Value> *, std::pair<const Key, Value> &>(pNode)
            {
            }

        public:
            inline iterator()
            {
            }
        };

        class const_iterator : public iterator_base<const_iterator, const std::pair<const Key, Value> *, const std::pair<const Key, Value> &>
        {
            friend class map;
            friend class iterator;

            inline const_iterator(node_type *pNode)
                : iterator_base<const_iterator, const std::pair<const Key, Value> *, const std::pair<const Key, Value> &>(pNode)
            {
            }

        public:
            inline const_iterator()
            {
            }

            inline const_iterator(iterator itr)
                : iterator_base<const_iterator, const std::pair<const Key, Value> *, const std::pair<const Key, Value> &>(itr.m_pNode)
            {
            }
        };

        // max_level must be < MaxLevels (i.e. 15 or less with default template MaxLevels)
        // By default the max level will automatically increase as the container grows, which leads to reasonable behavior in testing.
        // You can fix the max level by calling set_fixed_map_level(), but use caution because if the level is too low perf will be dreadful.
        //
        // Max level guide:
        // 0 < 4 entries
        // 1 = 4 entries
        // 2 = 16 entries
        // 3 = 128 entries
        // 4 = 256 entries
        // 5 = ~1k entries
        // 6 is good enough for 1/(.25 ^ 6) = ~4k entries
        // 7 = ~16k entries
        // 8 = ~64k entries
        // 9 = ~256k entries
        // 10 = ~1m entries
        // 11 = ~4m entries
        // 12 = ~16m entries
        // 13 = ~64m entries
        // A decent max level ~= ceil(log(N)/log(2) * 0.5)

        enum
        {
            cDefaultMaxLevel = VOGL_MAP_USE_POINT_25_PROB ? 3 : 6
        };

        inline map(uint32_t initial_max_level = cDefaultMaxLevel, const LessComp &less_than_obj = less_comp_type(), const EqualComp &equal_to_obj = equal_comp_type())
            : m_total_allocated(0),
              m_pHead(NULL),
              m_size(0),
              m_fixed_max_level(false),
              m_is_key_less_than(less_than_obj),
              m_is_key_equal_to(equal_to_obj)
        {
            init(initial_max_level);
        }

        inline map(const map &other)
            : m_total_allocated(0),
              m_pHead(NULL),
              m_size(0),
              m_rand(other.m_rand),
              m_fixed_max_level(other.m_fixed_max_level),
              m_is_key_less_than(other.m_is_key_less_than),
              m_is_key_equal_to(other.m_is_key_equal_to)
        {
            clone(other);
        }

        inline map &operator=(const map &rhs)
        {
            if (this == &rhs)
                return *this;

            clear();

            m_is_key_less_than = rhs.m_is_key_less_than;
            m_is_key_equal_to = rhs.m_is_key_equal_to;
            m_fixed_max_level = rhs.m_fixed_max_level;
            m_rand = rhs.m_rand;

            clone(rhs);

            return *this;
        }

        inline ~map()
        {
            m_pHead->check_head_debug_marker();

            node_type *pCur = m_pHead->get_forward_ptr(0);
            while (pCur != m_pHead)
            {
                pCur->check_item_debug_marker();
                node_type *pNext = pCur->get_forward_ptr(0);
                free_node(pCur);
                pCur = pNext;
            }

#ifdef VOGL_ASSERTS_ENABLED
            m_pHead->set_debug_marker(cMapNodeFreeDebugMarker);
#endif

            vogl_free(m_pHead);
        }

        // clear() wipes the container, but does not change the current max level
        inline void clear()
        {
            m_pHead->check_head_debug_marker();

            node_type *pCur = m_pHead->get_forward_ptr(0);
            while (pCur != m_pHead)
            {
                pCur->check_item_debug_marker();

                node_type *pNext = pCur->get_forward_ptr(0);
                free_node(pCur);
                pCur = pNext;
            }

            VOGL_ASSERT(!m_total_allocated);

            m_pHead->set_prev_ptr(m_pHead);
            for (uint32_t i = 0; i <= m_max_level; i++)
                m_pHead->set_forward_ptr(i, m_pHead);

            m_total_allocated = 0;
            m_size = 0;
            m_cur_level = 0;
        }

        inline void reset(uint32_t initial_max_level = cDefaultMaxLevel, const LessComp &less_than_obj = less_comp_type(), const EqualComp &equal_to_obj = equal_comp_type())
        {
            clear();

            m_rand.set_default_seed();
            m_fixed_max_level = false;
            m_is_key_less_than = less_than_obj;
            m_is_key_equal_to = equal_to_obj;

            init(initial_max_level);
        }

        inline iterator begin()
        {
            m_pHead->check_head_debug_marker();
            return iterator(m_pHead->get_forward_ptr(0));
        }
        inline const_iterator begin() const
        {
            m_pHead->check_head_debug_marker();
            return const_iterator(m_pHead->get_forward_ptr(0));
        }

        inline iterator end()
        {
            m_pHead->check_head_debug_marker();
            return iterator(m_pHead);
        }
        inline const_iterator end() const
        {
            m_pHead->check_head_debug_marker();
            return const_iterator(m_pHead);
        }

        inline int size() const
        {
            return m_size;
        }

        inline bool is_empty() const
        {
            return !m_size;
        }

        inline int64_t get_total_bytes_allocated() const
        {
            return m_total_allocated;
        }

        inline uint32_t get_cur_list_level() const
        {
            return m_cur_level;
        }
        inline uint32_t get_cur_max_level() const
        {
            return m_max_level;
        }
        inline uint32_t get_max_possible_level() const
        {
            return cMaxLevels - 1;
        }

        inline uint32_t get_fixed_map_level() const
        {
            return m_fixed_max_level;
        }

        inline void set_fixed_map_level(uint32_t level)
        {
            if (m_fixed_max_level)
                return;

            m_fixed_max_level = true;
            m_bump_max_level_size_thresh = cUINT32_MAX;

            level = math::minimum<uint32_t>(level, cMaxLevels - 1);

            if (level > m_max_level)
            {
                m_max_level = level;
                m_pHead->set_num_next_ptrs(m_max_level + 1);

                VOGL_ASSERT(m_pHead->get_forward_ptr(m_max_level) == m_pHead);
            }
        }

        inline const EqualComp &get_equals() const
        {
            return m_is_key_equal_to;
        }

        inline EqualComp &get_equals()
        {
            return m_is_key_equal_to;
        }

        inline void set_equals(const EqualComp &equals)
        {
            m_is_key_equal_to = equals;
        }

        inline const LessComp &get_less() const
        {
            return m_is_key_less_than;
        }

        inline LessComp &get_less()
        {
            return m_is_key_less_than;
        }

        inline void set_less(const LessComp &less)
        {
            m_is_key_less_than = less;
        }

        inline void reserve(size_t)
        {
            // nothing to do
        }

        // insert_result.first will always point to inserted key/value (or the already existing key/value).
        // insert_result.second will be true if a new key/value was inserted, or false if the key already existed (in which case first will point to the already existing value).
        typedef std::pair<iterator, bool> insert_result;

        insert_result insert(const Key &key, const Value &value = Value())
        {
            return insert(key, value, false);
        }

        inline insert_result insert(const value_type &value)
        {
            return insert(value.first, value.second, false);
        }

        // insert_multi() allows items with duplicate keys.
        insert_result insert_multi(const Key &key, const Value &value = Value())
        {
            return insert(key, value, true);
        }

        inline insert_result insert_multi(const value_type &value)
        {
            return insert(value.first, value.second, true);
        }

        inline Value &operator[](const Key &key)
        {
            return (insert(key).first)->second;
        }

        // Returns const ref to value if key is found, otherwise returns the default.
        inline const Value &value(const Key &key, const Value &def = Value()) const
        {
            const_iterator it(find(key));
            if (it != end())
                return it->second;
            return def;
        }

        // iterator->first is the key, iterator->second is the value, or returns end() if the key cannot be found
        inline const_iterator find(const Key &key) const
        {
            return find_const(key);
        }

        // iterator->first is the key, iterator->second is the value, or returns end() if the key cannot be found
        inline iterator find(const Key &key)
        {
            const_iterator it(find_const(key));
            return iterator(it.m_pNode);
        }

        // Return pointer to the value associated with key, or NULL if it doesn't exist.
        inline Value *find_value(const Key &key)
        {
            node_type *p = find_prev_node(key);

            p = p->get_forward_ptr(0);
            if ((p != m_pHead) && (m_is_key_equal_to(p->first, key)))
                return &p->second;

            return NULL;
        }

        // Return pointer to the value associated with key, or NULL if it doesn't exist.
        inline const Value *find_value(const Key &key) const
        {
            node_type *p = find_prev_node(key);

            p = p->get_forward_ptr(0);
            if ((p != m_pHead) && (m_is_key_equal_to(p->first, key)))
                return &p->second;

            return NULL;
        }

        // true if the key is found.
        inline bool contains(const Key &key) const
        {
            m_pHead->check_head_debug_marker();

            node_type *p = find_prev_node(key);

            p = p->get_forward_ptr(0);
            if ((p != m_pHead) && (m_is_key_equal_to(p->first, key)))
                return true;

            return false;
        }

        // Returns the # of items associated with the specified key.
        inline uint32_t count(const Key &key) const
        {
            m_pHead->check_head_debug_marker();

            const_iterator it(find(key));
            if (it == end())
                return 0;

            uint32_t n = 1;

            for (;;)
            {
                if (++it == end())
                    break;
                if (it->first != key)
                    break;
                n++;
            }

            return n;
        }

        // Erases the first item associated with the specified key. If there are multiple items with the same key, the most recently inserted will be erased.
        bool erase(const Key &key)
        {
            VOGL_ASSERT((m_max_level < cMaxLevels) && (m_cur_level <= m_max_level));
            m_pHead->check_head_debug_marker();

            node_type *p = m_pHead;

            node_type *ppPredecessors[cMaxLevels];
            for (int i = m_cur_level; i >= 0; i--)
            {
                for (;;)
                {
                    node_type *pNext = p->get_forward_ptr(i);
                    if ((pNext == m_pHead) || (!m_is_key_less_than(pNext->first, key)))
                        break;

                    p = pNext;

                    pNext = p->get_forward_ptr(i);
                    if ((pNext == m_pHead) || (!m_is_key_less_than(pNext->first, key)))
                        break;

                    p = pNext;
                }

                ppPredecessors[i] = p;
            }

            node_type *pPrev = p;

            p = p->get_forward_ptr(0);

            if ((p == m_pHead) || (!(m_is_key_equal_to(p->first, key))))
                return false;

            VOGL_ASSERT(m_size > 0);

            node_type *pNext = p->get_forward_ptr(0);

            VOGL_ASSERT(pPrev->get_forward_ptr(0) == p);
            VOGL_ASSERT(pNext->get_prev_ptr() == p);

            pPrev->set_forward_ptr(0, pNext);
            pNext->set_prev_ptr(pPrev);

            for (uint32_t i = 1; i <= m_cur_level; i++)
            {
                if (ppPredecessors[i]->get_forward_ptr(i) != p)
                    break;

                ppPredecessors[i]->set_forward_ptr(i, p->get_forward_ptr(i));
            }

            // Be careful, key is a ref and it could be freed here!
            free_node(p);

            while ((m_cur_level > 0) && (m_pHead->get_forward_ptr(m_cur_level) == m_pHead))
                m_cur_level--;

            --m_size;

            return true;
        }

        // This method erases a specific object from the container.
        // It's more complex than erase() by key because there could be duplicate items.
        void erase(const iterator &it)
        {
            VOGL_ASSERT((m_max_level < cMaxLevels) && (m_cur_level <= m_max_level));
            m_pHead->check_head_debug_marker();

            if (!m_size)
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }

            node_type *pNode_to_erase = it.m_pNode;
            if (pNode_to_erase == m_pHead)
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }
            pNode_to_erase->check_item_debug_marker();

            const int max_node_level = pNode_to_erase->get_num_next_ptrs() - 1;
            if ((max_node_level < 0) || (max_node_level > m_cur_level))
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }

            if (max_node_level > 0)
            {
                const Key &key = it->first;

                node_type *p = m_pHead;

                node_type *ppPredecessors[cMaxLevels];

#ifdef VOGL_BUILD_DEBUG
                ppPredecessors[0] = NULL;
#endif
                for (int i = m_cur_level; i >= 1; i--)
                {
                    for (;;)
                    {
                        node_type *pNext = p->get_forward_ptr(i);
                        if ((pNext == m_pHead) || (!m_is_key_less_than(pNext->first, key)))
                            break;

                        p = pNext;

                        pNext = p->get_forward_ptr(i);
                        if ((pNext == m_pHead) || (!m_is_key_less_than(pNext->first, key)))
                            break;

                        p = pNext;
                    }

                    ppPredecessors[i] = p;
                }

                for (int i = 1; i <= max_node_level; i++)
                {
                    node_type *q = ppPredecessors[i];

                    while (q->get_forward_ptr(i) != pNode_to_erase)
                    {
                        q = q->get_forward_ptr(i);

                        if ((q == m_pHead) || (m_is_key_less_than(key, q->first)))
                        {
                            // Something is very wrong, because the node to delete MUST be in this level's list and we've now either reached the end or a key which is greater.
                            VOGL_ASSERT_ALWAYS;
                            return;
                        }

                        VOGL_ASSERT(m_is_key_equal_to(key, q->first));
                    }

                    q->set_forward_ptr(i, pNode_to_erase->get_forward_ptr(i));
                }
            }

            node_type *pPrev = pNode_to_erase->get_prev_ptr();
            node_type *pNext = pNode_to_erase->get_forward_ptr(0);

            VOGL_ASSERT(pPrev->get_forward_ptr(0) == pNode_to_erase);
            VOGL_ASSERT(pNext->get_prev_ptr() == pNode_to_erase);

            pPrev->set_forward_ptr(0, pNext);
            pNext->set_prev_ptr(pPrev);

            free_node(pNode_to_erase);

            while ((m_cur_level > 0) && (m_pHead->get_forward_ptr(m_cur_level) == m_pHead))
                m_cur_level--;

            --m_size;
        }

        // Erases all items with the specified key.
        // Returns the total number of erased keys.
        inline uint32_t erase_all(const Key &key)
        {
            m_pHead->check_head_debug_marker();

            std::pair<iterator, iterator> it_range(equal_range(key));

            // Use caution here - key could point into one of the items we're about to erase!

            uint32_t n = 0;
            for (iterator it = it_range.first; it != it_range.second;)
            {
                iterator next_it(it);
                ++next_it;

                erase(it);
                n++;

                it = next_it;
            }

            return n;
        }

        // Returns an iterator pointing to the first item with key key in the map.
        // If the map contains no item with key key, the function returns an iterator to the nearest item with a greater key.
        inline const_iterator lower_bound(const Key &key) const
        {
            m_pHead->check_head_debug_marker();

            node_type *pPrev = find_prev_node(key);
            node_type *pNext = pPrev->get_forward_ptr(0);
            return const_iterator(pNext);
        }

        inline iterator lower_bound(const Key &key)
        {
            m_pHead->check_head_debug_marker();

            node_type *pPrev = find_prev_node(key);
            node_type *pNext = pPrev->get_forward_ptr(0);
            return iterator(pNext);
        }

        // Returns an iterator pointing to the item that immediately follows the last item with key key in the map.
        // If the map contains no item with key key, the function returns an iterator to the nearest item with a greater key.
        inline const_iterator upper_bound(const Key &key) const
        {
            m_pHead->check_head_debug_marker();

            node_type *pPrev = find_prev_node(key);
            node_type *pNext = pPrev->get_forward_ptr(0);

            while (pNext != m_pHead)
            {
                if (!m_is_key_equal_to(pNext->first, key))
                    break;
                pNext = pNext->get_forward_ptr(0);
            }

            return const_iterator(pNext);
        }

        inline iterator upper_bound(const Key &key)
        {
            m_pHead->check_head_debug_marker();

            node_type *pPrev = find_prev_node(key);
            node_type *pNext = pPrev->get_forward_ptr(0);

            while (pNext != m_pHead)
            {
                if (!m_is_key_equal_to(pNext->first, key))
                    break;
                pNext = pNext->get_forward_ptr(0);
            }

            return iterator(pNext);
        }

        // Returns a pair of iterators delimiting the range of values that are stored under key.
        inline std::pair<iterator, iterator> equal_range(const Key &key)
        {
            m_pHead->check_head_debug_marker();

            node_type *pPrev = find_prev_node(key);
            node_type *pNext = pPrev->get_forward_ptr(0);

            if ((pNext == m_pHead) || (!m_is_key_equal_to(pNext->first, key)))
                return std::make_pair(end(), end());

            iterator first_it(pNext);

            for (;;)
            {
                pNext = pNext->get_forward_ptr(0);
                if ((pNext == m_pHead) || (!m_is_key_equal_to(pNext->first, key)))
                    break;
            }

            return std::make_pair(first_it, iterator(pNext));
        }

        inline std::pair<const_iterator, const_iterator> equal_range(const Key &key) const
        {
            m_pHead->check_head_debug_marker();

            node_type *pPrev = find_prev_node(key);
            node_type *pNext = pPrev->get_forward_ptr(0);

            if ((pNext == m_pHead) || (!m_is_key_equal_to(pNext->first, key)))
                return std::make_pair(end(), end());

            const_iterator first_it(pNext);

            for (;;)
            {
                pNext = pNext->get_forward_ptr(0);
                if ((pNext == m_pHead) || (!m_is_key_equal_to(pNext->first, key)))
                    break;
            }

            return std::make_pair(first_it, const_iterator(pNext));
        }

        // Appends all unique keys to the specified vector.
        inline vogl::vector<Key> &get_unique_keys(vogl::vector<Key> &vec) const
        {
            m_pHead->check_head_debug_marker();

            if (!m_size)
                return vec;

            node_type *pCur = m_pHead->get_forward_ptr(0);
            while (pCur != m_pHead)
            {
                const Key &cur_key = pCur->first;
                vec.push_back(cur_key);

                pCur = pCur->get_forward_ptr(0);

                while ((pCur != m_pHead) && (pCur->first == cur_key))
                    pCur = pCur->get_forward_ptr(0);
            }

            return vec;
        }

        // Appends all keys to the specified vector.
        inline vogl::vector<Key> &get_keys(vogl::vector<Key> &vec) const
        {
            m_pHead->check_head_debug_marker();

            if (!m_size)
                return vec;

            vec.reserve(vec.size() + m_size);

            node_type *pCur = m_pHead->get_forward_ptr(0);
            while (pCur != m_pHead)
            {
                vec.push_back(pCur->first);
                pCur = pCur->get_forward_ptr(0);
            }

            return vec;
        }

        // Appends all values to the specified vector.
        inline vogl::vector<Value> &get_values(vogl::vector<Value> &vec) const
        {
            m_pHead->check_head_debug_marker();

            if (!m_size)
                return vec;

            vec.reserve(vec.size() + m_size);

            node_type *pCur = m_pHead->get_forward_ptr(0);
            while (pCur != m_pHead)
            {
                vec.push_back(pCur->second);
                pCur = pCur->get_forward_ptr(0);
            }

            return vec;
        }

        // Appends all items with key to the specified vector.
        inline vogl::vector<Value> &get_values(const Key &key, vogl::vector<Value> &vec) const
        {
            m_pHead->check_head_debug_marker();

            node_type *pPrev = find_prev_node(key);
            node_type *pNext = pPrev->get_forward_ptr(0);

            while ((pNext != m_pHead) && (m_is_key_equal_to(pNext->first, key)))
            {
                vec.push_back(pNext->second);

                pNext = pNext->get_forward_ptr(0);
            }

            return vec;
        }

        // Inserts all items from the specified map to this map.
        inline map &unite_multi(const map &rhs)
        {
            m_pHead->check_head_debug_marker();
            rhs.m_pHead->check_head_debug_marker();

            if (this == &rhs)
            {
                VOGL_ASSERT_ALWAYS;
                return *this;
            }

            for (const_iterator it = rhs.begin(); it != rhs.end(); ++it)
                insert_multi(it->first, it->second);

            return *this;
        }

        inline map &unite(const map &rhs)
        {
            m_pHead->check_head_debug_marker();
            rhs.m_pHead->check_head_debug_marker();

            if (this == &rhs)
            {
                VOGL_ASSERT_ALWAYS;
                return *this;
            }

            for (const_iterator it = rhs.begin(); it != rhs.end(); ++it)
                insert(it->first, it->second);

            return *this;
        }

        // Fast swap of this container with another.
        inline void swap(map &other)
        {
            m_pHead->check_head_debug_marker();
            other.m_pHead->check_head_debug_marker();

            std::swap(m_total_allocated, other.m_total_allocated);
            std::swap(m_is_key_less_than, other.m_is_key_less_than);
            std::swap(m_is_key_equal_to, other.m_is_key_equal_to);
            std::swap(m_rand, other.m_rand);
            std::swap(m_size, other.m_size);
            std::swap(m_bump_max_level_size_thresh, other.m_bump_max_level_size_thresh);
            std::swap(m_cur_level, other.m_cur_level);
            std::swap(m_max_level, other.m_max_level);
            std::swap(m_pHead, other.m_pHead);
            std::swap(m_fixed_max_level, other.m_fixed_max_level);
        }

        // Compares this container's full contents to another.
        inline bool operator==(const map &rhs) const
        {
            if (this == &rhs)
                return true;

            m_pHead->check_head_debug_marker();
            rhs.m_pHead->check_head_debug_marker();

            if (m_size != rhs.m_size)
                return false;

            const_iterator lhs_it(begin());
            const_iterator rhs_it(rhs.begin());

            while (lhs_it != end())
            {
                VOGL_ASSERT(rhs_it != rhs.end());

                if (*lhs_it != *rhs_it)
                    return false;

                ++lhs_it;
                ++rhs_it;
            }

            return true;
        }

        inline bool operator!=(const map &rhs) const
        {
            return !(*this == rhs);
        }

        void print_debug_info()
        {
            m_pHead->check_head_debug_marker();

            vogl_debug_printf("map 0x%p: Size: %u elements, Max possible level: %u, Cur max level: %u, Cur level: %u\n",
                           this, m_size, cMaxLevels - 1, m_max_level, m_cur_level);
            vogl_debug_printf("  Bump max level size: %u\n", m_bump_max_level_size_thresh);
            vogl_debug_printf("  Key size: %u bytes, Value size: %u bytes, KeyValue size: %u bytes, Min element size: %u bytes\n", (uint32_t)sizeof(Key), (uint32_t)sizeof(Value), (uint32_t)sizeof(value_type), (uint32_t)sizeof(node_type));
            vogl_debug_printf("  Total allocated: %" PRIu64 " bytes, Avg allocated per element: %f bytes, Avg overhead: %f bytes\n", m_total_allocated,
                           m_size ? m_total_allocated / (double)m_size : 0,
                           m_size ? (m_total_allocated / (double)m_size) - sizeof(value_type) : 0);
            vogl_debug_printf("  Max element size: %u bytes\n", (uint32_t)(sizeof(node_type) + sizeof(void *) * m_max_level));

            for (uint32_t level = 0; level <= m_max_level; level++)
            {
                uint32_t n = 0;

                node_type *p = m_pHead->get_forward_ptr(level);

                int64_t total_size_at_this_level = 0;
                uint32_t total_count_at_this_level = 0;

                while (p != m_pHead)
                {
                    p->check_item_debug_marker();

                    if (p->get_num_next_ptrs() == (level + 1))
                    {
                        total_size_at_this_level += p->get_allocated_size();
                        total_count_at_this_level++;
                    }

                    p = p->get_forward_ptr(level);
                    n++;
                }

                float prob = VOGL_MAP_USE_POINT_25_PROB ? (1.0f / 4.0f) : (1.0f / 2.0f);
                vogl_debug_printf("  Level %u: Total: %u (Expected: %f), At this level: %u (Expected: %f), Alloc bytes at this level: %" PRIi64 ", Avg alloc bytes at this level: %f\n",
                               level, n, m_size * pow(prob, level),
                               total_count_at_this_level, m_size * pow(prob, level) * ((level == m_max_level) ? 1.0f : (1.0f - prob)),
                               total_size_at_this_level,
                               total_count_at_this_level ? total_size_at_this_level / (double)total_count_at_this_level : 0);
            }
        }

        // Returns false if the container is invalid/corrupted.
        bool debug_check() const
        {
            m_pHead->check_head_debug_marker();

            if (m_cur_level > m_max_level)
                return false;
            if (!m_pHead)
                return false;

            if (m_size > m_bump_max_level_size_thresh)
                return false;

            if (!m_size)
            {
                if (m_pHead->get_prev_ptr() != m_pHead)
                    return false;
                if (m_pHead->get_forward_ptr(0) != m_pHead)
                    return false;
                if (m_cur_level > 0)
                    return false;
                return true;
            }

            for (uint32_t i = m_cur_level + 1; i <= m_max_level; i++)
            {
                if (m_pHead->get_forward_ptr(i) != m_pHead)
                    return false;
            }

            for (uint32_t i = 0; i <= m_cur_level; i++)
            {
                uint32_t num_nodes_examined = 0;
                node_type *pCur = m_pHead;

                int64_t total_node_bytes = 0;

                for (;;)
                {
                    if (!pCur->get_num_next_ptrs())
                        return false;

                    if (pCur == m_pHead)
                    {
                        pCur->check_head_debug_marker();

                        if (pCur->get_num_next_ptrs() != (m_max_level + 1U))
                            return false;
                    }
                    else
                    {
                        pCur->check_item_debug_marker();

                        if (pCur->get_num_next_ptrs() > (m_cur_level + 1U))
                            return false;
                    }

                    if (i >= pCur->get_num_next_ptrs())
                        return false;

                    node_type *pNext = pCur->get_forward_ptr(i);

                    if (!pNext)
                        return false;

                    if (pNext == pCur)
                        return false;

                    if (i == 0)
                    {
                        if (pNext->get_prev_ptr() != pCur)
                            return false;
                    }

                    if (pNext == m_pHead)
                        break;

                    if (i == 0)
                    {
                        total_node_bytes += sizeof(node_type) + sizeof(node_type *) * (pNext->get_num_next_ptrs() - 1);
                    }

                    num_nodes_examined++;
                    if (num_nodes_examined > m_size)
                        return false;

                    if (pCur != m_pHead)
                    {
                        if (m_is_key_less_than(pNext->first, pCur->first))
                            return false;
                    }

                    if (i)
                    {
                        node_type *q = pCur->get_forward_ptr(i - 1);

                        while (q != pNext)
                        {
                            if (q == m_pHead)
                                return false;

                            q = q->get_forward_ptr(i - 1);
                        }
                    }

                    pCur = pNext;
                }

                if (!num_nodes_examined)
                    return false;

                if (i == 0)
                {
                    if (num_nodes_examined != m_size)
                        return false;
                    if (total_node_bytes != m_total_allocated)
                        return false;
                }
            }

            return true;
        }

    private:
        int64_t m_total_allocated;

        node_type *m_pHead;

        uint32_t m_size;
        uint32_t m_bump_max_level_size_thresh;

        fast_random m_rand;

        uint8_t m_cur_level;
        uint8_t m_max_level;

        bool m_fixed_max_level;

        less_comp_type m_is_key_less_than;
        equal_comp_type m_is_key_equal_to;

        inline void free_node(node_type *p)
        {
            p->check_item_debug_marker();

            m_total_allocated -= (sizeof(node_type) + sizeof(node_type *) * (p->get_num_next_ptrs() - 1));
            VOGL_ASSERT(m_total_allocated >= 0);

#ifdef VOGL_ASSERTS_ENABLED
            p->set_debug_marker(cMapNodeFreeDebugMarker);
#endif

            helpers::destruct(p);

            vogl_free(p);
        }

        inline node_type *alloc_node(uint32_t num_forward_ptrs, const Key &key, const Value &val)
        {
            VOGL_ASSERT(num_forward_ptrs && (num_forward_ptrs < cMaxLevels));

            uint32_t alloc_size = sizeof(node_type) + sizeof(node_type *) * (num_forward_ptrs - 1);
            m_total_allocated += alloc_size;

            node_type *p = static_cast<node_type *>(vogl_malloc(alloc_size));

            p->set_num_next_ptrs(num_forward_ptrs);

#ifdef VOGL_ASSERTS_ENABLED
            p->set_debug_marker(cMapListNodeItemDebugMarker);
#endif

            helpers::construct(const_cast<Key *>(&p->first), key);
            helpers::construct(&p->second, val);

            return p;
        }

        void init(uint32_t initial_max_level)
        {
            VOGL_VERIFY(initial_max_level < cMaxLevels);
            VOGL_ASSERT(!m_size && !m_total_allocated);

            m_max_level = initial_max_level;
            m_cur_level = 0;

            m_bump_max_level_size_thresh = cUINT32_MAX;
            if (!m_fixed_max_level)
            {
#if VOGL_MAP_USE_POINT_25_PROB
                if (initial_max_level < 16)
                    m_bump_max_level_size_thresh = 1U << (initial_max_level * 2U);
#else
                if (initial_max_level < 32)
                    m_bump_max_level_size_thresh = 1U << initial_max_level;
#endif
            }

            if (!m_pHead)
            {
                m_pHead = static_cast<node_type *>(vogl_malloc(sizeof(node_type) + (cMaxLevels - 1) * sizeof(void *)));

                // Purposely clearing the whole thing, because we're not going to construct the Key/Value at the beginning (and if somebody screws up and accesses the head by accident, at least they get zero's instead of garbage).
                memset(m_pHead, 0, sizeof(node_type));

#ifdef VOGL_ASSERTS_ENABLED
                m_pHead->set_debug_marker(cMapNodeHeadDebugMarker);
#endif
            }
            else
            {
                m_pHead->check_head_debug_marker();

                // Paranoid checks.
                VOGL_ASSERT(m_pHead->get_prev_ptr() == m_pHead);
                VOGL_ASSERT(m_pHead->get_num_next_ptrs() < cMaxLevels);
                for (uint32_t i = 0; i < cMaxLevels; i++)
                {
                    VOGL_ASSERT(m_pHead->m_ptrs.m_pNext[i] == m_pHead);
                }
            }

            m_pHead->set_prev_ptr(m_pHead);

            m_pHead->set_num_next_ptrs(cMaxLevels);
            for (uint32_t i = 0; i < cMaxLevels; i++)
                m_pHead->set_forward_ptr(i, m_pHead);

            m_pHead->set_num_next_ptrs(m_max_level + 1);
        }

        void clone(const map &other)
        {
            other.m_pHead->check_head_debug_marker();

            init(other.m_max_level);

            node_type *pCur = other.m_pHead->get_forward_ptr(0);
            while (pCur != other.m_pHead)
            {
                pCur->check_item_debug_marker();

                insert_multi(*pCur);

                pCur = pCur->get_forward_ptr(0);
            }
        }

        // Returns the node previous to key. The next node will be the first instance of key, or the next greater key, or m_pHead.
        inline node_type *find_prev_node(const Key &key) const
        {
            node_type *p = m_pHead;

            p->check_head_debug_marker();

            for (int i = m_cur_level; i >= 0; i--)
            {
                for (;;)
                {
                    node_type *pNext = p->get_forward_ptr(i);

                    if (pNext == m_pHead)
                        break;
                    pNext->check_item_debug_marker();
                    if (!m_is_key_less_than(pNext->first, key))
                        break;

                    p = pNext;

                    pNext = p->get_forward_ptr(i);
                    if (pNext == m_pHead)
                        break;
                    pNext->check_item_debug_marker();
                    if (!m_is_key_less_than(pNext->first, key))
                        break;

                    p = pNext;

                    pNext = p->get_forward_ptr(i);
                    if (pNext == m_pHead)
                        break;
                    pNext->check_item_debug_marker();
                    if (!m_is_key_less_than(pNext->first, key))
                        break;

                    p = pNext;
                }
            }

            return p;
        }

        inline const_iterator find_const(const Key &key) const
        {
            node_type *p = find_prev_node(key);

            p = p->get_forward_ptr(0);
            if ((p != m_pHead) && (m_is_key_equal_to(p->first, key)))
                return const_iterator(p);

            return end();
        }

        // insert_result.first will always point to inserted key/value (or the already existing key/value).
        // insert_result.second will be true if a new key/value was inserted, or false if the key already existed (in which case first will point to the already existing value).
        insert_result insert(const Key &key, const Value &value, bool allow_dups)
        {
            VOGL_ASSERT((m_max_level < cMaxLevels) && (m_cur_level <= m_max_level));
            m_pHead->check_head_debug_marker();

            node_type *p = m_pHead;

            node_type *ppPredecessors[cMaxLevels];
            for (int i = m_cur_level; i >= 0; i--)
            {
                for (;;)
                {
                    node_type *pNext = p->get_forward_ptr(i);
                    if ((pNext == m_pHead) || (!m_is_key_less_than(pNext->first, key)))
                        break;

                    p = pNext;

                    pNext = p->get_forward_ptr(i);
                    if ((pNext == m_pHead) || (!m_is_key_less_than(pNext->first, key)))
                        break;

                    p = pNext;

                    pNext = p->get_forward_ptr(i);
                    if ((pNext == m_pHead) || (!m_is_key_less_than(pNext->first, key)))
                        break;

                    p = pNext;
                }

                ppPredecessors[i] = p;
            }

            if (!allow_dups)
            {
                p = p->get_forward_ptr(0);
                if ((p != m_pHead) && (m_is_key_equal_to(p->first, key)))
                    return std::make_pair(iterator(p), false);
            }

            if (m_size == cUINT32_MAX)
            {
                VOGL_ASSERT_ALWAYS;
                return std::make_pair(begin(), false);
            }

            uint32_t rnd = m_rand.urand32();
            int new_level = math::count_leading_zero_bits(rnd);
#if VOGL_MAP_USE_POINT_25_PROB
            new_level >>= 1U;
#endif
            new_level = math::minimum<uint32_t>(new_level, m_max_level);

            if (new_level > m_cur_level)
            {
                for (int i = m_cur_level + 1; i <= new_level; i++)
                    ppPredecessors[i] = m_pHead;

                m_cur_level = new_level;
            }

            node_type *pNew_node = alloc_node(new_level + 1, key, value);

            node_type *pPrev = ppPredecessors[0];
            node_type *pNext = pPrev->get_forward_ptr(0);

            VOGL_ASSERT(pNext->get_prev_ptr() == pPrev);

            pPrev->set_forward_ptr(0, pNew_node);
            pNext->set_prev_ptr(pNew_node);
            pNew_node->set_prev_ptr(pPrev);
            pNew_node->set_forward_ptr(0, pNext);

            for (int i = 1; i <= new_level; i++)
            {
                pNew_node->set_forward_ptr(i, ppPredecessors[i]->get_forward_ptr(i));
                ppPredecessors[i]->set_forward_ptr(i, pNew_node);
            }

            ++m_size;
            if ((m_size > m_bump_max_level_size_thresh) && (static_cast<int>(m_max_level) < (static_cast<int>(cMaxLevels) - 1)))
            {
                m_max_level++;

                m_pHead->set_num_next_ptrs(m_max_level + 1);
                VOGL_ASSERT(m_pHead->get_forward_ptr(m_max_level) == m_pHead);

                uint32_t orig_thresh_size = m_bump_max_level_size_thresh;
                m_bump_max_level_size_thresh <<= (VOGL_MAP_USE_POINT_25_PROB ? 2U : 1U);
                if (m_bump_max_level_size_thresh < orig_thresh_size)
                    m_bump_max_level_size_thresh = cUINT32_MAX;
            }

            return std::make_pair(iterator(pNew_node), true);
        }
    };

    template <typename Key, typename Value, typename LessComp, typename EqualComp, uint32_t MaxLevels>
    struct bitwise_movable<map<Key, Value, LessComp, EqualComp, MaxLevels> >
    {
        enum
        {
            cFlag = true
        };
    };

    template <typename Key, typename Value, typename LessComp, typename EqualComp, uint32_t MaxLevels>
    inline void swap(map<Key, Value, LessComp, EqualComp, MaxLevels> &a, map<Key, Value, LessComp, EqualComp, MaxLevels> &b)
    {
        a.swap(b);
    }

    bool map_test();
    void map_perf_test(uint32_t Q = 20000);

} // namespace vogl

namespace std
{
    template <typename Key, typename Value, typename LessComp, typename EqualComp, uint32_t MaxLevels>
    inline void swap(vogl::map<Key, Value, LessComp, EqualComp, MaxLevels> &a, vogl::map<Key, Value, LessComp, EqualComp, MaxLevels> &b)
    {
        a.swap(b);
    }
}
