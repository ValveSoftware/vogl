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

// File: vogl_timer.cpp
#include "vogl_core.h"
#include "vogl_timer.h"
#include <time.h>

#include "vogl_timer.h"

#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

namespace vogl
{
    uint64_t timer::g_init_ticks;
    uint64_t timer::g_freq;
    double timer::g_inv_freq;

#if defined(VOGL_USE_WIN32_API)
    inline void query_counter(timer_ticks *pTicks)
    {
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(pTicks));
    }
    inline void query_counter_frequency(timer_ticks *pTicks)
    {
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER *>(pTicks));
    }
#elif defined(COMPILER_GCCLIKE)

	#if defined(VOGL_USE_LINUX_API)
	    #include <sys/timex.h>
	#elif defined(VOGL_USE_OSX_API)
		#include <sys/time.h>
	#endif

    inline void query_counter(timer_ticks *pTicks)
    {
        struct timeval cur_time;
        gettimeofday(&cur_time, NULL);
        *pTicks = static_cast<unsigned long long>(cur_time.tv_sec) * 1000000ULL + static_cast<unsigned long long>(cur_time.tv_usec);
    }
    inline void query_counter_frequency(timer_ticks *pTicks)
    {
        *pTicks = 1000000;
    }
#else
    #error Unimplemented
#endif

    timer::timer()
        : m_start_time(0),
          m_stop_time(0),
          m_started(false),
          m_stopped(false)
    {
        if (!g_inv_freq)
            init();
    }

    timer::timer(timer_ticks start_ticks)
    {
        if (!g_inv_freq)
            init();

        m_start_time = start_ticks;

        m_started = true;
        m_stopped = false;
    }

    void timer::start(timer_ticks start_ticks)
    {
        m_start_time = start_ticks;

        m_started = true;
        m_stopped = false;
    }

    void timer::start()
    {
        query_counter(&m_start_time);

        m_started = true;
        m_stopped = false;
    }

    void timer::stop()
    {
        VOGL_ASSERT(m_started);

        query_counter(&m_stop_time);

        m_stopped = true;
    }

    double timer::get_elapsed_secs() const
    {
        VOGL_ASSERT(m_started);
        if (!m_started)
            return 0;

        timer_ticks stop_time = m_stop_time;
        if (!m_stopped)
            query_counter(&stop_time);

        timer_ticks delta = stop_time - m_start_time;
        return delta * g_inv_freq;
    }

    timer_ticks timer::get_elapsed_us() const
    {
        VOGL_ASSERT(m_started);
        if (!m_started)
            return 0;

        timer_ticks stop_time = m_stop_time;
        if (!m_stopped)
            query_counter(&stop_time);

        timer_ticks delta = stop_time - m_start_time;
        return (delta * 1000000ULL + (g_freq >> 1U)) / g_freq;
    }

    timer_ticks timer::get_elapsed_ticks() const
    {
        VOGL_ASSERT(m_started);
        if (!m_started)
            return 0;

        timer_ticks stop_time = m_stop_time;
        if (!m_stopped)
            query_counter(&stop_time);

        return stop_time - m_start_time;
    }

    void timer::init()
    {
        if (!g_inv_freq)
        {
            query_counter_frequency(&g_freq);
            g_inv_freq = 1.0f / g_freq;

            query_counter(&g_init_ticks);
        }
    }

    timer_ticks timer::get_init_ticks()
    {
        if (!g_inv_freq)
            init();

        return g_init_ticks;
    }

    timer_ticks timer::get_ticks()
    {
        if (!g_inv_freq)
            init();

        timer_ticks ticks;
        query_counter(&ticks);
        return ticks - g_init_ticks;
    }

    double timer::ticks_to_secs(timer_ticks ticks)
    {
        if (!g_inv_freq)
            init();

        return ticks * g_inv_freq;
    }

} // namespace vogl
