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

// File: vogl_object_pool.h
// vogl_object_pool is a straightforward freelist class.
// It's fast, but it's very greedy. It never frees allocated blocks unless you explictly call clear() (fast) or free_unused_blocks() (not as fast if some blocks are still allocated).
#pragma once

#include "vogl_core.h"
#include "vogl_console.h"

// Set to 1 to enable allocation debugging using rmalloc.c (much slower)
#ifndef VOGL_OBJECT_POOL_DEBUGGING
#define VOGL_OBJECT_POOL_DEBUGGING 1
#endif

#if VOGL_OBJECT_POOL_DEBUGGING
//   #pragma message("vogl_object_pool.h: Object pool debugging enabled")
#endif

namespace vogl
{

#define CHECK_FAILURE check_failure(__LINE__);

class object_pool_no_lock_policy
{
public:
    void lock() const
    {
    }

    void unlock() const
    {
    }
};

class object_pool_spinlock_locking_policy
{
    mutable spinlock m_lock;

public:
    void lock() const
    {
        m_lock.lock();
    }

    void unlock() const
    {
        m_lock.unlock();
    }
};

class object_pool_mutex_locking_policy
{
    mutable mutex m_lock;

public:
    void lock() const
    {
        m_lock.lock();
    }

    void unlock() const
    {
        m_lock.unlock();
    }
};

enum object_pool_flags
{
    cObjectPoolGrowExponential = 1,
    cObjectPoolClearOnDestruction = 2
};

template <typename T, typename LockingPolicy = object_pool_no_lock_policy>
class object_pool : public LockingPolicy
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(object_pool);

    typedef object_pool<T, LockingPolicy> base;

    class scoped_lock
    {
        const object_pool &m_pool;

    public:
        scoped_lock(const object_pool &pool)
            : m_pool(pool)
        {
            pool.lock();
        }
        ~scoped_lock()
        {
            m_pool.unlock();
        }
    };

public:
    object_pool(size_t grow_size = 0, uint32_t flags = cObjectPoolGrowExponential | cObjectPoolClearOnDestruction)
        : m_flags(flags),
          m_grow_size(grow_size),
          m_next_grow_size(math::maximum<size_t>(1U, grow_size)),
          m_total_blocks(0),
          m_total_heap_bytes(0),
          m_pFirst_free_node(NULL),
          m_total_nodes(0),
          m_total_free_nodes(0),
          m_update_flag(false)
    {
        utils::zero_object(m_blocks);
        m_blocks.m_pPrev = &m_blocks;
        m_blocks.m_pNext = &m_blocks;
    }

    ~object_pool()
    {
        if (m_flags & cObjectPoolClearOnDestruction)
            clear();
    }

    inline T *alloc()
    {
        node *pNode = alloc_node();
        T *pObj = pNode->get_object_ptr();
        scalar_type<T>::construct(pObj);
        return pObj;
    }

    template <typename U>
    inline T *alloc(const U &a)
    {
        node *pNode = alloc_node();
        T *pObj = pNode->get_object_ptr();
        scalar_type<T>::construct(pObj, a);
        return pObj;
    }

    template <typename U, typename V>
    inline T *alloc(const U &a, const V &b)
    {
        node *pNode = alloc_node();
        T *pObj = pNode->get_object_ptr();
        new (static_cast<void *>(pObj)) T(a, b);
        return pObj;
    }

    template <typename U, typename V, typename W>
    inline T *alloc(const U &a, const V &b, const W &c)
    {
        node *pNode = alloc_node();
        T *pObj = pNode->get_object_ptr();
        new (static_cast<void *>(pObj)) T(a, b, c);
        return pObj;
    }

    template <typename U, typename V, typename W, typename X>
    inline T *alloc(const U &a, const V &b, const W &c, const X &d)
    {
        node *pNode = alloc_node();
        T *pObj = pNode->get_object_ptr();
        new (static_cast<void *>(pObj)) T(a, b, c, d);
        return pObj;
    }

    template <typename U, typename V, typename W, typename X, typename Y>
    inline T *alloc(const U &a, const V &b, const W &c, const X &d, const Y &e)
    {
        node *pNode = alloc_node();
        T *pObj = pNode->get_object_ptr();
        new (static_cast<void *>(pObj)) T(a, b, c, d, e);
        return pObj;
    }

    inline T *alloc_no_construction()
    {
        node *pNode = alloc_node();
        return pNode->get_object_ptr();
    }

