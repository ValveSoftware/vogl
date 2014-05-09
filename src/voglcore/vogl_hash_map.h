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

// File: vogl_hash_map.h
//
// Notes:
// stl-like hash map/hash set, with predictable performance across platforms/compilers/C run-times libs/etc.
// Hash function ref: "Data Structures and Algorithms with Object-Oriented Design Patterns in C++"
// http://www.brpreiss.com/books/opus4/html/page215.html
//
// Compared for performance against VC9's std::hash_map.
//
// Open addressing, linear probing, auto rehashes on ~50% load factor. Uses Knuth's multiplicative method (Fibonacci hashing).
// No support for items with duplicate keys (use vogl::map instead).
#pragma once

#include "vogl_core.h"
#include "vogl_hash.h"
#include "vogl_data_stream_serializer.h"

namespace vogl
{
    // See vogl_types.h for the default hash function and more alternatives.

    // With default template options the type should define operator size_t() (for hashing) and operator== (for equality).
    // The Key and Value objects are stored contiguously in the hash table, and will move on rehashing.
    // Iterators are invalidated on rehashing or erasing.
    // The Hasher and Equals objects must be bitwise movable (i.e. using memcpy).
    template <typename Key, typename Value = empty_type, typename Hasher = hasher<Key>, typename Equals = equal_to<Key> >
    class hash_map
    {
        friend class iterator;
        friend class const_iterator;

        enum state
        {
            cStateInvalid = 0,
            cStateValid = 1
        };

        enum
        {
            cMinHashSize = 4U
        };

    public:
        typedef hash_map<Key, Value, Hasher, Equals> hash_map_type;
        typedef std::pair<Key, Value> value_type;
        typedef Key key_type;
        typedef Value referent_type;
        typedef Hasher hasher_type;
        typedef Equals equals_type;

        hash_map()
            : m_hash_shift(32), m_num_valid(0), m_grow_threshold(0)
        {
        }

        hash_map(const hash_map &other)
            : m_values(other.m_values),
              m_hash_shift(other.m_hash_shift),
              m_hasher(other.m_hasher),
              m_equals(other.m_equals),
              m_num_valid(other.m_num_valid),
              m_grow_threshold(other.m_grow_threshold)
        {
        }

        hash_map &operator=(const hash_map &other)
        {
            if (this == &other)
                return *this;

            clear();

            m_values = other.m_values;
            m_hash_shift = other.m_hash_shift;
            m_num_valid = other.m_num_valid;
            m_grow_threshold = other.m_grow_threshold;
            m_hasher = other.m_hasher;
            m_equals = other.m_equals;

            return *this;
        }

        inline ~hash_map()
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

        inline void clear()
        {
            if (!m_values.is_empty())
            {
                if (VOGL_HAS_DESTRUCTOR(Key) || VOGL_HAS_DESTRUCTOR(Value))
                {
                    node *p = &get_node(0);
                    node *p_end = p + m_values.size();

                    uint32_t num_remaining = m_num_valid;
                    while (p != p_end)
                    {
                        if (p->state)
                        {
                            destruct_value_type(p);
                            num_remaining--;
                            if (!num_remaining)
                                break;
                        }

                        p++;
                    }
                }

                m_values.clear_no_destruction();

                m_hash_shift = 32;
                m_num_valid = 0;
                m_grow_threshold = 0;
            }
        }

