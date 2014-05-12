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

// File: vogl_rh_hash_map.h
//
// Notes:
// stl-like hash map/hash set, with predictable performance across platforms/compilers/C run-times libs/etc.
//
// Robin Hood hashing, open addressing, hashtable contains pointers to keys/values, backshifting on erasures.
// No support for items with duplicate keys (use vogl::map instead).
#pragma once

#include "vogl_core.h"
#include "vogl_hash.h"
#include "vogl_data_stream_serializer.h"
#include "vogl_object_pool.h"

namespace vogl
{
    template <typename T>
    class heap_allocator
    {
    public:
        heap_allocator()
        {
        }

        void clear()
        {
        }

        void swap(heap_allocator &other)
        {
            VOGL_NOTE_UNUSED(other);
        }

        bool is_valid_ptr(void *p) const
        {
            return (p != NULL) && (reinterpret_cast<uintptr_t>(p) & (VOGL_MIN_ALLOC_ALIGNMENT - 1)) == 0;
        }

        T *alloc_no_construction()
        {
            return static_cast<T *>(vogl_malloc(sizeof(T)));
        }

        void destroy_no_destruction(void *p)
        {
            vogl_free(p);
        }
    };

    template <typename T>
    class pool_allocator
    {
        object_pool<T> m_pool;

    public:
        pool_allocator()
        {
        }

        void clear()
        {
            m_pool.clear();
        }

        void swap(pool_allocator &other)
        {
            m_pool.swap(other.m_pool);
        }

        bool is_valid_ptr(const T *p) const
        {
            return m_pool.is_valid_ptr(p);
        }

        T *alloc_no_construction()
        {
            return m_pool.alloc_no_construction();
        }

        void destroy_no_destruction(T *p)
        {
            m_pool.destroy_no_destruction(p);
        }
    };

    // See vogl_types.h for the default hash function and more alternatives.

    // With default template options the type should define operator size_t() (for hashing) and operator== (for equality).
    // The Key and Value objects are stored contiguously in the hash table, and will move on rehashing.
    // Iterators are invalidated on rehashing or erasing.
    // The Hasher and Equals objects must be bitwise movable (i.e. using memcpy).
    template <typename Key, typename Value = empty_type, typename Hasher = hasher<Key>, typename Equals = equal_to<Key>, typename Allocator = pool_allocator<std::pair<Key, Value> > >
    class rh_hash_map
    {
        friend class iterator;
        friend class const_iterator;

        enum
        {
            cMinHashSize = 4U
        };

    public:
        typedef rh_hash_map<Key, Value, Hasher, Equals> rh_hash_map_type;
        typedef std::pair<Key, Value> value_type;
        typedef Key key_type;
        typedef Value referent_type;
        typedef Hasher hasher_type;
        typedef Equals equals_type;
        typedef Allocator allocator_type;

        rh_hash_map()
            : m_hash_shift(32),
              m_num_valid(0),
              m_grow_threshold(0)
        {
        }

        rh_hash_map(const rh_hash_map &other)
            : m_hash_shift(32),
              m_num_valid(0),
              m_grow_threshold(0)
        {
            *this = other;
        }

        rh_hash_map &operator=(const rh_hash_map &other)
        {
            if (this == &other)
                return *this;

            clear();

            m_values.resize(other.m_values.size());

            m_hash_shift = other.m_hash_shift;
            m_grow_threshold = other.m_grow_threshold;
            m_num_valid = other.m_num_valid;
            m_hasher = other.m_hasher;
            m_equals = other.m_equals;

            const hash_entry *pSrc_entry = other.m_values.get_ptr();

            hash_entry *pDst_entry = m_values.get_ptr();
            hash_entry *pDst_entry_end = m_values.end();

            while (pDst_entry != pDst_entry_end)
            {
                if (pSrc_entry->m_pValue)
                {
                    pDst_entry->m_pValue = construct_value_type(pSrc_entry->m_pValue->first, pSrc_entry->m_pValue->second);
                    pDst_entry->m_hash = pSrc_entry->m_hash;
                }
                else
                {
                    memset(pDst_entry, 0, sizeof(hash_entry));
                }

                ++pSrc_entry;
                ++pDst_entry;
            }

            return *this;
        }

