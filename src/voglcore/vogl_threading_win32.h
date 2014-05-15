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

// File: vogl_win32_threading.h
#pragma once

#include "vogl_core.h"

#include "vogl_atomics.h"
#if VOGL_NO_ATOMICS
#error No atomic operations defined in vogl_platform.h!
#endif

namespace vogl
{
    // g_number_of_processors defaults to 1. Will be higher on multicore machines.
    extern uint32_t g_number_of_processors;

    void vogl_threading_init();

    typedef uint64_t vogl_thread_id_t;
    vogl_thread_id_t vogl_get_current_thread_id();

    void vogl_sleep(unsigned int milliseconds);

    uint32_t vogl_get_max_helper_threads();

    class mutex
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(mutex);

    public:
        mutex(unsigned int spin_count = 0);
        ~mutex();
        void lock();
        void unlock();
        void set_spin_count(unsigned int count);

    private:
        int m_buf[12];

#ifdef VOGL_BUILD_DEBUG
        unsigned int m_lock_count;
#endif
    };

    class scoped_mutex
    {
        scoped_mutex(const scoped_mutex &);
        scoped_mutex &operator=(const scoped_mutex &);

    public:
        inline scoped_mutex(mutex &m)
            : m_mutex(m)
        {
            m_mutex.lock();
        }
        inline ~scoped_mutex()
        {
            m_mutex.unlock();
        }

    private:
        mutex &m_mutex;
    };

    // Simple non-recursive spinlock.
    class spinlock
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(spinlock);

    public:
        inline spinlock()
            : m_flag(0)
        {
        }

        void lock(uint32_t max_spins = 4096, bool yielding = true);

        inline void lock_no_barrier(uint32_t max_spins = 4096, bool yielding = true)
        {
            lock(max_spins, yielding);
        }

        void unlock();

        inline void unlock_no_barrier()
        {
            m_flag = VOGL_FALSE;
        }

    private:
        volatile int32_t m_flag;
    };

    class scoped_spinlock
    {
        scoped_spinlock(const scoped_spinlock &);
        scoped_spinlock &operator=(const scoped_spinlock &);

    public:
        inline scoped_spinlock(spinlock &lock)
            : m_lock(lock)
        {
            m_lock.lock();
        }
        inline ~scoped_spinlock()
        {
            m_lock.unlock();
        }

    private:
        spinlock &m_lock;
    };

    class semaphore
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(semaphore);

    public:
        // TODO: Change to uint32_t
        semaphore(int32_t initialCount, int32_t maximumCount, const char *pName = NULL);

        ~semaphore();

        inline HANDLE get_handle(void) const
        {
            return m_handle;
        }

        void release(int32_t releaseCount = 1, int32_t *pPreviousCount = NULL);
        bool try_release(int32_t releaseCount = 1, int32_t *pPreviousCount = NULL);

        bool wait(uint32_t milliseconds = cUINT32_MAX);

    private:
        HANDLE m_handle;
    };

    template <typename T>
    class tsstack
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(tsstack);

    public:
        inline tsstack(bool use_freelist = true)
            : m_use_freelist(use_freelist)
        {
            VOGL_VERIFY(((uintptr_t) this & (VOGL_GET_ALIGNMENT(tsstack) - 1)) == 0);
            InitializeSListHead(&m_stack_head);
            InitializeSListHead(&m_freelist_head);
        }

        inline ~tsstack()
        {
            clear();
        }

        inline void clear()
        {
            for (;;)
            {
                node *pNode = (node *)InterlockedPopEntrySList(&m_stack_head);
                if (!pNode)
                    break;

                VOGL_MEMORY_IMPORT_BARRIER

                helpers::destruct(&pNode->m_obj);

                vogl_free(pNode);
            }

            flush_freelist();
        }

        inline void flush_freelist()
        {
            if (!m_use_freelist)
                return;

            for (;;)
            {
                node *pNode = (node *)InterlockedPopEntrySList(&m_freelist_head);
                if (!pNode)
                    break;

                VOGL_MEMORY_IMPORT_BARRIER

                vogl_free(pNode);
            }
        }

        inline bool try_push(const T &obj)
        {
            node *pNode = alloc_node();
            if (!pNode)
                return false;

            helpers::construct(&pNode->m_obj, obj);

            VOGL_MEMORY_EXPORT_BARRIER

            InterlockedPushEntrySList(&m_stack_head, &pNode->m_slist_entry);

            return true;
        }

        inline bool pop(T &obj)
        {
            node *pNode = (node *)InterlockedPopEntrySList(&m_stack_head);
            if (!pNode)
                return false;

            VOGL_MEMORY_IMPORT_BARRIER

            obj = pNode->m_obj;

            helpers::destruct(&pNode->m_obj);

            free_node(pNode);

            return true;
        }

    private:
        SLIST_HEADER m_stack_head;
        SLIST_HEADER m_freelist_head;

        struct node
        {
            SLIST_ENTRY m_slist_entry;
            T m_obj;
        };

        bool m_use_freelist;

        inline node *alloc_node()
        {
            node *pNode = m_use_freelist ? (node *)InterlockedPopEntrySList(&m_freelist_head) : NULL;

            if (!pNode)
                pNode = (node *)vogl_malloc(sizeof(node));

            return pNode;
        }

        inline void free_node(node *pNode)
        {
            if (m_use_freelist)
                InterlockedPushEntrySList(&m_freelist_head, &pNode->m_slist_entry);
            else
                vogl_free(pNode);
        }
    };

    // Simple multithreaded task pool. This class assumes a single global thread will be issuing tasks and joining.
    class task_pool
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(task_pool);

    public:
        task_pool();
        task_pool(uint32_t num_threads);
        ~task_pool();

        enum
        {
            cMaxThreads = 16
        };
        bool init(uint32_t num_threads);
        void deinit();

        inline uint32_t get_num_threads() const
        {
            return m_num_threads;
        }
        inline uint32_t get_num_outstanding_tasks() const
        {
            return m_total_submitted_tasks - m_total_completed_tasks;
        }

        // C-style task callback
        typedef void (*task_callback_func)(uint64_t data, void *pData_ptr);
        bool queue_task(task_callback_func pFunc, uint64_t data = 0, void *pData_ptr = NULL);

        class executable_task
        {
        public:
            virtual void execute_task(uint64_t data, void *pData_ptr) = 0;
        };

        // It's the caller's responsibility to delete pObj within the execute_task() method, if needed!
        bool queue_task(executable_task *pObj, uint64_t data = 0, void *pData_ptr = NULL);

        template <typename S, typename T>
        inline bool queue_object_task(S *pObject, T pObject_method, uint64_t data = 0, void *pData_ptr = NULL);

        template <typename S, typename T>
        inline bool queue_multiple_object_tasks(S *pObject, T pObject_method, uint64_t first_data, uint32_t num_tasks, void *pData_ptr = NULL);

        // Waits for all outstanding tasks (if any) to complete.
        // The calling thread will steal any outstanding tasks from worker threads, if possible.
        void join();

    private:
        struct task
        {
            //inline task() : m_data(0), m_pData_ptr(NULL), m_pObj(NULL), m_flags(0) { }

            uint64_t m_data;
            void *m_pData_ptr;

            union
            {
                task_callback_func m_callback;
                executable_task *m_pObj;
            };

            uint32_t m_flags;
        };

        typedef tsstack<task> ts_task_stack_t;
        ts_task_stack_t *m_pTask_stack;

        uint32_t m_num_threads;
        HANDLE m_threads[cMaxThreads];

        // Signalled whenever a task is queued up.
        semaphore m_tasks_available;

        // Signalled when all outstanding tasks are completed.
        semaphore m_all_tasks_completed;

        enum task_flags
        {
            cTaskFlagObject = 1
        };

        volatile atomic32_t m_total_submitted_tasks;
        volatile atomic32_t m_total_completed_tasks;
        volatile atomic32_t m_exit_flag;

        void process_task(task &tsk);

        static unsigned __stdcall thread_func(void *pContext);
    };

    enum object_task_flags
    {
        cObjectTaskFlagDefault = 0,
        cObjectTaskFlagDeleteAfterExecution = 1
    };

    template <typename T>
    class object_task : public task_pool::executable_task
    {
    public:
        object_task(uint32_t flags = cObjectTaskFlagDefault)
            : m_pObject(NULL),
              m_pMethod(NULL),
              m_flags(flags)
        {
        }

        typedef void (T::*object_method_ptr)(uint64_t data, void *pData_ptr);

        object_task(T *pObject, object_method_ptr pMethod, uint32_t flags = cObjectTaskFlagDefault)
            : m_pObject(pObject),
              m_pMethod(pMethod),
              m_flags(flags)
        {
            VOGL_ASSERT(pObject && pMethod);
        }

        void init(T *pObject, object_method_ptr pMethod, uint32_t flags = cObjectTaskFlagDefault)
        {
            VOGL_ASSERT(pObject && pMethod);

            m_pObject = pObject;
            m_pMethod = pMethod;
            m_flags = flags;
        }

        T *get_object() const
        {
            return m_pObject;
        }
        object_method_ptr get_method() const
        {
            return m_pMethod;
        }

        virtual void execute_task(uint64_t data, void *pData_ptr)
        {
            (m_pObject->*m_pMethod)(data, pData_ptr);

            if (m_flags & cObjectTaskFlagDeleteAfterExecution)
                vogl_delete(this);
        }

    protected:
        T *m_pObject;

        object_method_ptr m_pMethod;

        uint32_t m_flags;
    };

    template <typename S, typename T>
    inline bool task_pool::queue_object_task(S *pObject, T pObject_method, uint64_t data, void *pData_ptr)
    {
        object_task<S> *pTask = vogl_new(object_task<S>, pObject, pObject_method, cObjectTaskFlagDeleteAfterExecution);
        if (!pTask)
            return false;
        return queue_task(pTask, data, pData_ptr);
    }

    template <typename S, typename T>
    inline bool task_pool::queue_multiple_object_tasks(S *pObject, T pObject_method, uint64_t first_data, uint32_t num_tasks, void *pData_ptr)
    {
        VOGL_ASSERT(pObject);
        VOGL_ASSERT(num_tasks);
        if (!num_tasks)
            return true;

        bool status = true;

        uint32_t i;
        for (i = 0; i < num_tasks; i++)
        {
            task tsk;

            tsk.m_pObj = vogl_new<object_task<S> >(pObject, pObject_method, cObjectTaskFlagDeleteAfterExecution);
            if (!tsk.m_pObj)
            {
                status = false;
                break;
            }

            tsk.m_data = first_data + i;
            tsk.m_pData_ptr = pData_ptr;
            tsk.m_flags = cTaskFlagObject;

            atomic_increment32(&m_total_submitted_tasks);

            if (!m_pTask_stack->try_push(tsk))
            {
                atomic_increment32(&m_total_completed_tasks);

                status = false;
                break;
            }
        }

        if (i)
        {
            m_tasks_available.release(i);
        }

        return status;
    }

} // namespace vogl