        // erases container, but doesn't free the allocated memory block
        inline void reset()
        {
            if (!m_num_valid)
                return;

            if (VOGL_HAS_DESTRUCTOR(Key) || VOGL_HAS_DESTRUCTOR(Value))
            {
                node *p = &get_node(0);
                node *p_end = p + m_values.size();

                uint32_t num_remaining = m_num_valid;
                while (p != p_end)
                {
                    if (p->state)
                    {
                        destruct_value_type(p);
                        p->state = cStateInvalid;

                        num_remaining--;
                        if (!num_remaining)
                            break;
                    }

                    p++;
                }
            }
            else if (sizeof(node) <= 32)
            {
                memset(&m_values[0], 0, m_values.size_in_bytes());
            }
            else
            {
                node *p = &get_node(0);
                node *p_end = p + m_values.size();

                uint32_t num_remaining = m_num_valid;
                while (p != p_end)
                {
                    if (p->state)
                    {
                        p->state = cStateInvalid;

                        num_remaining--;
                        if (!num_remaining)
                            break;
                    }

                    p++;
                }
            }

            m_num_valid = 0;
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
            friend class hash_map<Key, Value, Hasher, Equals>;
            friend class hash_map<Key, Value, Hasher, Equals>::const_iterator;

        public:
            inline iterator()
                : m_pTable(NULL), m_index(0)
            {
            }
            inline iterator(hash_map_type &table, uint32_t index)
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
            hash_map_type *m_pTable;
            uint32_t m_index;

            inline value_type *get_cur() const
            {
                VOGL_ASSERT(m_pTable && (m_index < m_pTable->m_values.size()));
                VOGL_ASSERT(m_pTable->get_node_state(m_index) == cStateValid);

                return &m_pTable->get_node(m_index);
            }

            inline void probe()
            {
                VOGL_ASSERT(m_pTable);
                m_index = m_pTable->find_next(m_index);
            }
        };

        class const_iterator
        {
            friend class hash_map<Key, Value, Hasher, Equals>;
            friend class hash_map<Key, Value, Hasher, Equals>::iterator;

        public:
            inline const_iterator()
                : m_pTable(NULL), m_index(0)
            {
            }
            inline const_iterator(const hash_map_type &table, uint32_t index)
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
            const hash_map_type *m_pTable;
            uint32_t m_index;

            inline const value_type *get_cur() const
            {
                VOGL_ASSERT(m_pTable && (m_index < m_pTable->m_values.size()));
                VOGL_ASSERT(m_pTable->get_node_state(m_index) == cStateValid);

                return &m_pTable->get_node(m_index);
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
            if (!m_values.size())
                return false;

            int index = hash_key(k);
            node *pNode = &get_node(index);

            if (pNode->state)
            {
                if (m_equals(pNode->first, k))
                {
                    result.first = iterator(*this, index);
                    result.second = false;
                    return true;
                }

                const int orig_index = index;

                for (;;)
                {
                    if (!index)
                    {
                        index = m_values.size() - 1;
                        pNode = &get_node(index);
                    }
                    else
                    {
                        index--;
                        pNode--;
                    }

                    if (orig_index == index)
                        return false;

                    if (!pNode->state)
                        break;

                    if (m_equals(pNode->first, k))
                    {
                        result.first = iterator(*this, index);
                        result.second = false;
                        return true;
                    }
                }
            }

            if (m_num_valid >= m_grow_threshold)
                return false;

            construct_value_type(pNode, k, v);

            pNode->state = cStateValid;

            m_num_valid++;
            VOGL_ASSERT(m_num_valid <= m_values.size());

            result.first = iterator(*this, index);
            result.second = true;

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
            return &(get_node(index).second);
        }

        inline const Value *find_value(const Key &key) const
        {
            uint32_t index = find_index(key);
            if (index == m_values.size())
                return NULL;
            return &(get_node(index).second);
        }

        inline bool contains(const Key &key) const
        {
            return find_index(key) != m_values.size();
        }

        // All active iterators become invalid after erase().
        inline bool erase(const Key &k)
        {
            int i = find_index(k);

            if (i >= static_cast<int>(m_values.size()))
                return false;

            node *pDst = &get_node(i);
            destruct_value_type(pDst);
            pDst->state = cStateInvalid;

            m_num_valid--;

            for (;;)
            {
                int r, j = i;

                node *pSrc = pDst;

                do
                {
                    if (!i)
                    {
                        i = m_values.size() - 1;
                        pSrc = &get_node(i);
                    }
                    else
                    {
                        i--;
                        pSrc--;
                    }

                    if (!pSrc->state)
                        return true;

                    r = hash_key(pSrc->first);

                } while ((i <= r && r < j) || (r < j && j < i) || (j < i && i <= r));

                move_node(pDst, pSrc);

                pDst = pSrc;
            }
        }

