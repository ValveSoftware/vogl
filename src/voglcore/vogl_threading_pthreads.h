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

// File: vogl_threading_pthreads.h
#pragma once

#include "vogl_core.h"

#if VOGL_USE_PTHREADS_API

#include "vogl_atomics.h"

#if VOGL_NO_ATOMICS
#error No atomic operations defined in vogl_platform.h!
#endif

#include <pthread.h>
#include <semaphore.h>

#if defined(PLATFORM_LINUX)
    #include <unistd.h>
#endif

#if defined(VOGL_USE_OSX_API)
	#include <libkern/OSAtomic.h>
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
        mutex(const mutex &);
        mutex &operator=(const mutex &);

    public:
        mutex(unsigned int spin_count = 0, bool recursive = false);
        ~mutex();
        void lock();
        void unlock();
        void set_spin_count(unsigned int count);

    private:
        pthread_mutex_t m_mutex;

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

    class semaphore
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(semaphore);

    public:
        semaphore(uint32_t initialCount, uint32_t maximumCount, const char *pName = NULL);
        ~semaphore();

        void release(uint32_t releaseCount = 1);
        void try_release(uint32_t releaseCount = 1);
        bool wait(uint32_t milliseconds = cUINT32_MAX);

    private:
        sem_t m_sem;
    };

    class spinlock
    {
    public:
        spinlock();
        ~spinlock();

        void lock();
        void unlock();

    private:
#if VOGL_USE_OSX_API
		OSSpinLock m_spinlock;
#else
        pthread_spinlock_t m_spinlock;
#endif

#ifdef VOGL_BUILD_DEBUG
        bool m_in_lock;
#endif
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

    template <typename T, uint32_t cMaxSize>
    class tsstack
    {
    public:
        inline tsstack()
            : m_top(0)
        {
        }

        inline ~tsstack()
        {
        }

        inline void clear()
        {
            m_spinlock.lock();
            m_top = 0;
            m_spinlock.unlock();
        }

        inline bool try_push(const T &obj)
        {
            bool result = false;
            m_spinlock.lock();
            if (m_top < (int)cMaxSize)
            {
                m_stack[m_top++] = obj;
                result = true;
            }
            m_spinlock.unlock();
            return result;
        }

        inline bool pop(T &obj)
        {
            bool result = false;
            m_spinlock.lock();
            if (m_top > 0)
            {
                obj = m_stack[--m_top];
                result = true;
            }
            m_spinlock.unlock();
            return result;
        }

    private:
        spinlock m_spinlock;
        T m_stack[cMaxSize];
        int m_top;
    };

    class task_pool
    {
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
            return static_cast<uint32_t>(m_total_submitted_tasks - m_total_completed_tasks);
        }

        // C-style task callback
        typedef void (*task_callback_func)(uint64_t data, void *pData_ptr);
        bool queue_task(task_callback_func pFunc, uint64_t data = 0, void *pData_ptr = NULL);

        class executable_task
        {
        public:
            virtual ~executable_task()
            {
            }
            virtual void execute_task(uint64_t data, void *pData_ptr) = 0;
        };

        // It's the caller's responsibility to delete pObj within the execute_task() method, if needed!
        bool queue_task(executable_task *pObj, uint64_t data = 0, void *pData_ptr = NULL);

        template <typename S, typename T>
        inline bool queue_object_task(S *pObject, T pObject_method, uint64_t data = 0, void *pData_ptr = NULL);

        template <typename S, typename T>
        inline bool queue_multiple_object_tasks(S *pObject, T pObject_method, uint64_t first_data, uint32_t num_tasks, void *pData_ptr = NULL);

        void join();

    private:
        struct task
        {
            inline task()
                : m_data(0), m_pData_ptr(NULL), m_pObj(NULL), m_flags(0)
            {
            }

            uint64_t m_data;
            void *m_pData_ptr;

            union
            {
                task_callback_func m_callback;
                executable_task *m_pObj;
            };

            uint32_t m_flags;
        };

        tsstack<task, cMaxThreads> m_task_stack;

        uint32_t m_num_threads;
        pthread_t m_threads[cMaxThreads];

        // Signalled whenever a task is queued up.
        semaphore m_tasks_available;

        // Signalled when all outstanding tasks are completed.
        semaphore m_all_tasks_completed;

        enum task_flags
        {
            cTaskFlagObject = 1
        };

        atomic32_t m_total_submitted_tasks;
        atomic32_t m_total_completed_tasks;
        atomic32_t m_exit_flag;

        void process_task(task &tsk);

        static void *thread_func(void *pContext);
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

        virtual ~object_task()
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

            tsk.m_pObj = vogl_new(object_task<S>, pObject, pObject_method, cObjectTaskFlagDeleteAfterExecution);
            if (!tsk.m_pObj)
            {
                status = false;
                break;
            }

            tsk.m_data = first_data + i;
            tsk.m_pData_ptr = pData_ptr;
            tsk.m_flags = cTaskFlagObject;

            atomic_increment32(&m_total_submitted_tasks);

            if (!m_task_stack.try_push(tsk))
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

#endif // VOGL_USE_PTHREADS_API
