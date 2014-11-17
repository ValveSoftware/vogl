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
 * furnished to do so, subject to the follo
 g conditions:
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

// File: vogl_threading_pthreads.cpp
#include "vogl_core.h"
#include "vogl_threading_pthreads.h"
#include "vogl_timer.h"
#include "vogl_port.h"

#if VOGL_USE_PTHREADS_API

#if defined(PLATFORM_WINDOWS)
    #include "vogl_winhdr.h"
#endif

#if defined(VOGL_USE_LINUX_API)
    #include <sys/sysinfo.h>
#endif

#if defined(VOGL_USE_OSX_API)
	#include <unistd.h>
	#include <sys/sysctl.h>
#endif

#if defined(PLATFORM_WINDOWS)
    #include <process.h>
#endif

namespace vogl
{
    uint32_t g_number_of_processors = 1;

    void vogl_threading_init()
    {
        #if defined(COMPILER_MSVC)
            SYSTEM_INFO g_system_info;
            GetSystemInfo(&g_system_info);
            g_number_of_processors = math::maximum<uint32_t>(1U, g_system_info.dwNumberOfProcessors);

        #elif defined(VOGL_USE_LINUX_API)
            g_number_of_processors = math::maximum<int>(1, get_nprocs());

		#elif defined(VOGL_USE_OSX_API)
			size_t	paramLen;
			int		numCPUs;

			numCPUs  = 1;
			paramLen = sizeof(numCPUs);

			if (sysctlbyname("hw.logicalcpu", &numCPUs, &paramLen, NULL, 0) == 0)
				g_number_of_processors = numCPUs;

        #else
            g_number_of_processors = 1;
        #endif
    }

    vogl_thread_id_t vogl_get_current_thread_id()
    {
        return plat_posix_gettid();
    }

    void vogl_sleep(unsigned int milliseconds)
    {
        #if defined(PLATFORM_WINDOWS)
            struct timespec interval;
            interval.tv_sec = milliseconds / 1000;
            interval.tv_nsec = (milliseconds % 1000) * 1000000L;
            pthread_delay_np(&interval);
        #else
            while (milliseconds)
            {
                int msecs_to_sleep = VOGL_MIN(milliseconds, 1000);
                usleep(msecs_to_sleep * 1000);
                milliseconds -= msecs_to_sleep;
            }
        #endif
    }