        inline ~rh_hash_map()
        {
            clear();
        }

        const Equals &get_equals() const
        {
            return m_equals;
        }
        Equals &get_equals()
        {
            return m_equals;
        }

        void set_equals(const Equals &equals)
        {
            m_equals = equals;
        }

        const Hasher &get_hasher() const
        {
            return m_hasher;
        }
        Hasher &get_hasher()
        {
            return m_hasher;
        }

        void set_hasher(const Hasher &hasher)
        {
            m_hasher = hasher;
        }

        const allocator_type &get_allocator() const
        {
            return m_allocator;
        }
        allocator_type &get_allocator()
        {
            return m_allocator;
        }

        void set_allocator(const allocator_type &alloc)
        {
            m_allocator = alloc;
        }

        // erases container, but doesn't free the allocated memory block
        inline void reset()
        {
            uint32_t num_remaining = m_num_valid;
            if (!num_remaining)
                return;

            hash_entry *p = m_values.begin();
            hash_entry *p_end = m_values.end();

            while (p != p_end)
            {
                if (p->m_pValue)
                {
                    delete_value_type(p->m_pValue);
                    p->m_pValue = NULL;

                    --num_remaining;
                    if (!num_remaining)
                        break;
                }

                ++p;
            }

            m_num_valid = 0;
        }

        inline void clear()
        {
            hash_entry *p = m_values.begin();
            hash_entry *p_end = m_values.end();

            uint32_t num_remaining = m_num_valid;
            while (p != p_end)
            {
                value_type *pValue = p->m_pValue;
                if (pValue)
                {
                    scalar_type<Key>::destruct(&pValue->first);
                    scalar_type<Value>::destruct(&pValue->second);

                    --num_remaining;
                    if (!num_remaining)
                        break;
                }

                ++p;
            }

            m_values.clear_no_destruction();

            m_hash_shift = 32;
            m_grow_threshold = 0;
            m_num_valid = 0;

            m_allocator.clear();
        }

        // Returns the number of active items in the container.
        inline uint32_t size() const
        {
            return m_num_valid;
        }

        // Returns the size of the hash table.
        inline uint32_t get_table_size() const
        {
            return m_values.size();
        }

        inline bool is_empty() const
        {
            return !m_num_valid;
        }

        // Before insertion, when the size() == get_grow_threshold() the container will rehash (double in size).
        inline uint32_t get_grow_threshold() const
        {
            return m_grow_threshold;
        }

        inline bool will_rehash_on_next_insertion() const
        {
            return m_num_valid >= m_grow_threshold;
        }

        inline void reserve(uint32_t new_capacity)
        {
            if (!new_capacity)
                return;

            uint32_t new_hash_size = new_capacity;

            new_hash_size = new_hash_size * 2U;

            if (!math::is_power_of_2(new_hash_size))
                new_hash_size = math::next_pow2(new_hash_size);

            new_hash_size = math::maximum<uint32_t>(cMinHashSize, new_hash_size);

            if (new_hash_size > m_values.size())
                rehash(new_hash_size);
        }

        class const_iterator;

        class iterator
        {
            friend class rh_hash_map<Key, Value, Hasher, Equals>;
            friend class rh_hash_map<Key, Value, Hasher, Equals>::const_iterator;

        public:
            inline iterator()
                : m_pTable(NULL), m_index(0)
            {
            }
            inline iterator(rh_hash_map_type &table, uint32_t index)
                : m_pTable(&table), m_index(index)
            {
            }
            inline iterator(const iterator &other)
                : m_pTable(other.m_pTable), m_index(other.m_index)
            {
            }

            inline iterator &operator=(const iterator &other)
            {
                m_pTable = other.m_pTable;
                m_index = other.m_index;
                return *this;
            }

            // post-increment
            inline iterator operator++(int)
            {
                iterator result(*this);
                ++*this;
                return result;
            }

            // pre-increment
            inline iterator &operator++()
            {
                probe();
                return *this;
            }