    inline void destroy_no_destruction(T *p)
    {
        VOGL_ASSERT(!m_update_flag);
        VOGL_ASSERT(m_total_blocks);

        if (!p)
            return;

        node *pNode = reinterpret_cast<node *>(reinterpret_cast<uint8_t *>(p) - static_cast<int>(VOGL_OFFSETOF(node, m_obj)));

#if VOGL_OBJECT_POOL_DEBUGGING
        VOGL_ASSERT(pNode->m_marker == cUsedNodeMarker);
        pNode->m_marker = cFreeNodeMarker;
#endif

        LockingPolicy::lock();

        pNode->m_pNext = m_pFirst_free_node;
        m_pFirst_free_node = pNode;

        ++m_total_free_nodes;

        LockingPolicy::unlock();
    }

    inline void destroy(T *p)
    {
        if (p)
        {
            scalar_type<T>::destruct(p);
            destroy_no_destruction(p);
        }
    }

    // This frees all allocated memory, but does NOT destroy all objects. That's the caller's responsibility.
    size_t clear()
    {
        scoped_lock lock(*this);
        return clear_internal();
    }

    // TODO: until vogl::vector supports size_t sizes there are some ugly casts in here.
    // This is a lot more complex than I originally thought it would be, and we're probably not going to need it.
    // If the free lists were hung off each block this would be much simpler, but that would require more node overhead (or more costly frees).
    size_t free_unused_blocks()
    {
        scoped_lock lock(*this);

        VOGL_ASSERT(!m_update_flag);

        if (!m_total_free_nodes)
            return 0;

        if (m_total_free_nodes == m_total_nodes)
            return clear_internal();

        if ((m_total_free_nodes > cUINT32_MAX) || (m_total_nodes > cUINT32_MAX) || (m_total_blocks > cUINT32_MAX))
        {
            VOGL_ASSERT_ALWAYS;
            return 0;
        }

        m_update_flag = true;

        vogl::vector<node *> free_nodes;
        free_nodes.reserve((uint32_t)m_total_free_nodes);

        // Put all freelist nodes into the free_nodes array and sort it by pointer address
        node *pCur_node = m_pFirst_free_node;
        while (pCur_node)
        {
#if VOGL_OBJECT_POOL_DEBUGGING
            if (pCur_node->m_marker != cFreeNodeMarker)
            {
                CHECK_FAILURE;
                m_update_flag = false;
                return 0;
            }
#endif
            free_nodes.push_back(pCur_node);
            pCur_node = pCur_node->m_pNext;
        }

        free_nodes.sort();

        VOGL_ASSERT(free_nodes.size() == m_total_free_nodes);

        // Now put all block pointers into the blocks array and sort it by address
        vogl::vector<block *> blocks;
        blocks.reserve((uint32_t)m_total_blocks);

        block *pCur_block = m_blocks.m_pNext;
        while (pCur_block != &m_blocks)
        {
            blocks.push_back(pCur_block);
            pCur_block = pCur_block->m_pNext;
        }

        VOGL_ASSERT(blocks.size() == m_total_blocks);

        blocks.sort();

        // Now find which block each free node pointer belongs in, and keep a tally of the # of free nodes we've found in each block (along with the first block index for each block)
        vogl::vector<uint32_t> free_nodes_in_block(blocks.size());
        vogl::vector<uint32_t> first_node_indices(blocks.size());

        int prev_block_index = -1;

        for (uint32_t i = 0; i < free_nodes.size(); i++)
        {
            node *pNode = free_nodes[i];

            if (prev_block_index >= 0)
            {
                node *pFirst_node = blocks[prev_block_index]->get_first_node_ptr();
                node *pEnd_node = blocks[prev_block_index]->get_end_node_ptr();

                if ((pNode >= pFirst_node) && (pNode < pEnd_node))
                {
                    free_nodes_in_block[prev_block_index]++;
                    continue;
                }
            }

            // Binary search to find which block this node belongs in
            int l = 0, h = blocks.size() - 1;
            while (l <= h)
            {
                int m = l + ((h - l) >> 1);
                node *pFirst_node = blocks[m]->get_first_node_ptr();
                node *pEnd_node = blocks[m]->get_end_node_ptr();

                if ((pNode >= pFirst_node) && (pNode < pEnd_node))
                {
                    // We've found which block this node belongs to. See if all nodes in this block are free.
                    first_node_indices[m] = i;
                    free_nodes_in_block[m]++;
                    prev_block_index = m;

                    size_t n = blocks[m]->m_total_nodes;
                    if ((n > 1) && ((i + n - 1) < free_nodes.size()))
                    {
                        pNode = free_nodes[(uint32_t)(i + n - 1)];
                        if (pNode == (pEnd_node - 1))
                        {
                            free_nodes_in_block[m] = (uint32_t)n;
                            i += safe_int_cast<uint32_t>(n - 1);
                        }
                    }

                    break;
                }
                else if (pNode >= pEnd_node)
                    l = m + 1;
                else
                    h = m - 1;
            }

            if (l > h)
            {
                CHECK_FAILURE;
                m_update_flag = false;
                return 0;
            }
        }

        // Now loop through each block and free the ones that have all-freed nodes
        size_t total_nodes_freed = 0;
        size_t total_blocks_freed = 0;
        size_t total_bytes_freed = 0;

        size_t prev_highwater_free_node_index = 0;

        for (uint32_t block_index = 0; block_index < blocks.size(); ++block_index)
        {
            block *pBlock = blocks[block_index];

            if (pBlock->m_total_nodes != free_nodes_in_block[block_index])
                continue;

            size_t total_nodes = pBlock->m_total_nodes;

            VOGL_ASSERT(m_total_nodes >= total_nodes);
            m_total_nodes -= total_nodes;

            VOGL_ASSERT(m_total_free_nodes >= total_nodes);
            m_total_free_nodes -= total_nodes;

            VOGL_ASSERT(m_total_blocks);
            m_total_blocks--;

            size_t block_size_in_bytes = vogl_msize(pBlock);
            total_bytes_freed += block_size_in_bytes;

            VOGL_ASSERT(m_total_heap_bytes >= block_size_in_bytes);
            m_total_heap_bytes -= block_size_in_bytes;

            total_nodes_freed += total_nodes;

            uint32_t first_node_index = first_node_indices[block_index];

            VOGL_ASSERT(first_node_index >= prev_highwater_free_node_index);

            if (!total_blocks_freed)
                m_pFirst_free_node = NULL;

            // Put any free nodes before the ones we're removing back into the freelist
            // TODO: This just puts the nodes back in any old order. This should prioritize the freelist order by those blocks which are must occupied.
            size_t n = first_node_index - prev_highwater_free_node_index;
            for (size_t i = 0; i < n; i++)
            {
                node *pFree_node = free_nodes[(uint32_t)(prev_highwater_free_node_index + i)];
                pFree_node->m_pNext = m_pFirst_free_node;
                m_pFirst_free_node = pFree_node;
            }

            prev_highwater_free_node_index = first_node_index + total_nodes;

            vogl_free(pBlock);

            total_blocks_freed++;

            blocks[block_index] = NULL;
        }

        if (!total_blocks_freed)
        {
            m_update_flag = false;
            return 0;
        }

        if (prev_highwater_free_node_index != free_nodes.size())
        {
            // Put any remaining free nodes before the ones we're removing back into the freelist
            size_t n = free_nodes.size() - prev_highwater_free_node_index;
            for (size_t i = 0; i < n; i++)
            {
                node *pFree_node = free_nodes[(uint32_t)(prev_highwater_free_node_index + i)];
                pFree_node->m_pNext = m_pFirst_free_node;
                m_pFirst_free_node = pFree_node;
            }
        }

        m_blocks.m_pPrev = &m_blocks;
        m_blocks.m_pNext = &m_blocks;

        if (m_total_blocks)
        {
            // Now fix up the block list
            for (uint32_t block_index = 0; block_index < blocks.size(); ++block_index)
            {
                block *pBlock = blocks[block_index];
                if (pBlock)
                {
                    pBlock->m_pPrev = &m_blocks;
                    pBlock->m_pNext = m_blocks.m_pNext;

                    m_blocks.m_pNext->m_pPrev = pBlock;
                    m_blocks.m_pNext = pBlock;
                }
            }
        }

        m_update_flag = false;

        return total_bytes_freed;
    }

