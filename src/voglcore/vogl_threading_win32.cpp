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

// File: vogl_win32_threading.cpp
#include "vogl_core.h"
#include "vogl_threading_win32.h"
#include "vogl_winhdr.h"
#include <process.h>

namespace vogl
{
    uint g_number_of_processors = 1;

    void vogl_threading_init()
    {
        SYSTEM_INFO g_system_info;
        GetSystemInfo(&g_system_info);

        g_number_of_processors = math::maximum<uint>(1U, g_system_info.dwNumberOfProcessors);
    }

    vogl_thread_id_t vogl_get_current_thread_id()
    {
        return static_cast<vogl_thread_id_t>(GetCurrentThreadId());
    }

    void vogl_sleep(unsigned int milliseconds)
    {
        Sleep(milliseconds);
    }

    uint vogl_get_max_helper_threads()
    {
        if (g_number_of_processors > 1)
        {
            // use all CPU's
            return VOGL_MIN((int)task_pool::cMaxThreads, (int)g_number_of_processors - 1);
        }

        return 0;
    }

    mutex::mutex(unsigned int spin_count)
    {
        VOGL_ASSUME(sizeof(mutex) >= sizeof(CRITICAL_SECTION));

        void *p = m_buf;
        CRITICAL_SECTION &m_cs = *static_cast<CRITICAL_SECTION *>(p);

        BOOL status = true;
        status = InitializeCriticalSectionAndSpinCount(&m_cs, spin_count);
        if (!status)
            vogl_fail("mutex::mutex: InitializeCriticalSectionAndSpinCount failed", __FILE__, __LINE__);

#ifdef VOGL_BUILD_DEBUG
        m_lock_count = 0;
#endif
    }

    mutex::~mutex()
    {
        void *p = m_buf;
        CRITICAL_SECTION &m_cs = *static_cast<CRITICAL_SECTION *>(p);

#ifdef VOGL_BUILD_DEBUG
        if (m_lock_count)
            vogl_assert("mutex::~mutex: mutex is still locked", __FILE__, __LINE__);
#endif
        DeleteCriticalSection(&m_cs);
    }

    void mutex::lock()
    {
        void *p = m_buf;
        CRITICAL_SECTION &m_cs = *static_cast<CRITICAL_SECTION *>(p);

        EnterCriticalSection(&m_cs);
#ifdef VOGL_BUILD_DEBUG
        m_lock_count++;
#endif
    }

    void mutex::unlock()
    {
        void *p = m_buf;
        CRITICAL_SECTION &m_cs = *static_cast<CRITICAL_SECTION *>(p);

#ifdef VOGL_BUILD_DEBUG
        if (!m_lock_count)
            vogl_assert("mutex::unlock: mutex is not locked", __FILE__, __LINE__);
        m_lock_count--;
#endif
        LeaveCriticalSection(&m_cs);
    }

    void mutex::set_spin_count(unsigned int count)
    {
        void *p = m_buf;
        CRITICAL_SECTION &m_cs = *static_cast<CRITICAL_SECTION *>(p);

        SetCriticalSectionSpinCount(&m_cs, count);
    }

    void spinlock::lock(uint32 max_spins, bool yielding)
    {
        if (g_number_of_processors <= 1)
            max_spins = 1;

        uint32 spinCount = 0;
        uint32 yieldCount = 0;

        for (;;)
        {
            VOGL_ASSUME(sizeof(long) == sizeof(int32));
            if (!InterlockedExchange((volatile long *)&m_flag, TRUE))
                break;

            YieldProcessor();
            YieldProcessor();
            YieldProcessor();
            YieldProcessor();
            YieldProcessor();
            YieldProcessor();
            YieldProcessor();
            YieldProcessor();

            spinCount++;
            if ((yielding) && (spinCount >= max_spins))
            {
                switch (yieldCount)
                {
                    case 0:
                    {
                        spinCount = 0;

                        Sleep(0);

                        yieldCount++;
                        break;
                    }
                    case 1:
                    {
                        if (g_number_of_processors <= 1)
                            spinCount = 0;
                        else
                            spinCount = max_spins / 2;

                        Sleep(1);

                        yieldCount++;
                        break;
                    }
                    case 2:
                    {
                        if (g_number_of_processors <= 1)
                            spinCount = 0;
                        else
                            spinCount = max_spins;

                        Sleep(2);
                        break;
                    }
                }
            }
        }

        VOGL_MEMORY_IMPORT_BARRIER
    }

    void spinlock::unlock()
    {
        VOGL_MEMORY_EXPORT_BARRIER

        InterlockedExchange((volatile long *)&m_flag, FALSE);
    }