            inline value_type &operator*() const
            {
                return *get_cur();
            }
            inline value_type *operator->() const
            {
                return get_cur();
            }

            inline bool operator==(const iterator &b) const
            {
                return (m_pTable == b.m_pTable) && (m_index == b.m_index);
            }
            inline bool operator!=(const iterator &b) const
            {
                return !(*this == b);
            }
            inline bool operator==(const const_iterator &b) const
            {
                return (m_pTable == b.m_pTable) && (m_index == b.m_index);
            }
            inline bool operator!=(const const_iterator &b) const
            {
                return !(*this == b);
            }

            inline uint32_t get_index() const
            {
                return m_index;
            }

        private:
            rh_hash_map_type *m_pTable;
            uint32_t m_index;

            inline value_type *get_cur() const
            {
                VOGL_ASSERT(m_pTable && (m_index < m_pTable->m_values.size()));
                VOGL_ASSERT(m_pTable->m_values[m_index].m_pValue != NULL);
                return m_pTable->m_values[m_index].m_pValue;
            }

            inline void probe()
            {
                VOGL_ASSERT(m_pTable);
                m_index = m_pTable->find_next(m_index);
            }
        };

        class const_iterator
        {
            friend class rh_hash_map<Key, Value, Hasher, Equals>;
            friend class rh_hash_map<Key, Value, Hasher, Equals>::iterator;

        public:
            inline const_iterator()
                : m_pTable(NULL), m_index(0)
            {
            }
            inline const_iterator(const rh_hash_map_type &table, uint32_t index)
                : m_pTable(&table), m_index(index)
            {
            }
            inline const_iterator(const iterator &other)
                : m_pTable(other.m_pTable), m_index(other.m_index)
            {
            }
            inline const_iterator(const const_iterator &other)
                : m_pTable(other.m_pTable), m_index(other.m_index)
            {
            }

            inline const_iterator &operator=(const const_iterator &other)
            {
                m_pTable = other.m_pTable;
                m_index = other.m_index;
                return *this;
            }

            inline const_iterator &operator=(const iterator &other)
            {
                m_pTable = other.m_pTable;
                m_index = other.m_index;
                return *this;
            }

            // post-increment
            inline const_iterator operator++(int)
            {
                const_iterator result(*this);
                ++*this;
                return result;
            }

            // pre-increment
            inline const_iterator &operator++()
            {
                probe();
                return *this;
            }

            inline const value_type &operator*() const
            {
                return *get_cur();
            }
            inline const value_type *operator->() const
            {
                return get_cur();
            }

            inline bool operator==(const const_iterator &b) const
            {
                return (m_pTable == b.m_pTable) && (m_index == b.m_index);
            }
            inline bool operator!=(const const_iterator &b) const
            {
                return !(*this == b);
            }
            inline bool operator==(const iterator &b) const
            {
                return (m_pTable == b.m_pTable) && (m_index == b.m_index);
            }
            inline bool operator!=(const iterator &b) const
            {
                return !(*this == b);
            }

            inline uint32_t get_index() const
            {
                return m_index;
            }

        private:
            const rh_hash_map_type *m_pTable;
            uint32_t m_index;

            inline const value_type *get_cur() const
            {
                VOGL_ASSERT(m_pTable && (m_index < m_pTable->m_values.size()));
                VOGL_ASSERT(m_pTable->m_values[m_index].m_pValue != NULL);
                return m_pTable->m_values[m_index].m_pValue;
            }

            inline void probe()
            {
                VOGL_ASSERT(m_pTable);
                m_index = m_pTable->find_next(m_index);
            }
        };

        inline const_iterator begin() const
        {
            if (!m_num_valid)
                return end();

            return const_iterator(*this, find_next(-1));
        }

        inline const_iterator end() const
        {
            return const_iterator(*this, m_values.size());
        }

        inline iterator begin()
        {
            if (!m_num_valid)
                return end();

            return iterator(*this, find_next(-1));
        }

        inline iterator end()
        {
            return iterator(*this, m_values.size());
        }