        inline void swap(hash_map_type &other)
        {
            m_values.swap(other.m_values);
            utils::swap(m_hash_shift, other.m_hash_shift);
            utils::swap(m_num_valid, other.m_num_valid);
            utils::swap(m_grow_threshold, other.m_grow_threshold);
            utils::swap(m_hasher, other.m_hasher);
            utils::swap(m_equals, other.m_equals);
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

        bool operator==(const hash_map &other) const
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

        bool operator!=(const hash_map &other) const
        {
            return !(*this == other);
        }

        // Direct hash table low-level manipulation

        // index can be retrieved from a iterator by calling get_index()
        inline bool is_valid_index(uint32_t index) const
        {
            return (index < m_values.size()) && (get_node(index).state != cStateInvalid);
        }

        inline const value_type &value_type_at_index(uint32_t index) const
        {
            return get_node(index).value_type;
        }

        inline const Key &key_at_index(uint32_t index) const
        {
            return get_node(index).value_type.first;
        }

        inline const Value &value_at_index(uint32_t index) const
        {
            return get_node(index).value_type.first;
        }

        inline Value &value_at_index(uint32_t index)
        {
            return get_node(index).value_type.first;
        }

        // Returns index if found, or index==size() if not found.
        inline uint32_t find_index(const Key &k) const
        {
            if (m_num_valid)
            {
                int index = hash_key(k);
                const node *pNode = &get_node(index);

                if (pNode->state)
                {
                    if (m_equals(pNode->first, k))
                        return index;

                    const int orig_index = index;

                    for (;;)
                    {
                        if (!index)
                        {
                            index = m_values.size() - 1;
                            pNode = &get_node(index);
                        }
                        else
                        {
                            index--;
                            pNode--;
                        }

                        if (index == orig_index)
                            break;

                        if (!pNode->state)
                            break;

                        if (m_equals(pNode->first, k))
                            return index;
                    }
                }
            }

            return m_values.size();
        }

        // This method serializes out the *entire* hash table (including unused nodes), so the key/value must be POD's with no ptrs, etc.
        bool raw_write_to_serializer(data_stream_serializer &serializer)
        {
            uint32_t size = m_values.size();
            uint32_t inv_size = ~size;

            serializer << size << m_hash_shift << m_num_valid << m_grow_threshold << static_cast<uint32_t>(sizeof(node)) << inv_size;

            if (m_values.size())
                serializer.write(m_values.get_ptr(), m_values.size_in_bytes());

            return !serializer.get_error();
        }

        uint64_t get_raw_write_size() const
        {
            return sizeof(uint32_t) * 6 + m_values.size_in_bytes();
        }

        // key/value must be POD's with no ptrs, it serializes the raw data.
        bool raw_read_from_serializer(data_stream_serializer &serializer)
        {
            uint32_t size = serializer.read_uint32();
            uint32_t hash_shift = serializer.read_uint32();
            uint32_t num_valid = serializer.read_uint32();
            uint32_t grow_threshold = serializer.read_uint32();
            uint32_t node_size = serializer.read_uint32();
            uint32_t inv_size = serializer.read_uint32();

            if ((size != ~inv_size) || (node_size != sizeof(node)))
                return false;

            if (size != m_values.size())
            {
                if (!m_values.try_resize(size))
                    return false;
            }

            if (size)
            {
                if (!serializer.read(m_values.get_ptr(), m_values.size_in_bytes()))
                    return false;
            }

            m_hash_shift = hash_shift;
            m_num_valid = num_valid;
            m_grow_threshold = grow_threshold;

            return true;
        }

    private:
        struct node : public value_type
        {
            uint8_t state;
        };

        static inline void construct_value_type(value_type *pDst, const Key &k, const Value &v)
        {
            if (VOGL_IS_BITWISE_COPYABLE(Key))
                memcpy(&pDst->first, &k, sizeof(Key));
            else
                scalar_type<Key>::construct(&pDst->first, k);

            if (VOGL_IS_BITWISE_COPYABLE(Value))
                memcpy(&pDst->second, &v, sizeof(Value));
            else
                scalar_type<Value>::construct(&pDst->second, v);
        }

        static inline void construct_value_type(value_type *pDst, const value_type *pSrc)
        {
            if ((VOGL_IS_BITWISE_COPYABLE(Key)) && (VOGL_IS_BITWISE_COPYABLE(Value)))
            {
                memcpy(pDst, pSrc, sizeof(value_type));
            }
            else
            {
                if (VOGL_IS_BITWISE_COPYABLE(Key))
                    memcpy(&pDst->first, &pSrc->first, sizeof(Key));
                else
                    scalar_type<Key>::construct(&pDst->first, pSrc->first);

                if (VOGL_IS_BITWISE_COPYABLE(Value))
                    memcpy(&pDst->second, &pSrc->second, sizeof(Value));
                else
                    scalar_type<Value>::construct(&pDst->second, pSrc->second);
            }
        }

        static inline void destruct_value_type(value_type *p)
        {
            scalar_type<Key>::destruct(&p->first);
            scalar_type<Value>::destruct(&p->second);
        }

        // Moves *pSrc to *pDst efficiently.
        // pDst should NOT be constructed on entry.
        static inline void move_node(node *pDst, node *pSrc)
        {
            VOGL_ASSERT(!pDst->state);

            if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(Key) && VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(Value))
            {
                memcpy(pDst, pSrc, sizeof(node));
            }
            else
            {
                if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(Key))
                    memcpy(&pDst->first, &pSrc->first, sizeof(Key));
                else
                {
                    scalar_type<Key>::construct(&pDst->first, pSrc->first);
                    scalar_type<Key>::destruct(&pSrc->first);
                }

                if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(Value))
                    memcpy(&pDst->second, &pSrc->second, sizeof(Value));
                else
                {
                    scalar_type<Value>::construct(&pDst->second, pSrc->second);
                    scalar_type<Value>::destruct(&pSrc->second);
                }