    bool is_valid_ptr(const T *p, bool check_freelist = false) const
    {
        if (!p)
            return false;

        scoped_lock lock(*this);

        block *pCur_block = m_blocks.m_pNext;
        while (pCur_block != &m_blocks)
        {
            if (pCur_block->m_begin_marker != cBlockBeginMarker)
            {
                CHECK_FAILURE;
                return false;
            }

            if (pCur_block->m_end_marker != cBlockEndMarker)
            {
                CHECK_FAILURE;
                return false;
            }

            if (!pCur_block->m_total_nodes)
            {
                CHECK_FAILURE;
                return false;
            }

            if ((p >= pCur_block->get_first_node_ptr()->get_object_ptr()) && (p <= (pCur_block->get_first_node_ptr() + pCur_block->m_total_nodes - 1)->get_object_ptr()))
            {
                std::ptrdiff_t ofs = reinterpret_cast<const uint8_t *>(p) - reinterpret_cast<uint8_t *>(pCur_block->get_first_node_ptr()->get_object_ptr());
                if (ofs % sizeof(node))
                {
                    CHECK_FAILURE;
                    return false;
                }

                if (check_freelist)
                {
                    node *pCur_node = m_pFirst_free_node;
                    while (pCur_node)
                    {
                        if (p == pCur_node->get_object_ptr())
                        {
                            CHECK_FAILURE;
                            return false;
                        }
                        pCur_node = pCur_node->m_pNext;
                    }
                }
                return true;
            }

            pCur_block = pCur_block->m_pNext;
        }

        return false;
    }