        // insert_result.first will always point to inserted key/value (or the already existing key/value).
        // insert_result.second will be true if a new key/value was inserted, or false if the key already existed (in which case first will point to the already existing value).
        // insert() may grow the container. After growing, any iterators (and indices) will be invalid.
        typedef std::pair<iterator, bool> insert_result;

        inline insert_result insert(const Key &k, const Value &v = Value())
        {
            insert_result result;
            if (!insert_no_grow(result, k, v))
            {
                grow();

                // This must succeed.
                if (!insert_no_grow(result, k, v))
                {
                    VOGL_FAIL("insert() failed");
                }
            }

            return result;
        }

        inline insert_result insert(const value_type &v)
        {
            return insert(v.first, v.second);
        }

        inline bool insert_no_grow(insert_result &result, const Key &k, const Value &v = Value())
        {
            if ((!m_values.size()) || (m_num_valid >= m_grow_threshold))
                return false;

            value_type *pValue = NULL;
            uint32_t hash = hash_key(k);
            uint32_t index_init = hash >> m_hash_shift;

            const uint32_t size = m_values.size();

            uint32_t index_current = index_init & (size - 1);
            uint32_t probe_current = 0;
            while (true)
            {
                hash_entry *pEntry = &m_values[index_current];

                if (!pEntry->m_pValue)
                {
                    if (!pValue)
                    {
                        pValue = construct_value_type(k, v);

                        ++m_num_valid;
                        VOGL_ASSERT(m_num_valid <= size);

                        result.first = iterator(*this, index_current);
                        result.second = true;
                    }

                    pEntry->m_pValue = pValue;
                    pEntry->m_hash = hash;
                    break;
                }

                if ((!pValue) && (m_equals(pEntry->m_pValue->first, k)))
                {
                    result.first = iterator(*this, index_current);
                    result.second = false;
                    break;
                }

                uint32_t probe_distance = calc_distance(index_current);
                if (probe_current > probe_distance)
                {
                    if (!pValue)
                    {
                        pValue = construct_value_type(k, v);

                        ++m_num_valid;
                        VOGL_ASSERT(m_num_valid <= size);

                        result.first = iterator(*this, index_current);
                        result.second = true;
                    }

                    value_type *pTemp_value = pEntry->m_pValue;
                    uint32_t temp_hash = pEntry->m_hash;

                    pEntry->m_pValue = pValue;
                    pEntry->m_hash = hash;

                    pValue = pTemp_value;
                    hash = temp_hash;

                    probe_current = probe_distance;
                }

                if (++index_current == size)
                    index_current = 0;

                ++probe_current;
            }

            return true;
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

        // Returns index if found, or index==size() if not found.
        uint32_t find_index(const Key &k) const
        {
            uint32_t hash = hash_key(k);
            uint32_t index_init = hash >> m_hash_shift;

            const uint32_t size = m_values.size();

            for (uint32_t i = 0; i < size; ++i)
            {
                uint32_t index_current = (index_init + i) & (size - 1);
                const hash_entry *pEntry = &m_values[index_current];

                if (!pEntry->m_pValue)
                    break;

                uint32_t probe_distance = calc_distance(index_current);
                if (i > probe_distance)
                    break;

                if ((hash == pEntry->m_hash) && (m_equals(pEntry->m_pValue->first, k)))
                    return index_current;
            }

            return size;
        }

        inline const_iterator find(const Key &k) const
        {
            return const_iterator(*this, find_index(k));
        }

        inline iterator find(const Key &k)
        {
            return iterator(*this, find_index(k));
        }

        inline Value *find_value(const Key &key)
        {
            uint32_t index = find_index(key);
            if (index == m_values.size())
                return NULL;
            return &(m_values[index].m_pEntry->second);
        }

        inline const Value *find_value(const Key &key) const
        {
            uint32_t index = find_index(key);
            if (index == m_values.size())
                return NULL;
            return &(m_values[index].m_pEntry->second);
        }

        inline bool contains(const Key &key) const
        {
            return find_index(key) != m_values.size();
        }

        // All active iterators become invalid after erase().
        inline bool erase(const Key &k)
        {
            if (!m_num_valid)
                return false;

            uint32_t hash = hash_key(k);
            uint32_t index_init = hash >> m_hash_shift;

            uint32_t index_current = 0;

            const uint32_t size = m_values.size();

            for (uint32_t i = 0; i < size; ++i)
            {
                index_current = (index_init + i) & (size - 1);
                hash_entry *pEntry = &m_values[index_current];

                if (!pEntry->m_pValue)
                    return false;

                uint32_t probe_distance = calc_distance(index_current);
                if (i > probe_distance)
                    return false;

                if ((hash == pEntry->m_hash) && (m_equals(pEntry->m_pValue->first, k)))
                {
                    delete_value_type(pEntry->m_pValue);
                    --m_num_valid;

                    break;
                }
            }

            for (uint32_t i = 1; i < size; ++i)
            {
                uint32_t prev = (index_current + i - 1) & (size - 1);
                uint32_t idx = (index_current + i) & (size - 1);

                if (!m_values[idx].m_pValue)
                {
                    m_values[prev].m_pValue = NULL;
                    break;
                }

                uint32_t dist = calc_distance(idx);
                if (!dist)
                {
                    m_values[prev].m_pValue = NULL;
                    break;
                }

                m_values[prev].m_pValue = m_values[idx].m_pValue;
                m_values[prev].m_hash = m_values[idx].m_hash;
            }

            return true;
        }

        inline bool erase(const iterator &it)
        {
            uint32_t index_current = it.m_index;
            if ((index_current >= m_values.size()) || (!m_values[index_current].m_pValue))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            const uint32_t size = m_values.size();

            delete_value_type(m_values[index_current].m_pValue);
            --m_num_valid;

            for (uint32_t i = 1; i < size; ++i)
            {
                uint32_t prev = (index_current + i - 1) & (size - 1);
                uint32_t idx = (index_current + i) & (size - 1);

                if (!m_values[idx].m_pValue)
                {
                    m_values[prev].m_pValue = NULL;
                    break;
                }

                uint32_t dist = calc_distance(idx);
                if (!dist)
                {
                    m_values[prev].m_pValue = NULL;
                    break;
                }

                m_values[prev].m_pValue = m_values[idx].m_pValue;
                m_values[prev].m_hash = m_values[idx].m_hash;
            }

            return true;
        }

        inline void swap(rh_hash_map_type &other)
        {
            m_values.swap(other.m_values);
            utils::swap(m_hash_shift, other.m_hash_shift);
            utils::swap(m_num_valid, other.m_num_valid);
            utils::swap(m_grow_threshold, other.m_grow_threshold);
            utils::swap(m_hasher, other.m_hasher);
            utils::swap(m_equals, other.m_equals);
            m_allocator.swap(other.m_allocator);
        }

        // Obviously, this method is very slow! It scans the entire hash table.
        inline const_iterator search_table_for_value(const Value &val) const
        {
            for (const_iterator it = begin(); it != end(); ++it)
                if (it->second == val)
                    return it;
            return end();
        }

        // Obviously, this method is very slow! It scans the entire hash table.
        inline iterator search_table_for_value(const Value &val)
        {
            for (iterator it = begin(); it != end(); ++it)
                if (it->second == val)
                    return it;
            return end();
        }

        // Obviously, this method is very slow! It scans the entire hash table.
        inline uint32_t search_table_for_value_get_count(const Value &val) const
        {
            uint32_t count = 0;
            for (const_iterator it = begin(); it != end(); ++it)
                if (it->second == val)
                    ++count;
            return count;
        }

        bool operator==(const rh_hash_map &other) const
        {
            if (this == &other)
                return true;

            if (size() != other.size())
                return false;

            for (const_iterator it = begin(); it != end(); ++it)
            {
                const_iterator other_it(other.find(it->first));
                if (other_it == other.end())
                    return false;

                if (!(it->second == other_it->second))
                    return false;
            }

            return true;
        }

        bool operator!=(const rh_hash_map &other) const
        {
            return !(*this == other);
        }

        bool check_failure() const
        {
            return false;
        }

        bool check() const
        {
            if (!m_values.size())
            {
                if (m_hash_shift != 32)
                    return check_failure();
                if (m_num_valid)
                    return check_failure();
                if (m_grow_threshold)
                    return check_failure();
                return true;
            }

            if (m_hash_shift != (32U - math::floor_log2i(m_values.size())))
                return check_failure();

            uint32_t total_found = 0;
            uint32_t max_dist = 0;
            for (uint32_t i = 0; i < m_values.size(); i++)
            {
                if (m_values[i].m_pValue)
                {
                    if (!m_allocator.is_valid_ptr(m_values[i].m_pValue))
                        return check_failure();

                    uint32_t actual_hash = hash_key(m_values[i].m_pValue->first);
                    if (m_values[i].m_hash != actual_hash)
                        return check_failure();

                    uint32_t dist = calc_distance(i);
                    max_dist = math::maximum<uint32_t>(max_dist, dist);

                    uint32_t k = find_index(m_values[i].m_pValue->first);
                    if (k != i)
                        return check_failure();

                    ++total_found;
                }
            }

            if (total_found != m_num_valid)
                return check_failure();

            return true;
        }

        // Direct hash table low-level manipulation

        // index can be retrieved from a iterator by calling get_index()
        inline bool is_valid_index(uint32_t index) const
        {
            return (index < m_values.size()) && (m_values[index].m_pValue != NULL);
        }

        inline const value_type &value_type_at_index(uint32_t index) const
        {
            return *m_values[index].m_pValue;
        }

        inline const Key &key_at_index(uint32_t index) const
        {
            return m_values[index].m_pValue->first;
        }

        inline const Value &value_at_index(uint32_t index) const
        {
            return m_values[index].m_pValue->second;
        }

        inline Value &value_at_index(uint32_t index)
        {
            return m_values[index].m_pValue->second;
        }

    private:
        struct hash_entry
        {
            value_type *m_pValue;
            uint32_t m_hash;
        };

        typedef vogl::vector<hash_entry> hash_entry_vec;

        hash_entry_vec m_values;

        uint32_t m_hash_shift;

        uint32_t m_num_valid;

        uint32_t m_grow_threshold;

        Hasher m_hasher;
        Equals m_equals;

        allocator_type m_allocator;

        value_type *construct_value_type(const Key &k, const Value &v)
        {
            //value_type *p = static_cast<value_type *>(vogl_malloc(sizeof(value_type)));
            value_type *p = m_allocator.alloc_no_construction();

            if (VOGL_IS_BITWISE_COPYABLE(Key))
                memcpy(&p->first, &k, sizeof(Key));
            else
                scalar_type<Key>::construct(&p->first, k);

            if (VOGL_IS_BITWISE_COPYABLE(Value))
                memcpy(&p->second, &v, sizeof(Value));
            else
                scalar_type<Value>::construct(&p->second, v);

            return p;
        }

        void delete_value_type(value_type *p)
        {
            scalar_type<Key>::destruct(&p->first);
            scalar_type<Value>::destruct(&p->second);

            //vogl_free(p);
            m_allocator.destroy_no_destruction(p);
        }

        inline uint32_t calc_distance(uint32_t index_stored) const
        {
            VOGL_ASSERT(m_values[index_stored].m_pValue);

            uint32_t initial_index = m_values[index_stored].m_hash >> m_hash_shift;
            VOGL_ASSERT((index_stored < m_values.size()) && (initial_index < m_values.size()));

            uint32_t distance = index_stored - initial_index;
            if (initial_index > index_stored)
                distance += m_values.size();

            return distance;
        }

        inline uint32_t hash_key(const Key &k) const
        {
            VOGL_ASSERT((1U << (32U - m_hash_shift)) == m_values.size());

            uint32_t hash = static_cast<uint32_t>(m_hasher(k));

            // Fibonacci hashing
            return 2654435769U * hash;
        }

        inline void grow()
        {
            if (m_values.size() >= 0x80000000UL)
            {
                // FIXME: This case (ginormous arrays on x64) will die.
                VOGL_ASSERT_ALWAYS;
                return;
            }

            rehash(math::maximum<uint32_t>(cMinHashSize, m_values.size() * 2U));
        }

        inline void move_into_container(hash_entry *pEntry_to_move)
        {
            ++m_num_valid;
            VOGL_ASSERT(m_num_valid <= m_values.size());

            value_type *pValue = pEntry_to_move->m_pValue;
            uint32_t hash = pEntry_to_move->m_hash;

            uint32_t index_init = hash >> m_hash_shift;
            uint32_t probe_current = 0;

            const uint32_t size = m_values.size();

            uint32_t i;
            for (i = 0; i < size; ++i, ++probe_current)
            {
                uint32_t index_current = (index_init + i) & (m_values.size() - 1);
                hash_entry *pEntry = &m_values[index_current];

                if (!pEntry->m_pValue)
                {
                    pEntry->m_pValue = pValue;
                    pEntry->m_hash = hash;
                    break;
                }

                uint32_t probe_distance = calc_distance(index_current);
                if (probe_current > probe_distance)
                {
                    value_type *pTemp_value = pEntry->m_pValue;
                    uint32_t temp_hash = pEntry->m_hash;

                    pEntry->m_pValue = pValue;
                    pEntry->m_hash = hash;

                    pValue = pTemp_value;
                    hash = temp_hash;

                    probe_current = probe_distance;
                }
            }

            VOGL_ASSERT(i != size);
        }

        inline void rehash(uint32_t new_hash_size)
        {
            VOGL_ASSERT(new_hash_size >= m_num_valid);
            VOGL_ASSERT(math::is_power_of_2(new_hash_size));

            if ((new_hash_size < m_num_valid) || (new_hash_size == m_values.size()))
                return;

            rh_hash_map new_map;

            new_map.m_values.resize(new_hash_size);
            memset(new_map.m_values.get_ptr(), 0, new_map.m_values.size_in_bytes());

            new_map.m_hash_shift = 32U - math::floor_log2i(new_hash_size);
            VOGL_ASSERT(new_hash_size == (1U << (32U - new_map.m_hash_shift)));
            new_map.m_grow_threshold = UINT_MAX;

            // Move items from old array into new array
            hash_entry *pNode = reinterpret_cast<hash_entry *>(m_values.begin());
            hash_entry *pNode_end = pNode + m_values.size();

            while (pNode != pNode_end)
            {
                if (pNode->m_pValue)
                {
                    new_map.move_into_container(pNode);

                    if (new_map.m_num_valid == m_num_valid)
                        break;
                }

                pNode++;
            }

            // 80% load factor.
            new_map.m_grow_threshold = (new_hash_size * 4) / 5;

            // Clear old array
            m_values.clear_no_destruction();
            m_hash_shift = 32;

            m_values.swap(new_map.m_values);
            utils::swap(m_hash_shift, new_map.m_hash_shift);
            utils::swap(m_num_valid, new_map.m_num_valid);
            utils::swap(m_grow_threshold, new_map.m_grow_threshold);
            utils::swap(m_hasher, new_map.m_hasher);
            utils::swap(m_equals, new_map.m_equals);

            VOGL_ASSERT(check());
        }

        inline uint32_t find_next(uint32_t index) const
        {
            if (++index >= m_values.size())
                return index;

            const hash_entry *pEntry = &m_values[index];
            const hash_entry *pLast_entry = m_values.end();

            for (;;)
            {
                if ((pEntry->m_pValue) || (++pEntry == pLast_entry))
                    break;
            }

            return static_cast<uint32_t>(pEntry - m_values.begin());
        }
    };

    bool rh_hash_map_test();

} // namespace vogl

namespace std
{
    template <typename Key, typename Value, typename Hasher, typename Equals, typename Allocator>
    inline void swap(vogl::rh_hash_map<Key, Value, Hasher, Equals, Allocator> &a, vogl::rh_hash_map<Key, Value, Hasher, Equals, Allocator> &b)
    {
        a.swap(b);
    }
}
