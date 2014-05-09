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

// File: vogl_atomics.h
#pragma once

#include "vogl_core.h"

#ifdef PLATFORM_WINDOWS
    #include "vogl_winhdr.h"
#endif

#if defined(COMPILER_GCCLIKE) && VOGL_PLATFORM_PC
extern __inline__ __attribute__((__always_inline__, __gnu_inline__)) void vogl_yield_processor()
{
    __asm__ __volatile__("pause");
}
#else
VOGL_FORCE_INLINE void vogl_yield_processor()
{
#if VOGL_USE_MSVC_INTRINSICS
#if VOGL_PLATFORM_PC_X64
    _mm_pause();
#else
    YieldProcessor();
#endif
#else
// No implementation
#endif
}
#endif

#if VOGL_USE_WIN32_ATOMIC_FUNCTIONS
extern "C" __int64 _InterlockedCompareExchange64(__int64 volatile *Destination, __int64 Exchange, __int64 Comperand);
#if defined(COMPILER_MSVC)
    #pragma intrinsic(_InterlockedCompareExchange64)
#endif
#endif // VOGL_USE_WIN32_ATOMIC_FUNCTIONS

namespace vogl
{
#if VOGL_USE_WIN32_ATOMIC_FUNCTIONS
    typedef volatile LONG atomic32_t;
    typedef volatile LONGLONG atomic64_t;

    // Returns the original value.
    inline atomic32_t atomic_compare_exchange32(atomic32_t volatile *pDest, atomic32_t exchange, atomic32_t comparand)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return InterlockedCompareExchange(pDest, exchange, comparand);
    }

    // Returns the original value.
    inline atomic64_t atomic_compare_exchange64(atomic64_t volatile *pDest, atomic64_t exchange, atomic64_t comparand)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 7) == 0);
        return _InterlockedCompareExchange64(pDest, exchange, comparand);
    }

    // Returns the resulting incremented value.
    inline atomic32_t atomic_increment32(atomic32_t volatile *pDest)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return InterlockedIncrement(pDest);
    }

    // Returns the resulting decremented value.
    inline atomic32_t atomic_decrement32(atomic32_t volatile *pDest)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return InterlockedDecrement(pDest);
    }

    // Returns the original value.
    inline atomic32_t atomic_exchange32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return InterlockedExchange(pDest, val);
    }

    // Returns the resulting value.
    inline atomic32_t atomic_add32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return InterlockedExchangeAdd(pDest, val) + val;
    }

    // Returns the original value.
    inline atomic32_t atomic_exchange_add32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return InterlockedExchangeAdd(pDest, val);
    }
#elif VOGL_USE_GCC_ATOMIC_BUILTINS
    typedef volatile long atomic32_t;
    typedef long nonvolatile_atomic32_t;
    typedef VOGL_ALIGNED_BEGIN(8) volatile long long VOGL_ALIGNED_END(8) atomic64_t;
    typedef long long nonvolatile_atomic64_t;

    // Returns the original value.
    inline nonvolatile_atomic32_t atomic_compare_exchange32(atomic32_t volatile *pDest, atomic32_t exchange, atomic32_t comparand)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return __sync_val_compare_and_swap(pDest, comparand, exchange);
    }

    // Returns the original value.
    inline nonvolatile_atomic64_t atomic_compare_exchange64(atomic64_t volatile *pDest, atomic64_t exchange, atomic64_t comparand)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 7) == 0);
        return __sync_val_compare_and_swap(pDest, comparand, exchange);
    }

    // Returns the resulting incremented value.
    inline nonvolatile_atomic32_t atomic_increment32(atomic32_t volatile *pDest)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return __sync_add_and_fetch(pDest, 1);
    }

    // Returns the resulting decremented value.
    inline nonvolatile_atomic32_t atomic_decrement32(atomic32_t volatile *pDest)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return __sync_sub_and_fetch(pDest, 1);
    }

    // Returns the original value.
    inline nonvolatile_atomic32_t atomic_exchange32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return __sync_lock_test_and_set(pDest, val);
    }

    // Returns the resulting value.
    inline nonvolatile_atomic32_t atomic_add32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return __sync_add_and_fetch(pDest, val);
    }

    // Returns the original value.
    inline nonvolatile_atomic32_t atomic_exchange_add32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return __sync_fetch_and_add(pDest, val);
    }
#else
#define VOGL_NO_ATOMICS 1

    // Atomic ops not supported - but try to do something reasonable. Assumes no threading at all.
    typedef long atomic32_t;
    typedef long long atomic64_t;

    inline atomic32_t atomic_compare_exchange32(atomic32_t volatile *pDest, atomic32_t exchange, atomic32_t comparand)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        atomic32_t cur = *pDest;
        if (cur == comparand)
            *pDest = exchange;
        return cur;
    }

    inline atomic64_t atomic_compare_exchange64(atomic64_t volatile *pDest, atomic64_t exchange, atomic64_t comparand)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 7) == 0);
        atomic64_t cur = *pDest;
        if (cur == comparand)
            *pDest = exchange;
        return cur;
    }

    inline atomic32_t atomic_increment32(atomic32_t volatile *pDest)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return (*pDest += 1);
    }

    inline atomic32_t atomic_decrement32(atomic32_t volatile *pDest)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return (*pDest -= 1);
    }

    inline atomic32_t atomic_exchange32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        atomic32_t cur = *pDest;
        *pDest = val;
        return cur;
    }

    inline atomic32_t atomic_add32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        return (*pDest += val);
    }

    inline atomic32_t atomic_exchange_add32(atomic32_t volatile *pDest, atomic32_t val)
    {
        VOGL_ASSERT((reinterpret_cast<uintptr_t>(pDest) & 3) == 0);
        atomic32_t cur = *pDest;
        *pDest += val;
        return cur;
    }
#endif

} // namespace vogl