                pDst->state = cStateValid;
            }

            pSrc->state = cStateInvalid;
        }

        struct raw_node
        {
            inline raw_node()
            {
                node *p = reinterpret_cast<node *>(this);
                p->state = cStateInvalid;
            }

            inline ~raw_node()
            {
                node *p = reinterpret_cast<node *>(this);
                if (p->state)
                    hash_map_type::destruct_value_type(p);
            }

            inline raw_node(const raw_node &other)
            {
                node *pDst = reinterpret_cast<node *>(this);
                const node *pSrc = reinterpret_cast<const node *>(&other);

                if (pSrc->state)
                {
                    hash_map_type::construct_value_type(pDst, pSrc);
                    pDst->state = cStateValid;
                }
                else
                    pDst->state = cStateInvalid;
            }

            inline raw_node &operator=(const raw_node &rhs)
            {
                if (this == &rhs)
                    return *this;

                node *pDst = reinterpret_cast<node *>(this);
                const node *pSrc = reinterpret_cast<const node *>(&rhs);

                if (pSrc->state)
                {
                    if (pDst->state)
                    {
                        pDst->first = pSrc->first;
                        pDst->second = pSrc->second;
                    }
                    else
                    {
                        hash_map_type::construct_value_type(pDst, pSrc);
                        pDst->state = cStateValid;
                    }
                }
                else if (pDst->state)
                {
                    hash_map_type::destruct_value_type(pDst);
                    pDst->state = cStateInvalid;
                }

                return *this;
            }

            uint8_t m_bits[sizeof(node)];
        };

        typedef vogl::vector<raw_node> node_vector;

        node_vector m_values;
        uint32_t m_hash_shift;

        Hasher m_hasher;
        Equals m_equals;