    mutex::mutex(unsigned int spin_count, bool recursive)
    {
        VOGL_NOTE_UNUSED(spin_count);

        pthread_mutexattr_t mta;

        int status = pthread_mutexattr_init(&mta);
        if (status)
        {
            dynamic_string msg(cVarArg, "pthread_mutexattr_init() failed with status %i", status);
            vogl_fail(msg.get_ptr(), __FILE__, __LINE__);
        }

        status = pthread_mutexattr_settype(&mta, recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
        if (status)
        {
            dynamic_string msg(cVarArg, "pthread_mutexattr_settype() failed with status %i", status);
            vogl_fail(msg.get_ptr(), __FILE__, __LINE__);
        }

        status = pthread_mutex_init(&m_mutex, &mta);
        if (status)
        {
            dynamic_string msg(cVarArg, "pthread_mutex_init() failed with status %i", status);
            vogl_fail(msg.get_ptr(), __FILE__, __LINE__);
        }

#ifdef VOGL_BUILD_DEBUG
        m_lock_count = 0;
#endif
    }

    mutex::~mutex()
    {
#ifdef VOGL_BUILD_DEBUG
        if (m_lock_count)
            vogl_assert("mutex::~mutex: mutex is still locked", __FILE__, __LINE__);
#endif
        if (pthread_mutex_destroy(&m_mutex))
            vogl_assert("mutex::~mutex: pthread_mutex_destroy() failed", __FILE__, __LINE__);
    }

    void mutex::lock()
    {
        pthread_mutex_lock(&m_mutex);
#ifdef VOGL_BUILD_DEBUG
        m_lock_count++;
#endif
    }

    void mutex::unlock()
    {
#ifdef VOGL_BUILD_DEBUG
        if (!m_lock_count)
            vogl_assert("mutex::unlock: mutex is not locked", __FILE__, __LINE__);
        m_lock_count--;
#endif
        pthread_mutex_unlock(&m_mutex);
    }

    void mutex::set_spin_count(unsigned int count)
    {
        VOGL_NOTE_UNUSED(count);
    }

    semaphore::semaphore(uint32_t initialCount, uint32_t maximumCount, const char *pName)
    {
        VOGL_NOTE_UNUSED(maximumCount);
        VOGL_NOTE_UNUSED(pName);

        VOGL_ASSERT(maximumCount >= initialCount);
        VOGL_ASSERT(maximumCount >= 1);

        if (sem_init(&m_sem, 0, initialCount))
        {
            VOGL_FAIL("semaphore: sem_init() failed");
        }
    }

    semaphore::~semaphore()
    {
        sem_destroy(&m_sem);
    }

    void semaphore::release(uint32_t releaseCount)
    {
        VOGL_ASSERT(releaseCount >= 1);

        if (plat_sem_post(&m_sem, releaseCount))
        {
            VOGL_FAIL("semaphore: sem_post() or sem_post_multiple() failed");
        }
    }

    void semaphore::try_release(uint32_t releaseCount)
    {
        VOGL_ASSERT(releaseCount >= 1);

        plat_try_sem_post(&m_sem, releaseCount);
    }

    bool semaphore::wait(uint32_t milliseconds)
    {
        int status;
        if (milliseconds == cUINT32_MAX)
        {
            status = sem_wait(&m_sem);
        }
#if defined(VOGL_USE_OSX_API)
		else
		{
			vogl::timer		timer;
			
			timer.start();
			do
				{
				status = sem_trywait(&m_sem);
				if (status == 0)
					break;

				usleep(1000);
				}
			while (timer.get_elapsed_ms() < milliseconds);
		}
#else
        else
        {
            struct timespec interval;
            interval.tv_sec = milliseconds / 1000;
            interval.tv_nsec = (milliseconds % 1000) * 1000000L;
            status = sem_timedwait(&m_sem, &interval);
        }
#endif

        if (status)
        {
            if (errno != ETIMEDOUT)
            {
                VOGL_FAIL("semaphore: sem_wait() or sem_timedwait() failed");
            }
            return false;
        }

        return true;
    }

    spinlock::spinlock()
    {
#ifdef VOGL_BUILD_DEBUG
        m_in_lock = false;
#endif

#if VOGL_USE_OSX_API
		m_spinlock = 0;
#else
        if (pthread_spin_init(&m_spinlock, 0))
        {
            VOGL_FAIL("spinlock: pthread_spin_init() failed");
        }
#endif
    }

    spinlock::~spinlock()
    {
#ifdef VOGL_BUILD_DEBUG
        VOGL_ASSERT(!m_in_lock);
#endif

#if !VOGL_USE_OSX_API
        pthread_spin_destroy(&m_spinlock);
#endif
    }

    void spinlock::lock()
    {
#if VOGL_USE_OSX_API
        OSSpinLockLock(&m_spinlock);
#else
        if (pthread_spin_lock(&m_spinlock))
        {
            VOGL_FAIL("spinlock: pthread_spin_lock() failed");
        }
#endif

#ifdef VOGL_BUILD_DEBUG
        VOGL_ASSERT(!m_in_lock);
        m_in_lock = true;
#endif
    }

    void spinlock::unlock()
    {
#ifdef VOGL_BUILD_DEBUG
        VOGL_ASSERT(m_in_lock);
        m_in_lock = false;
#endif

#if VOGL_USE_OSX_API
        OSSpinLockUnlock(&m_spinlock);
#else
        if (pthread_spin_unlock(&m_spinlock))
        {
            VOGL_FAIL("spinlock: pthread_spin_unlock() failed");
        }
#endif
    }

    task_pool::task_pool()
        : m_num_threads(0),
          m_tasks_available(0, 32767),
          m_all_tasks_completed(0, 1),
          m_total_submitted_tasks(0),
          m_total_completed_tasks(0),
          m_exit_flag(false)
    {
        utils::zero_object(m_threads);
    }

    task_pool::task_pool(uint32_t num_threads)
        : m_num_threads(0),
          m_tasks_available(0, 32767),
          m_all_tasks_completed(0, 1),
          m_total_submitted_tasks(0),
          m_total_completed_tasks(0),
          m_exit_flag(false)
    {
        utils::zero_object(m_threads);

        bool status = init(num_threads);
        VOGL_VERIFY(status);
    }

    task_pool::~task_pool()
    {
        deinit();
    }

    bool task_pool::init(uint32_t num_threads)
    {
        VOGL_ASSERT(num_threads <= cMaxThreads);
        num_threads = math::minimum<uint32_t>(num_threads, cMaxThreads);

        deinit();

        bool succeeded = true;

        m_num_threads = 0;
        while (m_num_threads < num_threads)
        {
            int status = pthread_create(&m_threads[m_num_threads], NULL, thread_func, this);
            if (status)
            {
                succeeded = false;
                break;
            }

            m_num_threads++;
        }

        if (!succeeded)
        {
            deinit();
            return false;
        }

        return true;
    }

    void task_pool::deinit()
    {
        if (m_num_threads)
        {
            join();

            atomic_exchange32(&m_exit_flag, true);

            m_tasks_available.release(m_num_threads);

            for (uint32_t i = 0; i < m_num_threads; i++)
                pthread_join(m_threads[i], NULL);

            m_num_threads = 0;

            atomic_exchange32(&m_exit_flag, false);
        }

        m_task_stack.clear();
        m_total_submitted_tasks = 0;
        m_total_completed_tasks = 0;
    }

    bool task_pool::queue_task(task_callback_func pFunc, uint64_t data, void *pData_ptr)
    {
        VOGL_ASSERT(pFunc);

        task tsk;
        tsk.m_callback = pFunc;
        tsk.m_data = data;
        tsk.m_pData_ptr = pData_ptr;
        tsk.m_flags = 0;

        atomic_increment32(&m_total_submitted_tasks);
        if (!m_task_stack.try_push(tsk))
        {
            atomic_increment32(&m_total_completed_tasks);
            return false;
        }

        m_tasks_available.release(1);

        return true;
    }

    // It's the object's responsibility to delete pObj within the execute_task() method, if needed!
    bool task_pool::queue_task(executable_task *pObj, uint64_t data, void *pData_ptr)
    {
        VOGL_ASSERT(pObj);

        task tsk;
        tsk.m_pObj = pObj;
        tsk.m_data = data;
        tsk.m_pData_ptr = pData_ptr;
        tsk.m_flags = cTaskFlagObject;

        atomic_increment32(&m_total_submitted_tasks);
        if (!m_task_stack.try_push(tsk))
        {
            atomic_increment32(&m_total_completed_tasks);
            return false;
        }

        m_tasks_available.release(1);

        return true;
    }

    void task_pool::process_task(task &tsk)
    {
        if (tsk.m_flags & cTaskFlagObject)
            tsk.m_pObj->execute_task(tsk.m_data, tsk.m_pData_ptr);
        else
            tsk.m_callback(tsk.m_data, tsk.m_pData_ptr);

        if (atomic_increment32(&m_total_completed_tasks) == m_total_submitted_tasks)
        {
            // Try to signal the semaphore (the max count is 1 so this may actually fail).
            m_all_tasks_completed.try_release();
        }
    }

    void task_pool::join()
    {
        // Try to steal any outstanding tasks. This could cause one or more worker threads to wake up and immediately go back to sleep, which is wasteful but should be harmless.
        task tsk;
        while (m_task_stack.pop(tsk))
            process_task(tsk);

        // At this point the task stack is empty.
        // Now wait for all concurrent tasks to complete. The m_all_tasks_completed semaphore has a max count of 1, so it's possible it could have saturated to 1 as the tasks
        // where issued and asynchronously completed, so this loop may iterate a few times.
        const int total_submitted_tasks = static_cast<int>(atomic_add32(&m_total_submitted_tasks, 0));
        while (m_total_completed_tasks != total_submitted_tasks)
        {
            // If the previous (m_total_completed_tasks != total_submitted_tasks) check failed the semaphore MUST be eventually signalled once the last task completes.
            // So I think this can actually be an INFINITE delay, but it shouldn't really matter if it's 1ms.
            m_all_tasks_completed.wait(1);
        }
    }

    void *task_pool::thread_func(void *pContext)
    {
        task_pool *pPool = static_cast<task_pool *>(pContext);
        task tsk;

        for (;;)
        {
            if (!pPool->m_tasks_available.wait())
                break;

            if (pPool->m_exit_flag)
                break;

            if (pPool->m_task_stack.pop(tsk))
            {
                pPool->process_task(tsk);
            }
        }

        return NULL;
    }

} // namespace vogl

#endif // VOGL_USE_PTHREADS_API