    bool check()
    {
        scoped_lock lock(*this);

        VOGL_ASSERT(!m_update_flag);

        if (m_total_blocks > static_cast<size_t>(cINT32_MAX))
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        m_update_flag = true;

        bool success = true;

        size_t total_blocks_found = 0;
        size_t total_heap_bytes_found = 0;

        size_t total_nodes_found = 0;

        vogl::vector<block *> blocks;
        blocks.reserve(static_cast<uint32_t>(m_total_blocks));

        block *pCur_block = m_blocks.m_pNext;
        while (pCur_block != &m_blocks)
        {
            if (pCur_block->m_begin_marker != cBlockBeginMarker)
            {
                CHECK_FAILURE;
                success = false;
                break;
            }

            if (pCur_block->m_end_marker != cBlockEndMarker)
            {
                CHECK_FAILURE;
                success = false;
                break;
            }

            if (!pCur_block->m_total_nodes)
            {
                CHECK_FAILURE;
                success = false;
                break;
            }

            size_t block_heap_size = vogl_msize(pCur_block);
            if (block_heap_size < (sizeof(block) + pCur_block->m_total_nodes * sizeof(node)))
            {
                CHECK_FAILURE;
                success = false;
                break;
            }

            total_heap_bytes_found += block_heap_size;

            total_blocks_found++;
            if (total_blocks_found > m_total_blocks)
            {
                CHECK_FAILURE;
                success = false;
                break;
            }

            total_nodes_found += pCur_block->m_total_nodes;

            blocks.push_back(pCur_block);

            pCur_block = pCur_block->m_pNext;
        }

        blocks.sort();

        if (!success)
        {
            m_update_flag = false;
            return false;
        }

        if (total_blocks_found != m_total_blocks)
        {
            CHECK_FAILURE;
            success = false;
        }

        if (total_heap_bytes_found != m_total_heap_bytes)
        {
            CHECK_FAILURE;
            success = false;
        }

        size_t total_free_nodes_found = 0;

        node *pCur_node = m_pFirst_free_node;
        while (pCur_node)
        {
#if VOGL_OBJECT_POOL_DEBUGGING
            if (pCur_node->m_marker != cFreeNodeMarker)
            {
                CHECK_FAILURE;
                success = false;
                break;
            }
#endif

            total_free_nodes_found++;
            if (total_free_nodes_found > total_nodes_found)
            {
                CHECK_FAILURE;
                success = false;
                break;
            }

            int l = 0, h = blocks.size() - 1;
            while (l <= h)
            {
                int m = l + ((h - l) >> 1);
                node *pFirst_node = blocks[m]->get_first_node_ptr();
                node *pEnd_node = blocks[m]->get_end_node_ptr();

                if ((pCur_node >= pFirst_node) && (pCur_node < pEnd_node))
                    break;
                else if (pCur_node >= pEnd_node)
                    l = m + 1;
                else
                    h = m - 1;
            }

            if (l > h)
            {
                CHECK_FAILURE;
                success = false;
                break;
            }

            pCur_node = pCur_node->m_pNext;
        }

        if (total_free_nodes_found != m_total_free_nodes)
        {
            CHECK_FAILURE;
            success = false;
        }

        m_update_flag = false;

        return success;
    }