    semaphore::semaphore(int32 initialCount, int32 maximumCount, const char *pName)
    {
        m_handle = CreateSemaphoreA(NULL, initialCount, maximumCount, pName);
        if (NULL == m_handle)
        {
            VOGL_FAIL("semaphore: CreateSemaphore() failed");
        }
    }

    semaphore::~semaphore()
    {
        if (m_handle)
        {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

    void semaphore::release(int32 releaseCount, int32 *pPreviousCount)
    {
        VOGL_ASSUME(sizeof(LONG) == sizeof(int32));
        if (0 == ReleaseSemaphore(m_handle, releaseCount, (LPLONG)pPreviousCount))
        {
            VOGL_FAIL("semaphore: ReleaseSemaphore() failed");
        }
    }

    bool semaphore::try_release(int32 releaseCount, int32 *pPreviousCount)
    {
        VOGL_ASSUME(sizeof(LONG) == sizeof(int32));
        return ReleaseSemaphore(m_handle, releaseCount, (LPLONG)pPreviousCount) != 0;
    }

    bool semaphore::wait(uint32 milliseconds)
    {
        uint32 result = WaitForSingleObject(m_handle, milliseconds);

        if (WAIT_FAILED == result)
        {
            VOGL_FAIL("semaphore: WaitForSingleObject() failed");
        }

        return WAIT_OBJECT_0 == result;
    }

    task_pool::task_pool()
        : m_pTask_stack(vogl_new<ts_task_stack_t>()),
          m_num_threads(0),
          m_tasks_available(0, 32767),
          m_all_tasks_completed(0, 1),
          m_total_submitted_tasks(0),
          m_total_completed_tasks(0),
          m_exit_flag(false)
    {
        utils::zero_object(m_threads);
    }

    task_pool::task_pool(uint num_threads)
        : m_pTask_stack(vogl_new<ts_task_stack_t>()),
          m_num_threads(0),
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
        vogl_delete(m_pTask_stack);
    }

    bool task_pool::init(uint num_threads)
    {
        VOGL_ASSERT(num_threads <= cMaxThreads);
        num_threads = math::minimum<uint>(num_threads, cMaxThreads);

        deinit();

        bool succeeded = true;

        m_num_threads = 0;
        while (m_num_threads < num_threads)
        {
            m_threads[m_num_threads] = (HANDLE)_beginthreadex(NULL, 32768, thread_func, this, 0, NULL);
            VOGL_ASSERT(m_threads[m_num_threads] != 0);

            if (!m_threads[m_num_threads])
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

            // Set exit flag, then release all threads. Each should wakeup and exit.
            atomic_exchange32(&m_exit_flag, true);

            m_tasks_available.release(m_num_threads);

            // Now wait for each thread to exit.
            for (uint i = 0; i < m_num_threads; i++)
            {
                if (m_threads[i])
                {
                    for (;;)
                    {
                        // Can be an INFINITE delay, but set at 30 seconds so this function always provably exits.
                        DWORD result = WaitForSingleObject(m_threads[i], 30000);
                        if ((result == WAIT_OBJECT_0) || (result == WAIT_ABANDONED))
                            break;
                    }

                    CloseHandle(m_threads[i]);
                    m_threads[i] = NULL;
                }
            }

            m_num_threads = 0;

            atomic_exchange32(&m_exit_flag, false);
        }

        if (m_pTask_stack)
            m_pTask_stack->clear();
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

        if (!m_pTask_stack->try_push(tsk))
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

        if (!m_pTask_stack->try_push(tsk))
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
        while (m_pTask_stack->pop(tsk))
            process_task(tsk);

        // At this point the task stack is empty.
        // Now wait for all concurrent tasks to complete. The m_all_tasks_completed semaphore has a max count of 1, so it's possible it could have saturated to 1 as the tasks
        // where issued and asynchronously completed, so this loop may iterate a few times.
        const int total_submitted_tasks = atomic_add32(&m_total_submitted_tasks, 0);
        while (m_total_completed_tasks != total_submitted_tasks)
        {
            // If the previous (m_total_completed_tasks != total_submitted_tasks) check failed the semaphore MUST be eventually signalled once the last task completes.
            // So I think this can actually be an INFINITE delay, but it shouldn't really matter if it's 1ms.
            m_all_tasks_completed.wait(1);
        }
    }

    unsigned __stdcall task_pool::thread_func(void *pContext)
    {
        task_pool *pPool = static_cast<task_pool *>(pContext);

        for (;;)
        {
            if (!pPool->m_tasks_available.wait())
                break;

            if (pPool->m_exit_flag)
                break;

            task tsk;
            if (pPool->m_pTask_stack->pop(tsk))
                pPool->process_task(tsk);
        }

        _endthreadex(0);
        return 0;
    }

} // namespace vogl