        uint32_t m_num_valid;

        uint32_t m_grow_threshold;

        inline int hash_key(const Key &k) const
        {
            VOGL_ASSERT((1U << (32U - m_hash_shift)) == m_values.size());

            uint32_t hash = static_cast<uint32_t>(m_hasher(k));

            // Fibonacci hashing
            hash = (2654435769U * hash) >> m_hash_shift;

            VOGL_ASSERT(hash < m_values.size());
            return hash;
        }

        inline const node &get_node(uint32_t index) const
        {
            return *reinterpret_cast<const node *>(&m_values[index]);
        }

        inline node &get_node(uint32_t index)
        {
            return *reinterpret_cast<node *>(&m_values[index]);
        }

        inline state get_node_state(uint32_t index) const
        {
            return static_cast<state>(get_node(index).state);
        }

        inline void set_node_state(uint32_t index, bool valid)
        {
            get_node(index).state = valid;
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

        inline void rehash(uint32_t new_hash_size)
        {
            VOGL_ASSERT(new_hash_size >= m_num_valid);
            VOGL_ASSERT(math::is_power_of_2(new_hash_size));

            if ((new_hash_size < m_num_valid) || (new_hash_size == m_values.size()))
                return;

            hash_map new_map;
            new_map.m_values.resize(new_hash_size);
            new_map.m_hash_shift = 32U - math::floor_log2i(new_hash_size);
            VOGL_ASSERT(new_hash_size == (1U << (32U - new_map.m_hash_shift)));
            new_map.m_grow_threshold = UINT_MAX;

            // Move items from old array into new array
            node *pNode = reinterpret_cast<node *>(m_values.begin());
            node *pNode_end = pNode + m_values.size();

            while (pNode != pNode_end)
            {
                if (pNode->state)
                {
                    new_map.move_into(pNode);

                    if (new_map.m_num_valid == m_num_valid)
                        break;
                }

                pNode++;
            }

            // Account for 50% target load factor
            new_map.m_grow_threshold = (new_hash_size + 1U) >> 1U;

            // Clear old array
            m_values.clear_no_destruction();
            m_hash_shift = 32;

            swap(new_map);
        }

        inline uint32_t find_next(int index) const
        {
            index++;

            if (index >= static_cast<int>(m_values.size()))
                return index;

            const node *pNode = &get_node(index);

            for (;;)
            {
                if (pNode->state)
                    break;

                if (++index >= static_cast<int>(m_values.size()))
                    break;

                pNode++;
            }

            return index;
        }

        inline void move_into(node *pNode)
        {
            int index = hash_key(pNode->first);
            node *pDst_node = &get_node(index);

            if (pDst_node->state)
            {
                const int orig_index = index;

                for (;;)
                {
                    if (!index)
                    {
                        index = m_values.size() - 1;
                        pDst_node = &get_node(index);
                    }
                    else
                    {
                        index--;
                        pDst_node--;
                    }

                    if (index == orig_index)
                    {
                        VOGL_ASSERT(false);
                        return;
                    }

                    if (!pDst_node->state)
                        break;
                }
            }

            move_node(pDst_node, pNode);

            m_num_valid++;
        }
    };

    template <typename Key, typename Value, typename Hasher, typename Equals>
    struct bitwise_movable<hash_map<Key, Value, Hasher, Equals> >
    {
        enum
        {
            cFlag = true
        };
    };

    template <typename Key, typename Value, typename Hasher, typename Equals>
    inline void swap(hash_map<Key, Value, Hasher, Equals> &a, hash_map<Key, Value, Hasher, Equals> &b)
    {
        a.swap(b);
    }

    bool hash_map_test();

} // namespace vogl

namespace std
{
    template <typename Key, typename Value, typename Hasher, typename Equals>
    inline void swap(vogl::hash_map<Key, Value, Hasher, Equals> &a, vogl::hash_map<Key, Value, Hasher, Equals> &b)
    {
        a.swap(b);
    }
}