    uint32_t get_flags() const
    {
        return m_flags;
    }
    size_t get_grow_size() const
    {
        return m_grow_size;
    }
    size_t get_total_blocks() const
    {
        return m_total_blocks;
    }
    size_t get_total_heap_bytes() const
    {
        return m_total_heap_bytes;
    }
    size_t get_total_nodes() const
    {
        return m_total_nodes;
    }
    size_t get_total_free_nodes() const
    {
        return m_total_free_nodes;
    }
    size_t get_total_used_nodes() const
    {
        return m_total_nodes - m_total_free_nodes;
    }

    void swap(object_pool &other)
    {
        std::swap(m_flags, other.m_flags);
        std::swap(m_grow_size, other.m_grow_size);

        std::swap(m_next_grow_size, other.m_next_grow_size);

        std::swap(m_total_blocks, other.m_total_blocks);
        std::swap(m_total_heap_bytes, other.m_total_heap_bytes);

        std::swap(m_pFirst_free_node, other.m_pFirst_free_node);
        std::swap(m_total_nodes, other.m_total_nodes);
        std::swap(m_total_free_nodes, other.m_total_free_nodes);

        std::swap(m_update_flag, other.m_update_flag);

        // Now swap m_blocks and other.m_blocks, which is tricky because nodes can point into either.
        block temp_blocks(other.m_blocks);

        // move m_blocks into other.m_blocks
        if (m_blocks.m_pNext == &m_blocks)
            other.m_blocks.m_pNext = &other.m_blocks;
        else
        {
            other.m_blocks.m_pNext = m_blocks.m_pNext;
            VOGL_ASSERT(m_blocks.m_pNext->m_pPrev == &m_blocks);
            m_blocks.m_pNext->m_pPrev = &other.m_blocks;
        }

        if (m_blocks.m_pPrev == &m_blocks)
            other.m_blocks.m_pPrev = &other.m_blocks;
        else
        {
            other.m_blocks.m_pPrev = m_blocks.m_pPrev;
            VOGL_ASSERT(m_blocks.m_pPrev->m_pNext == &m_blocks);
            m_blocks.m_pPrev->m_pNext = &other.m_blocks;
        }

        // move other.m_blocks into m_blocks
        if (temp_blocks.m_pNext == &other.m_blocks)
            m_blocks.m_pNext = &m_blocks;
        else
        {
            m_blocks.m_pNext = temp_blocks.m_pNext;
            VOGL_ASSERT(temp_blocks.m_pNext->m_pPrev == &other.m_blocks);
            temp_blocks.m_pNext->m_pPrev = &m_blocks;
        }

        if (temp_blocks.m_pPrev == &other.m_blocks)
            m_blocks.m_pPrev = &m_blocks;
        else
        {
            m_blocks.m_pPrev = temp_blocks.m_pPrev;
            VOGL_ASSERT(temp_blocks.m_pPrev->m_pNext == &other.m_blocks);
            temp_blocks.m_pPrev->m_pNext = &m_blocks;
        }

        std::swap(m_blocks.m_total_nodes, other.m_blocks.m_total_nodes);
    }

private:
    enum
    {
        cBlockBeginMarker = 0x1234ABCD,
        cBlockEndMarker = 0xEABC5678,
        cUsedNodeMarker = 0xCC139876,
        cFreeNodeMarker = 0xFF137654
    };

    struct raw_obj
    {
        uint64_t m_bytes[(sizeof(T) + sizeof(uint64_t) - 1) / sizeof(uint64_t)];
    };

    struct node
    {
#if VOGL_OBJECT_POOL_DEBUGGING
        uint32_t m_marker;
#endif

        union
        {
            node *m_pNext;
            raw_obj m_obj;
        };

        const T *get_object_ptr() const
        {
            return reinterpret_cast<const T *>(&m_obj);
        }

        T *get_object_ptr()
        {
            return reinterpret_cast<T *>(&m_obj);
        }
    };

    struct block
    {
        uint32_t m_begin_marker;

        block *m_pPrev;
        block *m_pNext;

        size_t m_total_nodes;

        uint32_t m_end_marker;

        node *get_first_node_ptr()
        {
            return reinterpret_cast<node *>(reinterpret_cast<uint8_t *>(this) + sizeof(block));
        }
        node *get_end_node_ptr()
        {
            return reinterpret_cast<node *>(reinterpret_cast<uint8_t *>(this) + sizeof(block)) + m_total_nodes;
        }
    };

    uint32_t m_flags;
    size_t m_grow_size;

    size_t m_next_grow_size;

    block m_blocks;
    size_t m_total_blocks;
    size_t m_total_heap_bytes;

    node *m_pFirst_free_node;
    size_t m_total_nodes;
    size_t m_total_free_nodes;

    mutable bool m_update_flag;

    void check_failure(uint32_t line) const
    {
        vogl_error_printf("check_failure() obj %p on line %u\n", this, line);
        VOGL_ASSERT_ALWAYS;
    }

    block *add_block(size_t size)
    {
        VOGL_ASSERT(size);
        size = math::maximum<size_t>(1U, size);

        block *pBlock = static_cast<block *>(vogl_malloc(sizeof(block) + size * sizeof(node)));

        m_total_blocks++;
        m_total_heap_bytes += vogl_msize(pBlock);

        m_total_nodes += size;
        m_total_free_nodes += size;

        pBlock->m_begin_marker = cBlockBeginMarker;
        pBlock->m_end_marker = cBlockEndMarker;

        pBlock->m_total_nodes = size;

        pBlock->m_pPrev = &m_blocks;
        pBlock->m_pNext = m_blocks.m_pNext;

        VOGL_ASSERT(m_blocks.m_pNext->m_pPrev == &m_blocks);
        m_blocks.m_pNext->m_pPrev = pBlock;

        m_blocks.m_pNext = pBlock;

        for (size_t i = size; i; --i)
        {
            node *pNode = pBlock->get_first_node_ptr() + i - 1;

#if VOGL_OBJECT_POOL_DEBUGGING
            pNode->m_marker = cFreeNodeMarker;
#endif

            pNode->m_pNext = m_pFirst_free_node;
            m_pFirst_free_node = pNode;
        }

        return pBlock;
    }

    block *grow()
    {
        add_block(m_next_grow_size);

        if (m_flags & cObjectPoolGrowExponential)
        {
            size_t prev_grow_size = m_next_grow_size;

            m_next_grow_size <<= 1U;

            if ((m_next_grow_size >> 1U) != prev_grow_size)
                m_next_grow_size = prev_grow_size;
        }

        return m_blocks.m_pNext;
    }

    inline node *alloc_node()
    {
        VOGL_ASSERT(!m_update_flag);

        LockingPolicy::lock();

        if (!m_pFirst_free_node)
            grow();

        VOGL_ASSERT(m_total_free_nodes);

        node *pNode = m_pFirst_free_node;

#if VOGL_OBJECT_POOL_DEBUGGING
        VOGL_ASSERT(pNode->m_marker == cFreeNodeMarker);
        pNode->m_marker = cUsedNodeMarker;
#endif

        m_pFirst_free_node = pNode->m_pNext;

        --m_total_free_nodes;

        LockingPolicy::unlock();

        return pNode;
    }

    size_t clear_internal()
    {
        VOGL_ASSERT(!m_update_flag);

        m_update_flag = true;

        size_t total_bytes_freed = m_total_heap_bytes;

        size_t total_blocks_found = 0;
        VOGL_NOTE_UNUSED(total_blocks_found);

        block *pCur_block = m_blocks.m_pNext;
        while (pCur_block != &m_blocks)
        {
            VOGL_ASSERT(pCur_block->m_begin_marker == cBlockBeginMarker);
            VOGL_ASSERT(pCur_block->m_end_marker == cBlockEndMarker);

            total_blocks_found++;
            VOGL_ASSERT(total_blocks_found <= m_total_blocks);

            block *pNext_block = pCur_block->m_pNext;

            vogl_free(pCur_block);

            pCur_block = pNext_block;
        }

        VOGL_ASSERT(total_blocks_found == m_total_blocks);

        m_next_grow_size = math::maximum<size_t>(1U, m_grow_size);

        m_blocks.m_pPrev = &m_blocks;
        m_blocks.m_pNext = &m_blocks;

        m_pFirst_free_node = NULL;
        m_total_nodes = 0;
        m_total_free_nodes = 0;

        m_total_blocks = 0;
        m_total_heap_bytes = 0;

        m_update_flag = false;

        return total_bytes_freed;
    }
};

#undef CHECK_FAILURE

bool object_pool_test();

} // namespace vogl
