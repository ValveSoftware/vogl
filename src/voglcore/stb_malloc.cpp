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

// stb_malloc.h 0.01 -- public domain multi-heap malloc -- Sean Barrett, 2012
//
// WARNING -- IN THIS VERSION THE STBM_TC STUFF IS UNTESTED AND
// CROSS-THREAD FREES AREN'T CODED -- IGGY DOESN'T USE THOSE FEATURES
// AND THAT'S ALL I'VE CODED/TESTED
//
// A malloc "implementation" that you need to do a bit of configuring
// on to actually make a malloc.
//
// Usage:
//
// Define the preprocessor macros STB_MALLOC_IMPLEMENTATION
// in *one* .c/.cpp file before #including this file, e.g.:
//
//      #define STB_MALLOC_IMPLEMENTATION
//      #include "stb_malloc.h"
//
// Also optionally define the symbols listed in the section
// "Optional definitions" below.
//
//
// Features:
//   - TLSF for mid-sized allocations for O(1) allocation & freeing
//   - large allocations go to user-defined "system allocator"
//   - small allocations (<= pointer-sized) slab-allocated (no coalescing)
//   - no constraints on system allocator (e.g. page-aligned NOT required)
//   - multiple separate heaps which don't interfragment
//     - can free heap all-at-once without freeing individual allocations
//     - can free without specifying which heap it came from
//   - "thread"-cache
//     - must create one thread cache per thread per heap that you want cached
//     - you must create/manage any thread-local variables yourself
//   - almost every allocation offers a 16-bit user-defined "header" field
//   - overhead of 8 bytes in 32-bit, 16 bytes in 64-bit
//     - half that for allocations of 4 bytes in 32-bit, 8 bytes in 64-bit
//       - but they get no 16-bit user field
//     - plus overhead for allocations that aren't multiples of alignment
//   - all configuration at run-time
//     - choose per-heap minimum alignment at heap creation
//     - provide mutex handles or not for thread-safety / non-safety
//     - enable file/line recording on the fly (mixed in one heap)
//     - enable trailing guard bytes on the fly (mixed in one heap)
//     - enable variable-sized user-defined debug data on the fly (mixed)
//     - all on-the-fly-enables incur significant extra size in malloc header
//   - compile time configuration of platform features
//     - definition of a mutex handle and operations to lock/unlock it
//     - definition of a compare-and-swap for faster cross-thread frees
//
// Four possible models:
//     - create single-thread-only heaps without mutexes for fast performance
//     - create mutexed heap with per-thread thread cache for "global" malloc
//       that shares memory between threads
//     - create per-thread single-thread-only heap with cross-thread mutex for
//       fast "global" malloc without sharing memory (no false sharing)
//     - create M heaps for N threads with per-thread thread caches and
//       primary mutexes and cross-thread-mutexes
//
// Optional definitions:
//
//      STBM_MUTEX_HANDLE
//         The type of a handle to a mutex. It must be comparable against
//         0 to mean "is it null". These should be as-efficient-as-possible
//         process-private mutexes, not cross-process mutexes. If not
//         defined, allocation heaps are not thread-safe.
//
//      STBM_MUTEX_ACQUIRE
//         "Lock" a mutex handle. A single thread of stb_malloc will
//         never lock more than one mutex at a time, nor try to lock
//         the same mutex twice (they are not "reentrant"). Must be
//         defined if STBM_MUTEX_HANDLE is defined.
//
//      STBM_MUTEX_RELEASE
//         Release a previously acquired mutex, as above.
//
//      STBM_COMPARE_AND_SWAP32
//         Define a function that performs an atomic compare-and-swap on
//         a 32-bit value. (To be specified, since it's unimplemented so far.)
//         Only used if STBM_MUTEX_HANDLE is defined and a cross-thread
//         mutex is defined for a given heap.
//
//      STBM_UINT32
//      STBM_UINTPTR
//      STBM_POINTER_SIZE
//         If your compiler is C99, you do not need to define these.
//         Otherwise, stb_malloc will try default (32-bit) assignments
//         for them and validate them at compile time. If they are
//         incorrect, you will get compile errors, and will need to
//         define them yourself. STBM_POINTER_SIZE should be 32 or 64.
//
//     STBM_LOG2_32BIT(x)
//         Compute floor of log2 of a 32-bit number, or -1 if 0, i.e.
//         "31-count_leading_zeroes32(x)". This function is important to
//         high-performance of the allocator; it is used for computing block
//         "sizeclasses" and for TLSF O(1) scans. If you do not define it,
//         stb_malloc uses a small binary tree and a small table lookup.
//
//     STBM_ASSERT
//         If you don't define this, stb_malloc uses assert.h and assert().
//         This is the only C standard library function used by stb_malloc.
//
//     STBM_MEMSET
//         You can define this to 'memset' or your own memset replacement;
//         if not, stb_malloc uses a naive (possibly less efficient)
//         implementation.
//
//     STBM_MEMCPY
//         You can define this to 'memcpy' or your own memcpy replacement;
//         if not, stb_malloc uses a naive (probably inefficient)
//         implementation.
//
//     STBM_FAIL(s)
//         This function (which takes a char*) is called when there is an
//         extraordinary failure: a corrupt header is detected, guard bytes
//         are overwritten, or the API is used incorrectly. It is NOT called
//         if an allocation fails, which is considered a normal behavior.
//         If not defined, defaults to STBM_ASSERT(0).
//
//     STBM_CALLBACK
//         Defines an attribute applied to any callback functions, useful
//         for special linkage conventions.
//
//     STBM_NO_TYPEDEF
//         If defined, supresses the stbm_heap typedef for compilers that
//         don't like to see duplicate identical typedefs.
//
//     STBM_STATIC
//         If defined, declares all functions as static, so they can only
//         be accessed from the file that creates the implementation.
//
//     STBM_DEBUGCHECK
//         If defined, enables internal automatic calls to iggy_heap_check
//         to validate the heap after every operation (slow).
//
//
// On The Design Of stb_malloc.h
//
//   I've written many allocators (google for 'satoria smalloc' for the 1st).
//   stb_malloc.h is my most refined, the collision of seven central ideas:
//
//    - feature: multiple heaps, with heap-independent freeing
//    - design: overhead mostly proportional to used memory
//    - design: reuse-oriented API/implementation (i.e. stb_-library-style)
//    - design: all malloc options available without compile-time switches
//    - algorithm: TLSF (two-level segregated fit) for O(1) allocation
//    - algorithm: boundary-tag coalescing for O(1) frees
//    - algorithm: thread-caching inspired by TCMalloc
//
// Implementation information appears after the header section.

#include "vogl_core.h"
#include "vogl_threading.h"
#include "stb_malloc.h"

#define STBM_ASSERT VOGL_ASSERT
#define STBM_FAIL VOGL_FAIL
#define STBM_LOG2_32BIT(x) (31 - static_cast<int>(vogl::math::count_leading_zero_bits(x)))
#define STBM_MEMSET memset
#define STBM_MEMCPY memcpy

#if defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
#define STBM_POINTER_SIZE 64
#else
#define STBM_POINTER_SIZE 32
#endif
#define STBM_UINTPTR uintptr_t

// General notes:
//
//   Between needing to minimize overhead and be O(1), we cannot
//   afford to construct indices over all of the memory space. Also
//   we cannot guarantee that system allocations are contiguous.
//   The former means we must store data immediately before the
//   user pointer; the latter means we don't bother trying to coalesce
//   across system allocation boundaries.
//
//   Allocations that are pointer-sized or smaller are satisfied from
//   a "homogenous heap" (slab) that does no coalescing.
//
//   Allocations >= 1MB are allocated from the system allocator (with
//   additional padding to allow our header). The threshhold is
//   configurable and actually defaults to 256KB, but 1MB is the
//   largest allowed, due to limits of encoding sizes into header.
//
//   Allocations outside the ranges above are allocated out of chunks
//   allocated from the system allocator, using TLSF to locate sufficiently
//   large regions, using boundary tags to free them.
//
//   Specially-aligned blocks waste up to alignment-worth of space
//   using special markup, rather than creating a larger block and freeing
//   pieces on either side of it.
//
//   Debug blocks have special markup to allow storing __FILE__ and __LINE__
//   or other user data without recompilation.
//
//   Thread-cached blocks are left in the 'allocated' state--they are
//   essentially a layer over the regular heaps--and they just construct
//   a singly-linked list in the block's user storage space (which is
//   always at least pointer-sized).
//
//
// Details:
//
// The allocator is split into five separate layered abstractions.
// However, the memory layout is designed accounting for all the layers
// at once, so that we can determine from a pointer what state it's
// in from any of the layers.
//
// == Layer 1:  Raw storage manipulation
//
//    stbm__small_*   - handles allocations <= sizeof(void*)
//    stbm__large_*   - handles allocations >= 1024*1024
//    stbm__medium_*  - handles other allocations
//
// This layer defines three independent allocators. All three allocators
// store their state independently in an stbm_heap structure, and all
// use the system allocator defined in the stbm_heap to create underlying
// storage. stbm__medium and stbm__large both guarantee allocations meet
// the minimum_alignment defined in stbm_heap.
//
// == Layer 2: Heap allocator
//
//    stbm__heap_*
//
// Based on the size of the allocation, dispatches to one of the above
// allocators. Handles cross-thread frees.
//
// == Layer 3: Thread cache
//
//     stbm__tc_*
//
// If defined, allocates from the thread cache, and fills the thread cache
// using layer 2. If undefined, calls layer 2.
//
// == Layer 4: Abstract allocations
//
//     stbm__alloc_*
//
// Handles aligned and debug allocations by making larger allocations
// to layer 3, and takes responsibility for setting the 16-bit userdata
// field
//
// == Layer 5: API
//
// Implements the top-level API functions by dispatching to stbm__alloc
// functions based on the API function and the internal debug settings
// of the heap.
//
//
// ======================================================================
//
// Memory consists of discontinuous blocks called "system allocations".
// System allocations come in two flavors:
//
//         LARGE-ALLOC                      MEDIUM-ALLOC
//      +---------------+                +----------------+
//      | system alloc  |                |  system alloc  |
//      |    header     |                |     header     |
//      +---------------+  stbm__large   +----------------+
//      |  pad-to-align |  user pointer  |  pad-to-align  |  stbm__medium
//      |+-------------+|   |            |+--------------+|  user pointer
//      || large alloc ||   |            || medium alloc ||   |
//      ||    header   ||   |            ||    header    ||   |
//      |+=============+|   |            |+==============+|   |
//      ||  allocated  ||<--+            ||  allocated   ||<--+
//      ||   storage   ||                ||   storage    ||
//      ||             ||                ||              ||
//      ||             ||                ||              ||  stbm__medium
//      ||             ||                |+--------------+|  user pointer
//      ||             ||                || medium alloc ||   |
//      ||             ||                ||    header    ||   |
//      ||             ||                |+==============+|   |
//      ||             ||                ||  allocated   ||<--+
//      ||             ||                ||   storage    ||
//      ||             ||                |+--------------+|
//      ||             ||                || medium alloc ||
//      ||             ||                ||    header    ||
//      ||             ||                |+--------------+|
//      ||             ||                ||+------------+||
//      ||             ||                |||small chunk |||  stbm__small
//      ||             ||                |||  header    |||  user pointer
//      ||             ||                ||+------------+||   |||
//      ||             ||                |||small header|||   |||
//      ||             ||                ||+============+||   |||
//      ||             ||                |||small alloc |||<--+||
//      ||             ||                ||+------------+||    ||
//      ||             ||                |||small header|||    ||
//      ||             ||                ||+============+||    ||
//      ||             ||                |||small alloc |||<---+|
//      ||             ||                ||+------------+||     |
//      ||             ||                |||small header|||     |
//      ||             ||                ||+============+||     |
//      ||             ||                |||small alloc |||<----+
//      ||             ||                ||+------------+||
//      |+-------------+|                |||    ...     |||
//      +---------------+                ||+------------+||
//                                       |+--------------+|
//                                       || medium alloc ||
//  Boundaries indicated as              ||    header    ||
//  "=====" are boundaries that          |+--------------+|
//  free and get functions look          ||+------------+||
//  across to read a pointer-sized       |||aligned base|||
//  "prefix" field which contains        |||  header    |||  stbm__medium
//  tag bits which explain what          ||+------------+||  user pointer
//  type of pointer/block a user         |||pad-to-align|||  with explicit
//  pointer corresponds to.              ||+------------+||  alignment
//                                       |||align alloc |||    |
//                                       |||   header   |||    |
//                                       ||+============+||    |
//                                       |||  aligned   |||<---+
//  Debug blocks are wrapped             ||| allocated  |||
//  similarly to the aligned             |||  storage   |||
//  block shown on the right.            |||            |||
//                                       |||            |||
//                                       ||+------------+||
//                                       |+--------------+|  stbm__medium
//                                       || medium alloc ||  user pointer
//                                       ||    header    ||   |
//                                       |+==============+|   |
//                                       ||  allocated   ||<--+
//                                       ||   storage    ||
//                                       ||              ||
//                                       |+--------------+|
//                                       || fake medium  ||
//                                       || alloc header ||
//                                       |+--------------+|
//                                       +----------------+
//                                       |  unused system |
//                                       |  alloc storage |
//                                       |due to alignment|
//                                       +----------------+
//
//
//
// All allocations have one of the following two structures:
//
//        low-level header data
//        uintptr header "prefix"
//    ==> pointer returned by malloc
//        user data
//
// or:
//
//        low-level header data
//        uintptr header "prefix"
//    ==> high-level extra data
//        [padding]
//        high-level header bits
//    ==> pointer returned by malloc
//        user data
//        optional guard bytes
//
// Where the items with ==> are guaranteed to be aligned
// to the heap minimum alignment (except for small blocks).
//
// The most general case header bits in the "prefix" field
// are:
//
//  [xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx   only in 64-bit]
//   uuuuuuuu uuuuuuuu ssssssss sewbpttt
//
// u = user data for medium/large allocated
// s = size class for medium allocated
// e = extra bit for medium allocated size
//
// ttt = medium (110) small (000) large (100) aligned (001) debug (011)
// b = block is allocated (boundary tag for medium allocated & free blocks)
// p = previous allocated (boundary tag for medium allocated & free blocks)
// w = block is a low-level block containing a high-level "wrapped" block:
//     internal is either debug, aligned, or a chunk of small blocks
//
// In debug-type blocks, bp are repurposed as sub-type.
//
// The tag bit assignments are chosen to allow accelerating joint
// tests ("medium or large" and "debug or aligned").

//#define STBM_SELFTEST

extern "C" {

#ifdef STBM_SELFTEST
#define STBM_UNITTEST
#endif

#if !defined(STBM_DEBUGCHECK) && defined(STBM_UNITTEST)
#define STBM_DEBUGCHECK
#endif

typedef unsigned char stbm__uint8;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#include <stdint.h>

#ifndef STBM_UINT32
typedef uint32_t stbm__uint32;
#endif

#ifndef STBM_UINTPTR
typedef uintptr_t stbm__uintptr;
#endif

#ifndef STBM_POINTER_SIZE
#if UINTPTR_MAX > 0xffffffff
#define STBM_POINTER_SIZE 64
#else
#define STBM_POINTER_SIZE 32
#endif
#endif

#else
#ifndef STBM_UINT32
typedef unsigned int stbm__uint32;
#endif

#ifndef STBM_UINTPTR
typedef size_t stbm__uintptr;
#endif

#ifndef STBM_POINTER_SIZE
#define STBM_POINTER_SIZE 32
#endif
#endif

#ifdef STBM_UINTPTR
typedef STBM_UINTPTR stbm__uintptr;
#endif

#ifdef STBM_UINT32
typedef STBM_UINT32 stbm__uint32;
#endif

typedef int stbm__check_ptrsize[sizeof(void *) == STBM_POINTER_SIZE / 8 ? 1 : -1];
typedef int stbm__check_uintptr[sizeof(stbm__uintptr) == sizeof(void *) ? 1 : -1];
typedef int stbm__check_uint32[sizeof(stbm__uint32) == 4 ? 1 : -1];

#define STBM__SMALLEST_ALLOCATION (sizeof(void *))    // 4 or 8
#define STBM__SMALLEST_BLOCKSIZE (2 * sizeof(void *)) // 8 or 16
#define STBM__SMALL_ALIGNMENT (2 * sizeof(void *))    // 8 or 16
#define STBM__MEDIUM_SMALLEST_BLOCK (4 * sizeof(void *))

#if STBM_POINTER_SIZE == 64
#define STBM__SMALLEST_BLOCKSIZE_LOG2 4
#else
#define STBM__SMALLEST_BLOCKSIZE_LOG2 3
#endif

typedef int stbm__check_smallest_log2[(1 << STBM__SMALLEST_BLOCKSIZE_LOG2) == STBM__SMALLEST_BLOCKSIZE ? 1 : -1];

#ifndef STBM_ASSERT
#include <assert.h>
#define STBM_ASSERT(x) assert(x)
#endif

#ifndef STBM_FAIL
// if this function is called, it represents a bug in the client code
// (failing to obey the heap semantics properly in stbm_free)
static void STBM_FAIL(const char *message)
{
    STBM_ASSERT(0);
}
#endif

#if defined(STBM_MUTEX_HANDLE) && !defined(STBM__NO_MUTEX_HANDLE)
#define STBM__MUTEX
#define STBM__LOCKING(x) x
#endif

#if defined(STBM__MUTEX) && (!defined(STBM_MUTEX_ACQUIRE) || !defined(STBM_MUTEX_RELEASE))
#error "Must define STBM_MUTEX_ACQUIRE and STBM_MUTEX_RELEASE if defining STBM_MUTEX_HANDLE"
#endif

#ifndef STBM__LOCKING
#define STBM__LOCKING(x)
#endif

#ifdef STBM__MUTEX
#define STBM__ACQUIRE(lock)       \
    if (lock)                     \
        STBM_MUTEX_ACQUIRE(lock); \
    else
#define STBM__RELEASE(lock)       \
    if (lock)                     \
        STBM_MUTEX_RELEASE(lock); \
    else
#else
#define STBM__ACQUIRE(lock)
#define STBM__RELEASE(lock)
#endif

#ifdef STBM_DEBUGCHECK
static void stbm__heap_check_locked(stbm_heap *heap);
#define STBM__CHECK_LOCKED(heap) stbm__heap_check_locked(heap)
#else
#define STBM__CHECK_LOCKED(heap)
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Utility functions
//

int stbm__is_pow2(size_t num)
{
    return (num & (num - 1)) == 0;
}

static size_t stbm__round_up_to_multiple_of_power_of_two(size_t num, size_t pow2)
{
    STBM_ASSERT(stbm__is_pow2(pow2));
    return (num + pow2 - 1) & ~(pow2 - 1);
}

#ifdef STBM_LOG2_32BIT
#define stbm__log2_floor(n) STBM_LOG2_32BIT(n)
#else
static int stbm__log2_floor(stbm__uint32 n)
{
    static signed char log2_4[16] = { -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 };
    int t = 0;
    if (n >= (1U << 19))
        t += 20, n >>= 20;
    if (n >= (1U << 9))
        t += 10, n >>= 10;
    if (n >= (1U << 4))
        t += 5, n >>= 5;
    return t + log2_4[n];
}
#endif

// return the index of a zero bit, or <0 if all bits are set
static int stbm__find_any_zero(stbm__uint32 n)
{
    // @OPTIMIZE: we can replace this with an intrinsic that finds
    // any zero in any direction

    // address of left-most zero bit is address of left-most 1 bit of ~n
    return stbm__log2_floor(~n);
}

// find the leftmost set bit which has index >= minbitpos counting from left
// for TLSF this would be clearer counting from the right, but counting from
// the left is guaranteed to be efficient
static int stbm__find_leftmost_one_bit_after_skipping(stbm__uint32 value, stbm__uint32 minbitpos)
{
    stbm__uint32 mask = 0xffffffff >> minbitpos;
    return 31 - stbm__log2_floor(value & mask);
}

// align an address to a specific alignment & offset
static void *stbm__align_address(void *ptr, size_t align, size_t align_off)
{
    stbm__uintptr cur_off;
    stbm__uintptr align_mask = (stbm__uintptr)align - 1;

    union
    {
        void *ptr;
        stbm__uintptr address;
    } v;

    v.ptr = ptr;

    cur_off = (v.address & align_mask);
    v.address += (align_off - cur_off) & align_mask;
    STBM_ASSERT((v.address & align_mask) == align_off);

    return v.ptr;
}

#ifndef STBM_MEMSET
static void STBM_MEMSET(void *mem, unsigned char value, size_t num_bytes)
{
    unsigned char *s = (unsigned char *)mem;
    size_t i;
    for (i = 0; i < num_bytes; ++i)
        s[i] = value;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Size-class computation
//
// For simplicity, we use the same 13*32 size classes for both TLSF and for
// the thread cache, although they actually mean different things.
//
// For TLSF, we need to split the size class into two
// variables, the "index" and the "slot". For the thread
// cache, we store them in a single variable.
//
// The size classes are defined by the following logic:
//
//    if index=0
//        size =              0 + slot * 8
//    else
//        size = (128 << index) + slot * (4 << index)
//

#define STBM__NUM_INDICES 13
#define STBM__NUM_SLOTS 32

stbm__uint32 stbm__size_classes[STBM__NUM_INDICES][STBM__NUM_SLOTS] =
    {
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, },
        { 256, 264, 272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384, 392, 400, 408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, },
        { 512, 528, 544, 560, 576, 592, 608, 624, 640, 656, 672, 688, 704, 720, 736, 752, 768, 784, 800, 816, 832, 848, 864, 880, 896, 912, 928, 944, 960, 976, 992, 1008, },
        { 1024, 1056, 1088, 1120, 1152, 1184, 1216, 1248, 1280, 1312, 1344, 1376, 1408, 1440, 1472, 1504, 1536, 1568, 1600, 1632, 1664, 1696, 1728, 1760, 1792, 1824, 1856, 1888, 1920, 1952, 1984, 2016, },
        { 2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496, 2560, 2624, 2688, 2752, 2816, 2880, 2944, 3008, 3072, 3136, 3200, 3264, 3328, 3392, 3456, 3520, 3584, 3648, 3712, 3776, 3840, 3904, 3968, 4032, },
        { 4096, 4224, 4352, 4480, 4608, 4736, 4864, 4992, 5120, 5248, 5376, 5504, 5632, 5760, 5888, 6016, 6144, 6272, 6400, 6528, 6656, 6784, 6912, 7040, 7168, 7296, 7424, 7552, 7680, 7808, 7936, 8064, },
        { 8192, 8448, 8704, 8960, 9216, 9472, 9728, 9984, 10240, 10496, 10752, 11008, 11264, 11520, 11776, 12032, 12288, 12544, 12800, 13056, 13312, 13568, 13824, 14080, 14336, 14592, 14848, 15104, 15360, 15616, 15872, 16128, },
        { 16384, 16896, 17408, 17920, 18432, 18944, 19456, 19968, 20480, 20992, 21504, 22016, 22528, 23040, 23552, 24064, 24576, 25088, 25600, 26112, 26624, 27136, 27648, 28160, 28672, 29184, 29696, 30208, 30720, 31232, 31744, 32256, },
        { 32768, 33792, 34816, 35840, 36864, 37888, 38912, 39936, 40960, 41984, 43008, 44032, 45056, 46080, 47104, 48128, 49152, 50176, 51200, 52224, 53248, 54272, 55296, 56320, 57344, 58368, 59392, 60416, 61440, 62464, 63488, 64512, },
        { 65536, 67584, 69632, 71680, 73728, 75776, 77824, 79872, 81920, 83968, 86016, 88064, 90112, 92160, 94208, 96256, 98304, 100352, 102400, 104448, 106496, 108544, 110592, 112640, 114688, 116736, 118784, 120832, 122880, 124928, 126976, 129024, },
        { 131072, 135168, 139264, 143360, 147456, 151552, 155648, 159744, 163840, 167936, 172032, 176128, 180224, 184320, 188416, 192512, 196608, 200704, 204800, 208896, 212992, 217088, 221184, 225280, 229376, 233472, 237568, 241664, 245760, 249856, 253952, 258048, },
        { 262144, 270336, 278528, 286720, 294912, 303104, 311296, 319488, 327680, 335872, 344064, 352256, 360448, 368640, 376832, 385024, 393216, 401408, 409600, 417792, 425984, 434176, 442368, 450560, 458752, 466944, 475136, 483328, 491520, 499712, 507904, 516096, },
        { 524288, 540672, 557056, 573440, 589824, 606208, 622592, 638976, 655360, 671744, 688128, 704512, 720896, 737280, 753664, 770048, 786432, 802816, 819200, 835584, 851968, 868352, 884736, 901120, 917504, 933888, 950272, 966656, 983040, 999424, 1015808, 1032192, },
    };
#define STBM__SIZECLASS_MAX 1032192

#define stbm__size_for_index_slot_calc(i, s) ((stbm__uint32)(((i) == 0 ? (s) * 8 : (128 << (i)) + (s) * (4 << (i)))))
#if 1
#define stbm__size_for_index_slot(i, s) stbm__size_classes[i][s] // @OPTIMIZE: better to use table or _calc?
#else
#define stbm__size_for_index_slot(i, s) stbm__size_for_index_slot_calc(i, s)
#endif
#define stbm__size_for_tc_index(i) stbm__size_classes[0][i]

typedef struct
{
    int index, slot;
} stbm__index_slot;

// compute the size class that a given size block can satisfy
static stbm__index_slot stbm__index_slot_for_free_block(size_t size)
{
    stbm__index_slot is;
    if (size < 256)
    {
        is.index = 0;
        is.slot = (stbm__uint32)(size >> 3);
    }
    else
    {
        is.index = stbm__log2_floor((stbm__uint32)size) - 7;
        is.slot = (stbm__uint32)((size - (128 << is.index)) >> (is.index + 2));
    }
    STBM_ASSERT(is.index < STBM__NUM_INDICES && is.slot < STBM__NUM_SLOTS);
    STBM_ASSERT(size >= stbm__size_for_index_slot(is.index, is.slot));
    return is;
}

// compute the size class that is required to satisfy a given request size
// computing the correct index requires rounding up in a complex way; maybe
// it can be done (but I suspect it requires a divide), so for now instead
// of doing it closed-form we'll just compute the round-down one and
// "increment" it if needed
static stbm__index_slot stbm__index_slot_for_request(size_t size)
{
    stbm__index_slot is = stbm__index_slot_for_free_block(size);
    if (size > stbm__size_for_index_slot(is.index, is.slot))
    {
        if (++is.slot == STBM__NUM_SLOTS)
        {
            is.slot = 0;
            ++is.index;
        }
    }
    STBM_ASSERT(is.index < STBM__NUM_INDICES && is.slot < STBM__NUM_SLOTS);
    STBM_ASSERT(size <= stbm__size_for_index_slot(is.index, is.slot));
    return is;
}

static int stbm__tc_index_for_free_block(size_t size)
{
    stbm__index_slot is = stbm__index_slot_for_free_block(size);
    return is.index * STBM__NUM_SLOTS + is.slot;
}

static int stbm__tc_index_for_request(size_t size)
{
    stbm__index_slot is = stbm__index_slot_for_request(size);
    return is.index * STBM__NUM_SLOTS + is.slot;
}

#ifdef STBM_UNITTEST
static void stbm__test_sizes(void)
{
    int i, j;
    for (i = 0; i < 13; ++i)
    {
        for (j = 0; j < 32; ++j)
        {
            if (i || j)
            {
                stbm__uint32 size = stbm__size_for_index_slot(i, j);
                stbm__index_slot is;
                STBM_ASSERT(stbm__size_classes[i][j] == size);
                STBM_ASSERT(stbm__size_for_index_slot_calc(i, j) == size);

                // if the user requests a size exactly at a size class, it should return that sizeclass
                is = stbm__index_slot_for_request(size);
                STBM_ASSERT(is.index == i && is.slot == j);

                // if the user requests a size just smaller than the size class, it should return the sizeclass
                is = stbm__index_slot_for_request(size - 1);
                STBM_ASSERT(is.index == i && is.slot == j);

                // if the user requests a size just larger than the size class, it should return the next sizeclass
                if (i < 12 || j < 31)
                {
                    is = stbm__index_slot_for_request(size + 1);
                    STBM_ASSERT(is.index == i + 1 || is.slot == j + 1);
                }

                // if a free block is exactly at the size class, it should return that size class
                is = stbm__index_slot_for_free_block(size);
                STBM_ASSERT(is.index == i && is.slot == j);

                // if a free block is just larger than the size class, it should return that size class
                is = stbm__index_slot_for_free_block(size + 1);
                STBM_ASSERT(is.index == i && is.slot == j);

                // if a free block is just smaller than the size class, it should return the previous size class
                is = stbm__index_slot_for_free_block(size - 1);
                STBM_ASSERT(is.index + 1 == i || is.slot + 1 == j);
            }
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Define the block data structures
//

#define STBM__SYSTEM_ALLOC_LARGE 0xface0ff5
#define STBM__SYSTEM_ALLOC_MEDIUM 0xfacefac1

typedef struct stbm__system_allocation
{
    stbm__uintptr magic;
    struct stbm__system_allocation *prev;
    struct stbm__system_allocation *next;
    void *begin_pointer; // used for both large and medium
    void *end_pointer;   // only used for iterating medium blocks
    stbm__uintptr size;  // RG 12/02/13: this was used only for medium blocks, now it should be set on large blocks too
} stbm__system_allocation;

typedef struct stbm__small_chunk
{
    void *wrapped_user_pointer; // required for heap walking

    stbm_heap *heap;
    struct stbm__small_chunk *next;
    struct stbm__small_chunk *prev;

    struct stbm__small_freeblock *first_free;
    char *unused_storage;
    stbm__uint32 num_alloc;
    stbm__uint32 full;
} stbm__small_chunk;

typedef struct
{
    void *wrapped_user_pointer;
} stbm__aligned_base;

typedef struct
{
    void *underlying_allocation;
    stbm__uintptr prefix;
    // [user data]
} stbm__aligned_allocated;

typedef struct
{
    // [prefix]
    void *wrapped_user_pointer;
    char *file;
    stbm__uintptr line;
} stbm__debug_base1;

typedef struct
{
    // [prefix]
    stbm__debug_base1 debug1;

    unsigned short num_guard_bytes;
    stbm__uint8 guard_value;
    stbm__uint8 padding;
    stbm__uint32 extra_debug_bytes;
} stbm__debug_base2;

typedef struct stbm__debug_allocated
{
    union
    {
        stbm__debug_base1 *debug1;
        stbm__debug_base2 *debug2;
    } underlying_allocation;
    stbm__uintptr user_size_in_bytes;
    stbm__uintptr prefix;
    // [user data]
} stbm__debug_allocated;

// stbm__lock allows the user to either specify locks
// as structures (e.g. CRITICAL_SECTION) or as mutex
// handles.
typedef STBM_MUTEX_HANDLE stbm__lock;
#define STBM__LOCKFIELD(s) (s)

typedef struct
{
    // linked list sentinels
    stbm__small_chunk full;
    stbm__small_chunk nonfull;

    // single-item cache
    stbm__small_chunk *chunkcache;
} stbm__heap_smalldata;

typedef struct
{
    struct stbm__medium_freeblock *lists[STBM__NUM_INDICES][STBM__NUM_SLOTS]; // 1456 bytes on 32-bit
    stbm__system_allocation *cached_system_alloc;
    stbm__uintptr cached_system_alloc_size;
    stbm__uint32 bitmap[STBM__NUM_INDICES];
    stbm__uint32 toplevel_bitmap;
    stbm__uint32 minimum_freeblock_size; // larger of minimum_alignment and STBM__SMALLEST_ALLOCATION
} stbm__heap_mediumdata;

#define STBM__FREE_STACK_SIZE 16

// NB: changes to this struct must be propogated to stbm_heap_size() macro
struct stbm_heap
{
    // if data is freed "to" other heaps, they store it here and
    // let this heap resolve it, to avoid excess contention on the
    // heap's core data structures

    // ct_free_stack

    void *crossthread_free_stack[STBM__FREE_STACK_SIZE];
    // pick STBM__FREE_STACK_SIZE so there's probably a cacheline boundary here
    stbm__uintptr crossthread_free_pointer_stack_pos;
    char ct_stack_padding[64 - sizeof(stbm__uintptr)];
    // pad to a cacheline boundary

    // ct_free_list
    stbm__lock crossthread_mutex;
    void *crossthread_free_list;
    char ct_list_padding[64 - sizeof(void *) - sizeof(stbm__lock)];

    // alloc_lock
    stbm__lock allocation_mutex;

    // everything below here can be accessed unguarded, assuming
    // two threads don't try to access a single heap at the same time

    // configuration
    size_t smallchunk_size; // typically 4KB
    size_t cur_medium_chunk_size;
    size_t max_medium_chunk_size;

    stbm__uint32 num_guard_bytes;
    stbm__uint32 minimum_alignment;
    stbm__uint32 large_alloc_threshhold;

    stbm__uint8 align_all_blocks_to_minimum;
    stbm__uint8 force_file_line;
    stbm__uint8 force_debug; // force debug blocks always
    stbm__uint8 guard_value;

    stbm__uint8 memset_on_alloc;
    stbm__uint8 memset_on_free;
    stbm__uint8 memset_alloc_value;
    stbm__uint8 memset_free_value;

    stbm__uint8 full_stats;
    stbm__uint8 cache_medium_chunks;
    stbm__uint8 config_padding2;
    stbm__uint8 config_padding3;

    // system allocation
    stbm__system_allocation system_sentinel;
    stbm_system_alloc *system_alloc;
    stbm_system_free *system_free;
    void *user_context;

    // stats
    stbm__uint32 cur_outstanding_allocations_count;
    stbm__uint32 max_outstanding_allocations_count;
    stbm__uintptr cur_outstanding_allocations_bytes;
    stbm__uintptr max_outstanding_allocations_bytes;
    stbm__uintptr total_allocations_count;
    stbm__uintptr total_allocations_bytes;

    // small
    stbm__heap_smalldata small;

    // medium
    stbm__heap_mediumdata medium;
};
typedef int stbm__heap_size_validate[sizeof(stbm_heap) + 60 <= STBM_HEAP_SIZEOF ? 1 : -1];

#define STBM__MIN(a, b) ((a) < (b) ? (a) : (b))
#define STBM__MAX(a, b) ((a) > (b) ? (a) : (b))

static void stbm__stats_init(stbm_heap *heap)
{
    heap->cur_outstanding_allocations_bytes = 0;
    heap->cur_outstanding_allocations_count = 0;
    heap->max_outstanding_allocations_bytes = 0;
    heap->max_outstanding_allocations_count = 0;
    heap->total_allocations_bytes = 0;
    heap->total_allocations_count = 0;
}

static void stbm__stats_update_alloc(stbm_heap *heap, size_t bytes)
{
    heap->cur_outstanding_allocations_count += 1;
    heap->cur_outstanding_allocations_bytes += bytes;
    if (heap->full_stats)
    {
        heap->max_outstanding_allocations_count = STBM__MAX(heap->max_outstanding_allocations_count, heap->cur_outstanding_allocations_count);
        heap->max_outstanding_allocations_bytes = STBM__MAX(heap->max_outstanding_allocations_bytes, heap->cur_outstanding_allocations_bytes);
        heap->total_allocations_count += 1;
        heap->total_allocations_bytes += bytes;
    }
}

static void stbm__stats_update_free(stbm_heap *heap, size_t bytes)
{
    STBM_ASSERT(heap->cur_outstanding_allocations_count >= 1);
    STBM_ASSERT(heap->cur_outstanding_allocations_bytes >= bytes);
    heap->cur_outstanding_allocations_count -= 1;
    heap->cur_outstanding_allocations_bytes -= bytes;
}

//////////////////////////////////////////////////////////////////////////////
//
// Decode the prefix bits
//

// only if allocated
#define STBM__TYPE_small 0 // small gets 0 so 8-aligned heap pointer works
#define STBM__TYPE_aligned 1
#define STBM__TYPE_debug 3
#define STBM__TYPE_large 4
#define STBM__TYPE_medium 6
#define STBM__TYPE_MASK 7
#define STBM__TYPE(x) ((x) & STBM__TYPE_MASK)
#define STBM__TYPE_med_large(x) (STBM__TYPE(x) & 4)
#define STBM__TYPE_align_debug(x) ((x) & 1)

// only if allocated (if allocated, must be true)
#define STBM__PREFIX_VALID(x) (STBM__TYPE(x) != 2 && STBM__TYPE(x) != 5 && STBM__TYPE(x) != 7)

//////////////////////////////////////////////////////////////////////////////
//
//  stbm__heap_*  --   dispatch layer
//
//  This layer dispatches to the three different allocators, and
//  also manages cross-heap frees.
//

static void *stbm__small_alloc(stbm_heap *heap, size_t size);
static void stbm__small_free(stbm_heap *heap, void *ptr);
static void *stbm__medium_alloc(stbm_heap *heap, size_t size);
static void stbm__medium_free(stbm_heap *heap, void *ptr);
static void *stbm__large_alloc(stbm_heap *heap, size_t size);
static void stbm__large_free(stbm_heap *heap, void *ptr);

static void *stbm__heap_alloc(stbm_heap *heap, size_t size)
{
    if (size <= STBM__SMALLEST_ALLOCATION)
        return stbm__small_alloc(heap, size);
    if (size < heap->large_alloc_threshhold)
        return stbm__medium_alloc(heap, size);
    else
        return stbm__large_alloc(heap, size);
}

static void stbm__heap_crossthread_free(stbm_heap *src_heap, void *ptr)
{
    (void)src_heap;
    (void)ptr;
    // use CAS to push it onto the crossthread stack
    STBM_ASSERT(0);
    STBM_FAIL("crossheap frees not implemented");
}

static void stbm__heap_free(stbm_heap *safe_heap, stbm_heap *src_heap, void *ptr)
{
    stbm__uintptr prefix;

    // four cases:
    //     - src_heap == safe_heap
    //       - use regular heap free path
    //     - src_heap != safe_heap && src_heap->crossthread_handle
    //       - use cross-thread free path
    //     - src_heap != safe_heap && src_heap->allocation_handle
    //       - use regular heap free path
    //     - src_heap != safe_heap && neither handle defined
    //       - STBM_FAIL()

    if (src_heap != safe_heap)
    {
        if (src_heap->crossthread_mutex)
        {
            stbm__heap_crossthread_free(src_heap, ptr);
            return;
        }
        if (!src_heap->allocation_mutex)
        {
            STBM_FAIL("Freed block from wrong thread and no mutexes available");
            return;
        }
    }

    prefix = ((stbm__uintptr *)ptr)[-1];
    if (STBM__TYPE(prefix) == STBM__TYPE_medium)
        stbm__medium_free(src_heap, ptr);
    else if (STBM__TYPE(prefix) == STBM__TYPE_small)
        stbm__small_free(src_heap, ptr);
    else if (STBM__TYPE(prefix) == STBM__TYPE_large)
        stbm__large_free(src_heap, ptr);
    else
        STBM_FAIL("Corrupt header found in stbm_free");
}

// only if STBM__TYPE_debug
#define STBM__DEBUG_1 (0 << 3)
#define STBM__DEBUG_2 (1 << 3)
#define STBM__DEBUG_MASK (3 << 3) // room for future expansion
#define STBM__DEBUG(x) ((x) & STBM__DEBUG_MASK)

// only if STBM__TYPE_debug
#define STBM__DEBUG_MAGIC (0x55 << 8)
#define STBM__DEBUG_SETMAGIC(x) (((x) & ~0xff00) | STBM__DEBUG_MAGIC)
#define STBM__DEBUG_TESTMAGIC(x) (((x) & 0xff00) == STBM__DEBUG_MAGIC)

// only if allocated and not small
#define STBM__USERDATA_BITS(x) (((x) & 0xffffffff) >> 16) // mask is a no-op on 32-bit
#define STBM__USERDATA_SETBITS(x, u) (((x) & 0xffff) | ((u) << 16))

//////////////////////////////////////////////////////////////////////////////
//
//  stbm__small_*  --   very tiny allocations, non-coalescing
//
//  1-4 bytes in 32-bit
//  1-8 bytes in 64-bit
//

typedef struct
{
    union
    {
        stbm__uintptr prefix;
        stbm__small_chunk *chunk;
    } u;
} stbm__small_allocated;

typedef struct stbm__small_freeblock
{
    stbm__uintptr prefix;
    struct stbm__small_freeblock *next_free;
} stbm__small_freeblock;

static void stbm__small_init(stbm_heap *heap)
{
    heap->small.full.next = heap->small.full.prev = &heap->small.full;
    heap->small.nonfull.next = heap->small.nonfull.prev = &heap->small.nonfull;
    heap->small.chunkcache = 0;
}

static void stbm__smallchunk_link(stbm__small_chunk *sentinel, stbm__small_chunk *chunk)
{
    chunk->next = sentinel->next;
    chunk->prev = sentinel;
    sentinel->next->prev = chunk;
    sentinel->next = chunk;
}

static void stbm__smallchunk_unlink(stbm__small_chunk *sentinel, stbm__small_chunk *chunk)
{
    (void)sentinel;
    stbm__small_chunk *next = chunk->next;
    stbm__small_chunk *prev = chunk->prev;

    next->prev = prev;
    prev->next = next;
}

#define STBM__SMALL_FREE_BLOCK_MAGIC 1

static void stbm__small_free(stbm_heap *heap, void *ptr)
{
    stbm__small_allocated *b = (stbm__small_allocated *)ptr - 1;
    stbm__small_chunk *c = b->u.chunk, *chunk_to_free = NULL;
    stbm__small_freeblock *f = (stbm__small_freeblock *)b;
    STBM_ASSERT(c->num_alloc > 0);

    STBM__ACQUIRE(heap->allocation_mutex);
    f->next_free = c->first_free;
    f->prefix = STBM__SMALL_FREE_BLOCK_MAGIC;
    c->first_free = f;
    --c->num_alloc;
    if (c->num_alloc)
    {
        if (c->full)
        {
            c->full = 0;
            stbm__smallchunk_unlink(&heap->small.full, c);
            stbm__smallchunk_link(&heap->small.nonfull, c);
        }
    }
    else
    {
        // chunk is empty, free it
        STBM_ASSERT(!c->full);
        stbm__smallchunk_unlink(&heap->small.nonfull, c);
        if (!heap->small.chunkcache)
        {
            heap->small.chunkcache = c; // cache one smallblock chunk for hysteresis
        }
        else
        {
            chunk_to_free = c;
        }
    }
    stbm__stats_update_free(heap, STBM__SMALLEST_ALLOCATION);
    STBM__RELEASE(heap->allocation_mutex);
    // avoid nesting mutexes
    if (chunk_to_free)
        stbm__medium_free(heap, chunk_to_free);
}

static void *stbm__small_alloc(stbm_heap *heap, size_t size)
{
    (void)size;
    stbm__small_chunk *c;
    stbm__small_allocated *b;

    STBM__ACQUIRE(heap->allocation_mutex);

    // find first non-full chunk of small blocks
    c = heap->small.nonfull.next;

    // if non-full list is empty, allocate a new small chunk
    if (c == &heap->small.nonfull)
    {
        if (heap->small.chunkcache)
        {
            // if we cached an entirely-empty smallchunk, use that
            c = heap->small.chunkcache;
            heap->small.chunkcache = NULL;
        }
        else
        {
            // avoid nesting mutexes
            STBM__RELEASE(heap->allocation_mutex);
            c = (stbm__small_chunk *)stbm__medium_alloc(heap, heap->smallchunk_size);
            if (c == NULL)
                return NULL;
            STBM__ACQUIRE(heap->allocation_mutex);
            c->wrapped_user_pointer = NULL; // flags which type this is
            c->heap = heap;
            c->first_free = NULL;
            c->next = c->prev = 0;
            c->num_alloc = 0;
            c->full = 0;
            c->unused_storage = (char *)stbm__align_address((char *)c + heap->smallchunk_size - STBM__SMALL_ALIGNMENT, STBM__SMALL_ALIGNMENT, STBM__SMALL_ALIGNMENT - sizeof(stbm__small_allocated));
            STBM_ASSERT(c->unused_storage + STBM__SMALLEST_ALLOCATION <= (char *)c + heap->smallchunk_size);
        }
        stbm__smallchunk_link(&heap->small.nonfull, c);
    }
    STBM_ASSERT(!c->full); // must be on the nonfull list

    // allocate from free list or from unsubdivided storage
    if (c->first_free)
    {
        stbm__small_freeblock *f = c->first_free;
        c->first_free = f->next_free;
        b = (stbm__small_allocated *)f;
        if (b->u.prefix != STBM__SMALL_FREE_BLOCK_MAGIC)
            STBM_FAIL("Corrupt data structure in stbm_alloc");
        b->u.chunk = c;
    }
    else
    {
        STBM_ASSERT(c->unused_storage);
        b = (stbm__small_allocated *)c->unused_storage;
        b->u.chunk = c;
        STBM_ASSERT((char *)b + size <= (char *)c + heap->smallchunk_size);
        STBM_ASSERT((char *)b >= (char *)(c + 1));
        c->unused_storage -= STBM__SMALLEST_BLOCKSIZE;
        STBM_ASSERT(STBM__SMALLEST_BLOCKSIZE == sizeof(stbm__small_freeblock));
        if (c->unused_storage < (char *)(c + 1))
            c->unused_storage = NULL;
    }
    // check if the chunk became full
    if (c->first_free == NULL && c->unused_storage == NULL)
    {
        stbm__smallchunk_unlink(&heap->small.nonfull, c);
        stbm__smallchunk_link(&heap->small.full, c);
        c->full = 1;
    }
    ++c->num_alloc;
    stbm__stats_update_alloc(heap, STBM__SMALLEST_ALLOCATION);
    STBM__RELEASE(heap->allocation_mutex);
    return b + 1;
}

//////////////////////////////////////////////////////////////////////////////
//
//  stbm__system_*  tracks system allocations for fast full-heap free
//

static void stbm__system_init(stbm_heap *heap)
{
    heap->system_sentinel.next = &heap->system_sentinel;
    heap->system_sentinel.prev = &heap->system_sentinel;
}

static void stbm__system_alloc_link(stbm_heap *heap, stbm__system_allocation *alloc, stbm__uint32 magic)
{
    alloc->next = heap->system_sentinel.next;
    alloc->prev = &heap->system_sentinel;
    heap->system_sentinel.next->prev = alloc;
    heap->system_sentinel.next = alloc;

    alloc->magic = magic;
}

static void stbm__system_alloc_unlink(stbm_heap *heap, stbm__system_allocation *alloc, stbm__uint32 magic)
{
    (void)heap;
    stbm__system_allocation *next = alloc->next;
    stbm__system_allocation *prev = alloc->prev;

    next->prev = prev;
    prev->next = next;

    if (alloc->magic != magic)
        STBM_FAIL("Corrupt block in stbm_free");
}

//////////////////////////////////////////////////////////////////////////////
//
//  stbm__large_*   large allocations -- every large allocation
//                                 is a wrapped system allocation
//

typedef struct
{
    stbm__system_allocation *system_allocation;
    stbm__uintptr user_size_in_bytes;
    stbm_heap *heap;
    stbm__uintptr prefix;
    // [user data]
} stbm__large_allocated;

static void stbm__large_init(stbm_heap *heap)
{
    (void)(sizeof(heap));
}

static void *stbm__large_alloc(stbm_heap *heap, size_t size)
{
    stbm__system_allocation *s;
    void *user_ptr;
    stbm__large_allocated *a;

    size_t user_size;
    size_t overhead = sizeof(stbm__system_allocation) + sizeof(stbm__large_allocated);
    size_t actual = overhead + (heap->minimum_alignment - 1) + size;

    s = (stbm__system_allocation *)heap->system_alloc(heap->user_context, actual, &actual);
    if (s == NULL)
        return NULL;

    s->size = actual;

    user_ptr = stbm__align_address((char *)s + overhead, heap->minimum_alignment, 0);

    user_size = ((char *)s + actual) - ((char *)user_ptr);

    STBM_ASSERT(user_size >= size);
    STBM_ASSERT((char *)user_ptr + size <= (char *)s + actual);

    STBM__ACQUIRE(heap->allocation_mutex);
    stbm__system_alloc_link(heap, s, STBM__SYSTEM_ALLOC_LARGE);
    stbm__stats_update_alloc(heap, user_size);
    STBM__RELEASE(heap->allocation_mutex);

    s->begin_pointer = user_ptr;

    a = ((stbm__large_allocated *)user_ptr) - 1;

    a->system_allocation = s;
    a->heap = heap;
    a->prefix = STBM__TYPE_large;
    a->user_size_in_bytes = user_size;

    return user_ptr;
}

static void stbm__large_free(stbm_heap *heap, void *ptr)
{
    stbm__large_allocated *a = ((stbm__large_allocated *)ptr) - 1;
    if (a->system_allocation->magic != STBM__SYSTEM_ALLOC_LARGE)
        STBM_FAIL("Corrupt block in stbm_free");

    STBM__ACQUIRE(heap->allocation_mutex);
    stbm__system_alloc_unlink(heap, a->system_allocation, STBM__SYSTEM_ALLOC_LARGE);
    stbm__stats_update_free(heap, a->user_size_in_bytes);
    STBM__RELEASE(heap->allocation_mutex);

    heap->system_free(heap->user_context, a->system_allocation, a->system_allocation->size);
}

//////////////////////////////////////////////////////////////////////////////
//
//  stbm__medium_*   medium allocations with TLSF and boundary-tag coalescing
//

// only if STBM__TYPE_medium or if on normal-size free list
#define STBM__PREVIOUS_free (0 << 3)
#define STBM__PREVIOUS_alloc (1 << 3)
#define STBM__PREVIOUS_MASK (1 << 3)

// only if STBM__TYPE_medium or if on normal-size free list
#define STBM__BLOCK_free (0 << 4)
#define STBM__BLOCK_alloc (1 << 4)
#define STBM__BLOCK_MASK (1 << 4)

#define STBM__WRAPPED_false (0 << 5)
#define STBM__WRAPPED_true (1 << 5)
#define STBM__WRAPPED_MASK (1 << 5)
#define STBM__IS_WRAPPED(x) ((x) & STBM__WRAPPED_MASK)

// only if allocated and STBM__TYPE_medium
#define STBM__SIZE_DATA(x) (((x) & 0xffff) >> 6)
#define STBM__SIZE_SETDATA(x, s) (((x) & 0x3f) | ((s) << 6)) // doesn't preserve userdata

#define STBM__SIZEDATA_index(x) ((x) >> 6)
#define STBM__SIZEDATA_slot(x) (((x) >> 1) & 31)
#define STBM__SIZEDATA_extra(x) ((x) & 1)
#define STBM__EXTRA_SIZE (STBM__MEDIUM_SMALLEST_BLOCK >> 1)

// only if normal-sized and free -- this limits us to just under 2MB as the largest allowed free block
#define STBM__MED_FREE_BYTES(x) (((x) >> 2) & 0x1FFFF8)
#define STBM__MED_FREE_SIZECLASS(x) ((x) >> 23)
#define STBM__MED_FREE_SETSIZE(x, bytes, sizeclass) (((x) & 0x1f) | ((size_t)(bytes) << 2) | ((size_t)(sizeclass) << 23))

#define STBM__MEDIUM_CHUNK_PRE_HEADER 0x80000000

// the header for a medium chunk
typedef struct stbm__medium_chunk
{
    stbm__system_allocation *system_alloc; // lets us recover the original chunk pointer
    stbm__uintptr chunk_length_marker;     // tag to allow us to detect that a block is the first block
} stbm__medium_chunk;

typedef struct
{
    stbm_heap *heap;
    stbm__uintptr prefix;
    // [user data]
} stbm__medium_allocated;

typedef struct stbm__medium_freeblock
{
    struct stbm__medium_freeblock *prev_free; // aliases with 'heap' in allocated block
    stbm__uintptr prefix;                     // aliases with 'prefix' in allocated block
    struct stbm__medium_freeblock *next_free; // aliases with first bytes of 'user data' in allocated block
                                              // [unused memory]
                                              //stbm__uintptr size_in_bytes_trailing;     // aliases with last bytes of 'user data' in allocated block
} stbm__medium_freeblock;

// set bitmap bits starting from the left
#define STBM__TLSF_BITMAP_BIT(x) (1 << (31 - (x)))

static void stbm__medium_init(stbm_heap *heap)
{
    STBM_MEMSET(&heap->medium, 0, sizeof(heap->medium));
    heap->medium.minimum_freeblock_size = STBM__MAX(heap->minimum_alignment, STBM__MEDIUM_SMALLEST_BLOCK);
    heap->medium.cached_system_alloc = NULL;
    heap->medium.cached_system_alloc_size = 0;
}

// allocate a new chunk from the system allocator, then set up the header
// and footer so it looks like it's surrounded by allocated blocks (so it
// can't coalesce against them). this is called without any locks taken.
static size_t stbm__medium_init_chunk(stbm_heap *heap, stbm__medium_freeblock **ptr_block, size_t freesize, stbm__system_allocation *s)
{
    size_t start_size;

    stbm__medium_chunk *c;
    void *ptr;
    stbm__medium_allocated *tail;
    stbm__medium_freeblock *block;

    // now we need to create phantom blocks before and after the free region,
    // and all of them need to be heap->minimum_alignment aligned

    // first we want to create a block immediately after the system allocation region
    // find the correctly-aligned user address
    start_size = sizeof(*s) + sizeof(stbm__medium_chunk) + sizeof(stbm__medium_allocated);
    ptr = stbm__align_address((char *)s + start_size, heap->minimum_alignment, 0);

    s->begin_pointer = ptr;
    s->size = (size_t)freesize;

    // back up to the header that corresponds to this user address
    ptr = ((stbm__medium_allocated *)ptr) - 1;
    // and then switch to a free block at the same address
    block = (stbm__medium_freeblock *)ptr;
    // and get the medium chunk header
    c = ((stbm__medium_chunk *)ptr) - 1;

    // setup the medium chunk header
    c->system_alloc = s;

    block->prefix = STBM__BLOCK_free | STBM__PREVIOUS_free;
    // we don't need to set up the size for block because it's about to be allocated from,
    // which will reset it

    // now we want to set up a phantom block at the end of the region, called 'tail'
    //   (a) aligned such that the corresponding user data would be on heap minimum alignment
    //   (b) be positioned so that it doesn't extend past the end of the allocated block

    // find the end of the chunk
    ptr = (char *)s + freesize;
    // retreat by minimum alignment
    ptr = (char *)ptr - heap->minimum_alignment;
    // find the first aligned point AFTER that point, which must be before the end of the block
    ptr = stbm__align_address(ptr, heap->minimum_alignment, 0);
    STBM_ASSERT((char *)ptr < (char *)s + freesize);

    s->end_pointer = ptr;

    tail = ((stbm__medium_allocated *)ptr) - 1;
    STBM_ASSERT((char *)(tail + 1) < (char *)s + freesize);

    tail->prefix = STBM__BLOCK_alloc | STBM__PREVIOUS_free;
    // we don't need to set up the size for tail because nobody will ever look at it

    // change 'freesize' to be the size of block
    freesize = (char *)(tail) - (char *)block;
    STBM_ASSERT((freesize & (heap->minimum_alignment - 1)) == 0);

    // compute proper chunk_length_marker that allows us to recognize a fully-free block
    c->chunk_length_marker = STBM__MEDIUM_CHUNK_PRE_HEADER + freesize;

    *ptr_block = block;

    return freesize;
}

// this is called inside the heap lock, but releases it
static void stbm__medium_free_chunk(stbm_heap *heap, stbm__system_allocation *s)
{
    stbm__system_alloc_unlink(heap, s, STBM__SYSTEM_ALLOC_MEDIUM);

    if (heap->cache_medium_chunks == STBM_MEDIUM_CACHE_one)
    {
        if (heap->medium.cached_system_alloc == NULL)
        {
            heap->medium.cached_system_alloc = s;
            heap->medium.cached_system_alloc_size = s->size;
            STBM__RELEASE(heap->allocation_mutex);
            return;
        }
    }

    STBM__RELEASE(heap->allocation_mutex);
    heap->system_free(heap->user_context, s, s->size);
}

// link 'block' to the correct free list and set up its header / footer
static void stbm__medium_link(stbm_heap *heap, struct stbm__medium_freeblock *block, size_t size)
{
    stbm__index_slot is;
    stbm__medium_freeblock *list;

    STBM_ASSERT(size <= (2 << 20) - 8); // necessary for packing into MED_FREE

    if (size >= STBM__SIZECLASS_MAX)
    {
        is.index = STBM__NUM_INDICES - 1;
        is.slot = STBM__NUM_SLOTS - 1;
    }
    else
    {
        is = stbm__index_slot_for_free_block(size);
    }

    block->prefix = STBM__MED_FREE_SETSIZE(block->prefix, size, is.index * STBM__NUM_SLOTS + is.slot);
    STBM_ASSERT(size == STBM__MED_FREE_BYTES(block->prefix));
    STBM_ASSERT(STBM__MED_FREE_SIZECLASS(block->prefix) == (unsigned)is.index * STBM__NUM_SLOTS + is.slot);

    list = heap->medium.lists[is.index][is.slot];
    heap->medium.lists[is.index][is.slot] = block;
    block->next_free = list;
    block->prev_free = NULL;
    if (list)
        list->prev_free = block;
    else
    {
        // list was empty, so set the non-empty bit -- @OPTIMIZE is it faster to always
        // set both, or should we check whether the toplevel_bitmap needs setting?
        heap->medium.toplevel_bitmap |= (1 << (31 - is.index));
        heap->medium.bitmap[is.index] |= (1 << (31 - is.slot));
    }

    // set the size at the end of the block as well for O(1) coalescing
    ((stbm__uintptr *)((char *)block + size))[-1] = size;
}

// unlink 'block' from its current free list
static void stbm__medium_unlink(stbm_heap *heap, struct stbm__medium_freeblock *block)
{
    int sizeclass = (int)STBM__MED_FREE_SIZECLASS(block->prefix);
    int index = sizeclass >> 5;
    int slot = sizeclass & 31;

    STBM_ASSERT(heap->medium.lists[index][slot] != NULL);

    if (heap->medium.lists[index][slot] != block)
    {
        // the block is not the first one on the list
        stbm__medium_freeblock *next = block->next_free;
        stbm__medium_freeblock *prev = block->prev_free;
        STBM_ASSERT(prev != NULL);
        prev->next_free = next;
        if (next)
            next->prev_free = prev;
    }
    else
    {
        // if the block is the first one on the list, we have to do the
        // linked-list logic differently, but also it may be the ONLY one
        //on the list, so unlinking it may make the list empty, in which
        // case we need to update the bitmaps
        stbm__medium_freeblock *next = block->next_free;
        heap->medium.lists[index][slot] = next;
        if (next)
            next->prev_free = NULL;
        else
        {
            // the list is empty, so update the bitmap
            heap->medium.bitmap[index] &= ~(1 << (31 - slot));
            if (heap->medium.bitmap[index] == 0)
                heap->medium.toplevel_bitmap &= ~(1 << (31 - index));
        }
    }
}

static void stbm__medium_free(stbm_heap *heap, void *ptr)
{
    stbm__uintptr previous_tag;
    stbm__medium_allocated *alloc = ((stbm__medium_allocated *)ptr) - 1;
    stbm__medium_freeblock *block = (stbm__medium_freeblock *)alloc;
    stbm__medium_freeblock *follow;
    stbm__uintptr sizedata = STBM__SIZE_DATA(alloc->prefix);
    size_t size;
    stbm__index_slot is;
    is.index = (int)STBM__SIZEDATA_index(sizedata);
    is.slot = (int)STBM__SIZEDATA_slot(sizedata);
    size = stbm__size_for_index_slot(is.index, is.slot);
    if (STBM__SIZEDATA_extra(sizedata))
        size += STBM__EXTRA_SIZE;

    STBM_ASSERT((size & (heap->minimum_alignment - 1)) == 0);

    STBM__ACQUIRE(heap->allocation_mutex);
    STBM__CHECK_LOCKED(heap);
    stbm__stats_update_free(heap, size - sizeof(*alloc));

    // merge with following block
    follow = (stbm__medium_freeblock *)((char *)block + size);
    STBM_ASSERT((follow->prefix & STBM__PREVIOUS_MASK) == STBM__PREVIOUS_alloc);
    if ((follow->prefix & STBM__BLOCK_MASK) == STBM__BLOCK_free)
    {
        size_t blocksize = STBM__MED_FREE_BYTES(follow->prefix);
        stbm__medium_unlink(heap, follow);
        size += blocksize;
        STBM__CHECK_LOCKED(heap);
    }
    STBM_ASSERT((size & (heap->minimum_alignment - 1)) == 0);

    // merge with previous block
    if ((block->prefix & STBM__PREVIOUS_MASK) == STBM__PREVIOUS_free)
    {
        size_t prev_length = ((stbm__uintptr *)block)[-1];
        // it's possible this is actually the first block in the medium chunk,
        // which is indicated by the bogus size of the previous block
        if (prev_length < STBM__MEDIUM_CHUNK_PRE_HEADER)
        {
            stbm__medium_freeblock *prev = (stbm__medium_freeblock *)((char *)block - prev_length);
            STBM_ASSERT((prev->prefix & STBM__BLOCK_MASK) == STBM__BLOCK_free);
            STBM_ASSERT(STBM__MED_FREE_BYTES(prev->prefix) == prev_length);

            stbm__medium_unlink(heap, prev);
            size = prev_length + size;
            block = prev;
            STBM__CHECK_LOCKED(heap);
            previous_tag = STBM__PREVIOUS_alloc;
        }
        else
        {
            if (heap->cache_medium_chunks != STBM_MEDIUM_CACHE_all)
            {
                // if we're the first block in the chunk, we might have
                // now freed ALL the data in the chunk, in which case we should
                // go ahead and free the chunk entirely

                if (size == prev_length - STBM__MEDIUM_CHUNK_PRE_HEADER)
                {
                    // if this happens, we want to recover the chunk header
                    // and free it, but @TODO with a one-item cache to reduce churn
                    stbm__medium_free_chunk(heap, (((stbm__medium_chunk *)block) - 1)->system_alloc);
                    // the above call releases the heap lock
                    return;
                }
            }
            previous_tag = STBM__PREVIOUS_free;
        }
    }
    else
        previous_tag = STBM__PREVIOUS_alloc;
    STBM_ASSERT((size & (heap->minimum_alignment - 1)) == 0);

    block->prefix = STBM__BLOCK_free | previous_tag;
    ((stbm__medium_allocated *)((char *)block + size))->prefix &= ~STBM__PREVIOUS_MASK;
#if STBM__PREVIOUS_free != 0
#error "above computation is wrong"
#endif

    stbm__medium_link(heap, block, size);

    STBM__CHECK_LOCKED(heap);
    STBM__RELEASE(heap->allocation_mutex);
}

static void *stbm__medium_alloc(stbm_heap *heap, size_t size)
{
    stbm__medium_freeblock *block;
    stbm__medium_allocated *alloc;
    size_t freesize, expected_size;
    int index, slot;
    stbm__index_slot is;

    // switch from user-requested size to physically-needed size

    size += sizeof(stbm__medium_allocated);

    // force the size to be a multiple of the minimum alignment
    size = (size + heap->minimum_alignment - 1) & ~(heap->minimum_alignment - 1);

    is = stbm__index_slot_for_request(size);

    STBM_ASSERT(is.index < STBM__NUM_INDICES);
    STBM_ASSERT(stbm__size_for_index_slot(is.index, is.slot) >= size);
    STBM_ASSERT((stbm__size_for_index_slot(is.index, is.slot) & (STBM__EXTRA_SIZE - 1)) == 0);

    STBM__ACQUIRE(heap->allocation_mutex);
    STBM__CHECK_LOCKED(heap);

    // check whether there's a free block in that slot. note that
    // our bitmaps are indexed with smaller sizes to the left so that
    // the bitscan can be from the left so it's efficient on all
    // processors
    slot = stbm__find_leftmost_one_bit_after_skipping(heap->medium.bitmap[is.index], is.slot);

    if (slot < 32)
    {
        // if we got a valid slot, then we can use that block
        index = is.index;
        block = heap->medium.lists[index][slot];
        freesize = STBM__MED_FREE_BYTES(block->prefix);
        STBM_ASSERT(freesize >= size && freesize >= stbm__size_for_index_slot(is.index, is.slot));
        stbm__medium_unlink(heap, block);
    }
    else
    {
        // otherwise, we have to find a larger index with a non-empty slot
        index = stbm__find_leftmost_one_bit_after_skipping(heap->medium.toplevel_bitmap, is.index + 1);

        if (index < 32)
        {
            // if we got a valid index, find the smallest non-empty slot
            slot = stbm__find_leftmost_one_bit_after_skipping(heap->medium.bitmap[index], 0);
            STBM_ASSERT(slot < 32);
            block = heap->medium.lists[index][slot];
            freesize = STBM__MED_FREE_BYTES(block->prefix);
            STBM_ASSERT(freesize >= size && freesize >= stbm__size_for_index_slot(is.index, is.slot));
            stbm__medium_unlink(heap, block);
        }
        else
        {
            // if no sufficiently sufficiently large blocks exist, allocate a new chunk
            size_t size_with_overhead;
            stbm__system_allocation *s;
            freesize = heap->cur_medium_chunk_size;

            // we'll compute expected_size twice on this path because we compute
            // it redundantly below, but it's better than computing it earlier
            // pointlessly
            expected_size = stbm__size_for_index_slot(is.index, is.slot);

            size_with_overhead = expected_size + heap->minimum_alignment + 256; // inexact extra padding for chunk headers etc

            if (heap->medium.cached_system_alloc_size >= STBM__MAX(freesize, size_with_overhead))
            {
                s = (stbm__system_allocation *)heap->medium.cached_system_alloc;
                freesize = heap->medium.cached_system_alloc_size;
                STBM_ASSERT(s != NULL);
                heap->medium.cached_system_alloc = NULL;
                heap->medium.cached_system_alloc_size = 0;
                freesize = stbm__medium_init_chunk(heap, &block, freesize, s);
            }
            else
            {
                if (size_with_overhead > freesize)
                {
                    freesize = size_with_overhead;
                }
                else
                {
                    // double the amount we allocate next time
                    heap->cur_medium_chunk_size = STBM__MIN(heap->cur_medium_chunk_size * 2, heap->max_medium_chunk_size);
                }

                if (heap->medium.cached_system_alloc)
                {
                    s = (stbm__system_allocation *)heap->medium.cached_system_alloc;
                    heap->system_free(heap->user_context, s, s->size);
                    heap->medium.cached_system_alloc = 0;
                    heap->medium.cached_system_alloc_size = 0;
                }
                STBM__RELEASE(heap->allocation_mutex);

                s = (stbm__system_allocation *)heap->system_alloc(heap->user_context, freesize, &freesize);
                if (s == NULL)
                    return s;

                STBM_ASSERT(freesize >= size && freesize >= size_with_overhead);

                freesize = stbm__medium_init_chunk(heap, &block, freesize, s);

                STBM__ACQUIRE(heap->allocation_mutex);
            }
            stbm__system_alloc_link(heap, s, STBM__SYSTEM_ALLOC_MEDIUM);
        }
    }

    expected_size = stbm__size_for_index_slot(is.index, is.slot);
    STBM_ASSERT(freesize >= size && freesize >= expected_size);

    // now we have a free block 'block' which is not on any lists and whose
    // size is 'freesize'

    alloc = (stbm__medium_allocated *)block;

    // check if we need to split the block -- we only split if the
    // extra piece is larger than STBM__SMALLEST_ALLOCATION and
    // larger than the minimum alignment
    if (freesize >= expected_size + heap->medium.minimum_freeblock_size)
    {
        stbm__medium_freeblock *split;
        void *ptr;
        size_t tail_size;

        // move past allocation plus header to find location of user block after split
        ptr = stbm__align_address((char *)alloc + expected_size + sizeof(stbm__medium_allocated), heap->minimum_alignment, 0);

        // move back to find header corresponding to that user pointer
        ptr = (((stbm__medium_allocated *)ptr) - 1);

        split = (stbm__medium_freeblock *)ptr;
        STBM_ASSERT((char *)split + heap->medium.minimum_freeblock_size <= (char *)block + freesize);

        tail_size = ((char *)block + freesize) - (char *)split;
        freesize = (char *)split - (char *)block;
        STBM_ASSERT(freesize >= expected_size && freesize < expected_size + heap->medium.minimum_freeblock_size);

        split->prefix = STBM__BLOCK_free | STBM__PREVIOUS_free;
        stbm__medium_link(heap, split, tail_size);
    }

    // at this point, alloc points to the block, we just need to set up its header

    // the previous block cannot be free, or we would have coalesced it
    if ((alloc->prefix & STBM__PREVIOUS_MASK) != STBM__PREVIOUS_alloc)
    {
        // unless this block is the first block in the chunk
        STBM_ASSERT(((stbm__uintptr *)alloc)[-1] >= STBM__MEDIUM_CHUNK_PRE_HEADER);
    }

    // check that we can encode the size correctly
    STBM_ASSERT(freesize == expected_size || freesize == expected_size + STBM__EXTRA_SIZE);

    stbm__stats_update_alloc(heap, freesize - sizeof(*alloc));

    alloc->heap = heap;
    alloc->prefix = STBM__TYPE_medium | STBM__BLOCK_alloc | (alloc->prefix & STBM__PREVIOUS_MASK);

    alloc->prefix = STBM__SIZE_SETDATA(alloc->prefix, (is.index << 6) + (is.slot << 1) + (freesize == expected_size + STBM__EXTRA_SIZE ? 1 : 0));

    // and we need to set the following block to know that its previous block is allocated
    ((stbm__medium_allocated *)((char *)block + freesize))->prefix |= STBM__PREVIOUS_alloc;

#if STBM__PREVIOUS_alloc != STBM__PREVIOUS_MASK
#error "the above assignment is wrong"
#endif

    STBM__CHECK_LOCKED(heap);
    STBM__RELEASE(heap->allocation_mutex);
    return alloc + 1;
}

static void stbm__medium_check_list(stbm_heap *heap, int index, int slot)
{
    const char *message = "check";
    (void)message;

    stbm__medium_freeblock *cur = heap->medium.lists[index][slot];
    STBM_ASSERT((cur == NULL) == ((heap->medium.bitmap[index] & (1 << (31 - slot))) == 0));
    while (cur)
    {
        stbm__index_slot is;
        stbm__uintptr prefix = cur->prefix;
        // this block must be free
        if ((prefix & STBM__BLOCK_MASK) != STBM__BLOCK_free)
            STBM_FAIL(message);
        // the previous block cannot be free, unless it is the first block
        if (((stbm__uintptr *)cur)[-1] < STBM__MEDIUM_CHUNK_PRE_HEADER)
            if ((prefix & STBM__PREVIOUS_MASK) == STBM__PREVIOUS_free)
                STBM_FAIL(message);
        // the sizeclass in the block must match the passed-in sizeclass
        if (STBM__MED_FREE_SIZECLASS(prefix) != (unsigned)(index * STBM__NUM_SLOTS + slot))
            STBM_FAIL(message);
        // the linked list must be linked properly
        if (cur->next_free && cur->next_free->prev_free != cur)
            STBM_FAIL(message);
        // it should have the right stored sizeclass
        if ((int)(STBM__MED_FREE_SIZECLASS(prefix) >> 5) != index || (int)(STBM__MED_FREE_SIZECLASS(prefix) & 31) != slot)
            STBM_FAIL(message);
        // if it's not the largest size
        if (index != STBM__NUM_INDICES - 1 || slot != STBM__NUM_SLOTS - 1)
        {
            // then the size it is should generate the same index/slot pair
            is = stbm__index_slot_for_free_block(STBM__MED_FREE_BYTES(prefix));
            if (is.index != index || is.slot != slot)
                STBM_FAIL(message);
        }
        // the next block should have its prev free set
        if (((((stbm__medium_freeblock *)((char *)cur + STBM__MED_FREE_BYTES(prefix)))->prefix) & STBM__PREVIOUS_MASK) != STBM__PREVIOUS_free)
            STBM_FAIL(message);
        cur = cur->next_free;
    }
}

static void stbm__medium_check_all(stbm_heap *heap)
{
    int i, s;
    for (i = 0; i < STBM__NUM_INDICES; ++i)
    {
        if (((heap->medium.toplevel_bitmap & (1 << (31 - i))) == 0) != (heap->medium.bitmap[i] == 0))
            STBM_FAIL("check");
        for (s = 0; s < STBM__NUM_SLOTS; ++s)
            stbm__medium_check_list(heap, i, s);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//  allocation request structure
//
// allocation requests are reformatted into this structure,
// and then each layer only uses the parts of it that are relevant
//

typedef struct
{
    stbm_tc *tc;
    stbm_heap *heap;
    char *file;
    int line;
    size_t extra_debug_size;
    size_t size;
    stbm__uint32 alignment;
    stbm__uint32 align_offset;
    unsigned short userdata;
} stbm__request;

//////////////////////////////////////////////////////////////////////////////
//
//  stbm__tc_*  --   thread-caching layer
//
//  This layer caches allocations.
//
//  It keeps 32 free lists, each storing one unique allocation size class.
//  If we allocate more sizes than that, it'll start discarding cached
//  allocations, abandoning the least-recently-allocated-from list.
//
//  The heuristics for dealing with the cache becoming too large come
//  from TCMalloc. The doubling-number-of-cahed-allocations attempts to
//  keep excess memory usage from being excessive (e.g. if you're allocating
//  strings, you may not get duplicates at all) and to keep things more O(1)
//  in the worst case where we cycle through 33 different allocation sizes
//  (although since there's a fixed number of lists and at most 16 allocs
//  per list, technically it's always O(1) anyway).
//

#define STBM__NUM_TC_INDICES 257 // #256 is 32KB, which matches TCMalloc
#define STBM__MAX_TC_ALLOC_SIZE stbm__size_for_index_slot_calc((STBM__NUM_TC_INDICES >> 5), STBM__NUM_TC_INDICES & 31)

typedef struct stbm__cached_block
{
    struct stbm__cached_block *next;
} stbm__cached_block;

typedef struct stbm__cache
{
    stbm__cached_block *head; // first free block
    stbm__uint8 lowwater;     // smallest number of free blocks since last scavenge
    stbm__uint8 num;          // number of free blocks
    stbm__uint8 alloc;        // number of blocks allocated on last refill
    stbm__uint8 padding;
} stbm__cache;

#define STBM__NUM_THREADCACHE 32
#define STBM__NUM_TC_SIZECLASS (STBM__NUM_SLOTS *STBM__NUM_INDICES)

struct stbm_tc
{
    // caches for a subset of sizeclasses
    stbm__cache cache[STBM__NUM_THREADCACHE]; // 256 bytes in 32-bit

    // further parallel cache[] data, separated for different access patterns
    unsigned short last_used[STBM__NUM_THREADCACHE]; // 64 bytes
    unsigned short sizeclass[STBM__NUM_THREADCACHE]; // 64 bytes

    stbm_heap *heap;
    stbm__uint32 bitmap;      // which caches are in use
    stbm__uint32 lru_counter; // a counter that updates every time we allocate from a list
    stbm__uint32 cached_bytes;
    stbm__uint32 cached_bytes_scavenge_threshhold;
    signed char slot_for_sizeclass[STBM__NUM_TC_INDICES]; // 257 bytes
};                                                        // ~500/1000 bytes 32/64-bit
typedef int stbm__tc_size_validate[sizeof(stbm_tc) <= STBM_TC_SIZEOF ? 1 : -1];

static void stbm__tc_update_lowwater_mark(stbm__cache *sizecache)
{
    if (sizecache->num < sizecache->lowwater)
        sizecache->lowwater = sizecache->num;
}

static void stbm__tc_free_cached_blocks(stbm_tc *tc, stbm_heap *safe_heap, int tc_slot, int count)
{
    stbm__cached_block *cur;
    stbm__cache *sizecache = &tc->cache[tc_slot];
    int i;

    STBM_ASSERT(count >= 0);

    // @OPTIMIZE: free a whole list in one operation (reduces locking in heap)
    cur = sizecache->head;
    for (i = 0; i < count; ++i)
    {
        stbm__cached_block *next = cur->next;
        stbm__heap_free(safe_heap, tc->heap, cur);
        cur = next;
    }
    sizecache->head = cur;

    sizecache->num -= (stbm__uint8)count;
    stbm__tc_update_lowwater_mark(sizecache);

    tc->cached_bytes -= (stbm__uint32)(count * stbm__size_for_tc_index(tc->sizeclass[tc_slot]));
}

// Because we only keep 32 free lists for 257 size classes,
// we need to be able to discard an existing free list to
// use for a different size class. To do that, we keep
// track of how recently used each list is.
static void stbm__tc_lru_tick(stbm_tc *tc)
{
    if (++tc->lru_counter >= 0x10000)
    {
        int i;
        // every 32K allocations, we'll rewrite all 32 last_used values--this way
        // we avoid having to update LRU linked lists on every alloc
        for (i = 0; i < STBM__NUM_THREADCACHE; ++i)
            // remap last_used values from 0..ffff to 0..7fff
            tc->last_used[i] >>= 1;
        tc->lru_counter = 0x8000; // now lru counter is larger than any existing value
    }
}

static int stbm__tc_free_lru_slot(stbm_tc *tc, stbm_heap *safe_heap)
{
    int sizeclass;
    // find the LRU slot
    int lru = 0x10000, lru_slot = 0, i;
    for (i = 0; i < STBM__NUM_THREADCACHE; ++i)
    {
        if (tc->last_used[i] < lru)
        {
            lru = tc->last_used[i];
            lru_slot = i;
        }
    }

    // free the cached blocks in the slot
    if (tc->cache[lru_slot].num)
        stbm__tc_free_cached_blocks(tc, safe_heap, lru_slot, tc->cache[lru_slot].num);

    sizeclass = tc->sizeclass[lru_slot];
    STBM_ASSERT(tc->slot_for_sizeclass[sizeclass] == lru_slot);
    tc->slot_for_sizeclass[sizeclass] = -1;

    return lru_slot;
}

static int stbm__tc_allocate_slot(stbm_tc *tc, int tc_index, stbm_heap *safe_heap)
{
    // once we're in the steady state, all bitmap bits are set, so make that the fast path
    int tc_slot = (~tc->bitmap ? stbm__find_any_zero(tc->bitmap) : -1);
    if (tc_slot < 0 || tc_slot >= STBM__NUM_THREADCACHE)
        tc_slot = stbm__tc_free_lru_slot(tc, safe_heap);
    else
        tc->bitmap |= 1 << tc_slot;
    STBM_ASSERT(tc->bitmap & (1 << tc_slot));
    STBM_ASSERT(tc->cache[tc_slot].num == 0);
    tc->cache[tc_slot].head = 0;
    tc->cache[tc_slot].num = 0;
    tc->cache[tc_slot].alloc = 0;
    tc->cache[tc_slot].lowwater = 0;
    tc->slot_for_sizeclass[tc_index] = (signed char)tc_slot;
    return tc_slot;
}

static void stbm__tc_alloc_cached_blocks(stbm_tc *tc, int tc_slot, int sizeclass)
{
    size_t size = stbm__size_for_tc_index(sizeclass);
    stbm__cached_block *head;
    stbm__uint8 num_alloc_uc;
    int i, num_alloc;
    if (!tc->cache[tc_slot].alloc)
        // if we've never allocated any of this size before, allocate 2
        num_alloc = 1;
    else
    {
        // allocate twice as many as before, up to a max of 16 or 64 KB
        num_alloc = 2 * tc->cache[tc_slot].alloc;
        if (num_alloc >= 16 || size * num_alloc >= 65536)
            num_alloc = tc->cache[tc_slot].alloc;
    }

    // @OPTIMIZE: provide a function to allocate multiple so we don't
    // have to take the lock multiple times (and possibly can even
    // allocate one big block and subdivide it)
    head = tc->cache[tc_slot].head;
    for (i = 0; i < num_alloc; ++i)
    {
        stbm__cached_block *p = (stbm__cached_block *)stbm__heap_alloc(tc->heap, size);
        if (p == NULL)
        {
            num_alloc = i;
            break;
        }
        head->next = p;
        head = p;
    }
    tc->cache[tc_slot].head = head;

    num_alloc_uc = (stbm__uint8)num_alloc;
    tc->cache[tc_slot].num += num_alloc_uc;
    tc->cache[tc_slot].alloc = num_alloc_uc;
    tc->cache[tc_slot].lowwater = num_alloc_uc;
}

static void *stbm__tc_alloc(stbm__request *request)
{
    stbm_tc *tc = request->tc;
    if (tc && tc->heap == request->heap && request->size <= STBM__MAX_TC_ALLOC_SIZE)
    {
        stbm__cached_block *p;
        stbm__cache *sizecache;

        int tc_index = stbm__tc_index_for_request(request->size);
        int tc_slot = tc->slot_for_sizeclass[tc_index];

        if (tc_slot < 0)
            tc_slot = stbm__tc_allocate_slot(tc, tc_index, tc->heap); // can't fail

        // update our last_used now so tc_slot is most-recently-used
        tc->last_used[tc_slot] = (unsigned short)tc->lru_counter;

        sizecache = &tc->cache[tc_slot];
        if (sizecache->num == 0)
        {
            stbm__tc_alloc_cached_blocks(tc, tc_slot, tc_index);
            if (sizecache->num == 0)
                return 0; // if allocation fails, we should bail
        }
        // tc_slot is dead

        // update how much data we have; tc_index is dead past here
        tc->cached_bytes -= stbm__size_for_tc_index(tc_index);

        // update our lru timer; tc is dead past here
        stbm__tc_lru_tick(tc);

        // do accounting on free list
        --sizecache->num;
        stbm__tc_update_lowwater_mark(sizecache);

        // take head of free list
        p = sizecache->head;
        sizecache->head = p->next;

        return p;
    }
    return stbm__heap_alloc(request->heap, request->size);
}

// TCmalloc-style scavenging logic -- free half the
// unused objects in each list
static void stbm__tc_scavenge(stbm_tc *tc, stbm_heap *safe_heap)
{
    int i;
    for (i = 0; i < STBM__NUM_THREADCACHE; ++i)
    {
        if (tc->sizeclass[i] && tc->cache[i].num)
        {
            stbm__uint32 count = (tc->cache[i].lowwater + 1) >> 1;
            if (count)
            {
                if (count > tc->cache[i].num)
                    count = tc->cache[i].num;
                stbm__tc_free_cached_blocks(tc, safe_heap, i, count);
            }
        }
    }
}

static void stbm__tc_free(stbm_tc *tc, stbm_heap *safe_heap, void *ptr)
{
    // @OPTIMIZE: merge these two computations, which branch on the same cases
    stbm_heap *src_heap = stbm_get_heap(ptr);
    size_t size = stbm_get_allocation_size(ptr);

    if (tc && tc->heap == src_heap && size <= STBM__MAX_TC_ALLOC_SIZE)
    {
        stbm__cache *sizecache;
        stbm__cached_block *p = (stbm__cached_block *)ptr;
        int tc_index = stbm__tc_index_for_free_block(size);
        int tc_slot = tc->slot_for_sizeclass[tc_index];
        if (tc_slot < 0)
            tc_slot = stbm__tc_allocate_slot(tc, tc_index, safe_heap);
        sizecache = &tc->cache[tc_slot];
        p->next = sizecache->head;
        sizecache->head = p;
        ++sizecache->num;
        tc->cached_bytes += stbm__size_for_tc_index(tc_index);
        if (tc->cached_bytes >= tc->cached_bytes_scavenge_threshhold)
            stbm__tc_scavenge(tc, safe_heap);
    }
    else
        stbm__heap_free(safe_heap, src_heap, ptr);
}

//////////////////////////////////////////////////////////////////////////////
//
//  stbm__alloc_*
//
//  Allocates and frees debug & aligned blocks, and manages debug modes.
//  Also takes responsibility for setting the 16-bit userdata field.
//

#ifdef STBM_DEBUGCHECK
#define stbm__heap_check_internal(x) stbm_heap_check(x)
#else
#define stbm__heap_check_internal(x)
#endif

static void *stbm__alloc_normal(stbm__request *request)
{
    void *ptr;
    size_t size = request->size;
    if (size <= STBM__SMALLEST_ALLOCATION)
    {
        if (!request->userdata)
            return stbm__tc_alloc(request);
        request->size = STBM__SMALLEST_ALLOCATION + 1;
    }
    ptr = stbm__tc_alloc(request);
    if (ptr)
    {
        stbm__uintptr *prefix = (stbm__uintptr *)ptr - 1;
        STBM_ASSERT(STBM__TYPE(*prefix) != STBM__TYPE_small);
        *prefix = STBM__USERDATA_SETBITS(*prefix, request->userdata);
        stbm__heap_check_internal(request->heap);
    }
    return ptr;
}

// allocate a block with non-default alignment
static void *stbm__alloc_aligned_internal(stbm__request *request, size_t header_bytes, size_t footer_bytes, void **base_ptr)
{
    void *base;

    size_t align, align_off;
    size_t extra_bytes;

    // canonicalize the requested alignment
    if (request->alignment == 0)
    {
        align = request->heap->minimum_alignment;
        align_off = 0;
    }
    else
    {
        align = request->alignment;
        align_off = request->align_offset;
    }

    // suppose the underlying alloc is aligned to 'align', then compute
    // the number of bytes we need to guarantee userdata is correctly aligned
    extra_bytes = stbm__round_up_to_multiple_of_power_of_two(header_bytes - align_off, align) + align_off;

    // now, provide enough bytes to allow for the underlying alloc not being
    // sufficiently aligned
    if (align > request->heap->minimum_alignment)
    {
        // e.g. if they request 32 and default is 8, we might need 24 bytes to force alignment
        extra_bytes += align - request->heap->minimum_alignment;
    }

    // now, work out the full allocation size
    request->size = extra_bytes + request->size + footer_bytes;
    base = stbm__tc_alloc(request);
    if (base == NULL)
        return base;

    *base_ptr = base;

    // set the 'wrapped' flag so the heap walk can detect wrapped objects
    ((stbm__uintptr *)base)[-1] |= STBM__WRAPPED_true;

    return stbm__align_address((char *)base + header_bytes, align, align_off);
}

static void *stbm__alloc_align(stbm__request *request)
{
    void *base;
    size_t old_size = request->size;
    void *user_ptr = stbm__alloc_aligned_internal(request, sizeof(stbm__aligned_allocated) + sizeof(stbm__aligned_base), 0, &base);
    stbm__aligned_allocated *aa = ((stbm__aligned_allocated *)user_ptr) - 1;
    stbm__aligned_base *ab = ((stbm__aligned_base *)base);
    (void)(sizeof(old_size)); // avoid warning

    if (user_ptr == NULL)
        return user_ptr;

    aa->underlying_allocation = base;
    aa->prefix = STBM__USERDATA_SETBITS(STBM__TYPE_aligned, request->userdata);
    ab->wrapped_user_pointer = user_ptr;

    // verify that both blocks of memory (aa & user) fit inside allocated block
    STBM_ASSERT((char *)user_ptr + old_size <= (char *)base + request->size);
    STBM_ASSERT((char *)aa >= (char *)base);

    stbm__heap_check_internal(request->heap);

    return user_ptr;
}

static void stbm__alloc_check_guard(stbm__debug_allocated *da, const char *message)
{
    (void)message;
    size_t num_bytes;

    if (STBM__DEBUG(da->prefix) != STBM__DEBUG_2)
        return;

    num_bytes = da->underlying_allocation.debug2->num_guard_bytes;
    if (!num_bytes)
        return;

    if (num_bytes & 0xf00f)
    {
        STBM_FAIL("Invalid guard byte count in stbm_free");
    }
    else
    {
        unsigned char c = da->underlying_allocation.debug2->guard_value;
        unsigned char *guard_area = (unsigned char *)(da + 1) + da->user_size_in_bytes;
        size_t i;
        num_bytes >>= 4;
        for (i = 0; i < num_bytes; ++i)
            if (guard_area[i] != c)
                break;
        if (i < num_bytes)
            STBM_FAIL(message);
    }
}

static void stbm__alloc_link_debug_allocation(stbm__request *request, stbm__debug_allocated *da)
{
    stbm__debug_base1 *db = da->underlying_allocation.debug1;
    db->wrapped_user_pointer = da + 1;
    da->underlying_allocation.debug1->file = request->file;
    da->underlying_allocation.debug1->line = request->line;
}

static void *stbm__alloc_debug1(stbm__request *request)
{
    void *base;
    size_t user_size = request->size;
    size_t header_bytes = sizeof(stbm__debug_base1) + sizeof(stbm__debug_allocated);
    void *user_ptr = stbm__alloc_aligned_internal(request, header_bytes, 0, &base);
    stbm__debug_allocated *da = ((stbm__debug_allocated *)user_ptr) - 1;

    if (user_ptr == NULL)
        return user_ptr;

    // verify that both blocks of memory (aa & user) fit inside allocated block
    STBM_ASSERT((char *)user_ptr + user_size <= (char *)base + request->size);
    STBM_ASSERT((char *)da >= (char *)base);

    da->underlying_allocation.debug1 = ((stbm__debug_base1 *)base);
    da->user_size_in_bytes = user_size;
    da->prefix = STBM__USERDATA_SETBITS(STBM__TYPE_debug | STBM__DEBUG_1, request->userdata);
    da->prefix = STBM__DEBUG_SETMAGIC(da->prefix);

    stbm__alloc_link_debug_allocation(request, da);
    stbm__heap_check_internal(request->heap);
    return user_ptr;
}

static void *stbm__alloc_debug2(stbm__request *request)
{
    void *base;
    size_t excess, user_size = request->size;
    size_t extra_debug = stbm__round_up_to_multiple_of_power_of_two(request->extra_debug_size, sizeof(void *));
    int header_bytes = (int)(sizeof(stbm__debug_base2) + sizeof(stbm__debug_allocated) + extra_debug);
    void *user_ptr = stbm__alloc_aligned_internal(request, header_bytes,
                                                  request->heap->num_guard_bytes, &base);
    stbm__debug_base2 *db2 = ((stbm__debug_base2 *)base);
    stbm__debug_allocated *da = ((stbm__debug_allocated *)user_ptr) - 1;

    if (user_ptr == NULL)
        return user_ptr;

    // verify that all blocks of memory (aa & user) fit inside allocated block and don't overlap
    STBM_ASSERT((char *)user_ptr + user_size + request->heap->num_guard_bytes <= (char *)base + request->size);
    STBM_ASSERT((char *)(db2 + 1) + request->extra_debug_size <= (char *)da);

    da->underlying_allocation.debug2 = db2;
    da->user_size_in_bytes = user_size;
    da->prefix = STBM__USERDATA_SETBITS(STBM__TYPE_debug | STBM__DEBUG_2, request->userdata);
    da->prefix = STBM__DEBUG_SETMAGIC(da->prefix);

    db2->extra_debug_bytes = (stbm__uint32)request->extra_debug_size;
    db2->padding = 0x57;
    excess = stbm_get_allocation_size(user_ptr) - user_size;

    STBM_ASSERT(excess >= request->heap->num_guard_bytes);
    if (excess)
    {
        // we only room for a 16-bit length, so we limit it to 255 guard bytes, and we encode that specially
        int num_guard_bytes = (excess > 255) ? 255 : (int)excess;
        db2->num_guard_bytes = (unsigned short)(num_guard_bytes << 4);
        db2->guard_value = request->heap->guard_value;
        STBM_MEMSET((char *)user_ptr + user_size, db2->guard_value, (size_t)num_guard_bytes);
    }
    else
        db2->num_guard_bytes = 0;

    stbm__alloc_link_debug_allocation(request, da);
    stbm__heap_check_internal(request->heap);
    return user_ptr;
}

static void *stbm__alloc_debug(stbm__request *request)
{
    if (request->heap->num_guard_bytes)
    {
        request->extra_debug_size = 0;
        return stbm__alloc_debug2(request);
    }
    else
        return stbm__alloc_debug1(request);
}

static void stbm__alloc_free_aligned(stbm_tc *tc, stbm_heap *safe_heap, void *ptr)
{
    stbm__aligned_allocated *aa = ((stbm__aligned_allocated *)ptr) - 1;
    stbm__tc_free(tc, safe_heap, aa->underlying_allocation);
}

static void stbm__alloc_free_debug(stbm_tc *tc, stbm_heap *safe_heap, void *ptr)
{
    stbm__debug_allocated *da = ((stbm__debug_allocated *)ptr) - 1;
    if (!STBM__DEBUG_TESTMAGIC(da->prefix))
        STBM_FAIL("Invalid block header in stbm_free");
    stbm__alloc_check_guard(da, "Invalid trailing guard bytes in stbm_free");
    stbm__tc_free(tc, safe_heap, da->underlying_allocation.debug1);
}

//////////////////////////////////////////////////////////////////////////////
//
// API functions
//

STBM__API void *stbm_alloc(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16)
{
    stbm__request r;
    r.tc = tc;
    r.heap = heap;
    r.size = size;
    r.userdata = userdata16;

    if (heap->force_debug)
    {
        r.file = 0;
        r.line = 0;
        r.alignment = 0;
        return stbm__alloc_debug(&r);
    }

    // force small blocks to be larger if they need larger alignment
    if (size <= STBM__SMALLEST_ALLOCATION)
        if (heap->align_all_blocks_to_minimum)
            if (heap->minimum_alignment > STBM__SMALL_ALIGNMENT)
                r.size = STBM__SMALLEST_ALLOCATION + 1;

    return stbm__alloc_normal(&r);
}

STBM__API void *stbm_alloc_fileline(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, char *file, int line)
{
    stbm__request r;
    r.tc = tc;
    r.heap = heap;
    r.size = size;
    r.userdata = userdata16;

    if (heap->force_file_line || heap->force_debug)
    {
        r.file = file;
        r.line = line;
        r.alignment = 0;
        return stbm__alloc_debug(&r);
    }

    // force small blocks to be larger if they need larger alignment
    if (size <= STBM__SMALLEST_ALLOCATION)
        if (heap->align_all_blocks_to_minimum)
            if (heap->minimum_alignment > STBM__SMALL_ALIGNMENT)
                r.size = STBM__SMALLEST_ALLOCATION + 1;

    return stbm__alloc_normal(&r);
}

STBM__API void *stbm_alloc_align(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, size_t alignment, size_t align_offset)
{
    stbm__request r;
    r.tc = tc;
    r.heap = heap;
    r.size = size;
    r.userdata = userdata16;
    r.alignment = (stbm__uint32)alignment;
    r.align_offset = (stbm__uint32)align_offset;

    if (heap->force_debug)
    {
        r.file = 0;
        r.line = 0;
        return stbm__alloc_debug(&r);
    }

    // check if it's non-aligned
    if (align_offset == 0 && alignment <= heap->minimum_alignment)
        if (size > STBM__SMALLEST_ALLOCATION || !heap->align_all_blocks_to_minimum)
            return stbm__alloc_normal(&r);

    return stbm__alloc_align(&r);
}

STBM__API void *stbm_alloc_align_fileline(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, size_t alignment, size_t align_offset, char *file, int line)
{
    stbm__request r;
    r.tc = tc;
    r.heap = heap;
    r.size = size;
    r.userdata = userdata16;
    r.alignment = (stbm__uint32)alignment;
    r.align_offset = (stbm__uint32)align_offset;

    if (heap->force_file_line || heap->force_debug)
    {
        r.file = file;
        r.line = line;
        return stbm__alloc_debug(&r);
    }

    // check if it's non-aligned
    if (align_offset == 0 && alignment <= heap->minimum_alignment)
        if (size > STBM__SMALLEST_ALLOCATION || !heap->align_all_blocks_to_minimum)
            return stbm__alloc_normal(&r);

    return stbm__alloc_align(&r);
}

STBM__API void *stbm_alloc_debug(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, size_t alignment, size_t align_offset, char *file, int line, size_t extra_debug_size)
{
    (void)file;
    (void)line;
    stbm__request r;
    r.tc = tc;
    r.heap = heap;
    r.file = 0;
    r.line = 0;
    r.size = size;
    r.userdata = userdata16;
    r.alignment = (stbm__uint32)alignment;
    r.align_offset = (stbm__uint32)align_offset;
    r.extra_debug_size = extra_debug_size;
    if (extra_debug_size || heap->num_guard_bytes)
        return stbm__alloc_debug2(&r);
    else
        return stbm__alloc_debug1(&r);
}

STBM__API void stbm_free(stbm_tc *tc, stbm_heap *safe_heap, void *ptr)
{
    if (ptr)
    {
        stbm__uintptr prefix = ((stbm__uintptr *)ptr)[-1];

        if (!STBM__TYPE_align_debug(prefix))
            stbm__tc_free(tc, safe_heap, ptr);
        else if (STBM__TYPE(prefix) == STBM__TYPE_aligned)
            stbm__alloc_free_aligned(tc, safe_heap, ptr);
        else if (STBM__TYPE(prefix) == STBM__TYPE_debug)
            stbm__alloc_free_debug(tc, safe_heap, ptr);
        else
            STBM_FAIL("Invalid block header in stbm_free");
    }

    if (safe_heap)
    {
        stbm__heap_check_internal(safe_heap);
    }
}

STBM__API void *stbm_realloc(stbm_tc *tc, stbm_heap *heap, void *oldptr, size_t newsize, unsigned short userdata16)
{
    if (oldptr == 0)
    {
        return stbm_alloc(tc, heap, newsize, userdata16);
    }
    else if (newsize == 0)
    {
        stbm_free(tc, heap, oldptr);
        return 0;
    }
    else
    {
        size_t copysize;
        void *newptr;
        size_t oldsize = stbm_get_allocation_size(oldptr);
        if (newsize <= oldsize && oldsize <= newsize * 2)
            return oldptr;

        newptr = stbm_alloc(tc, heap, newsize, userdata16);
        if (newptr == NULL)
            return newptr;

        copysize = STBM__MIN(oldsize, newsize);

#ifdef STBM_MEMCPY
        STBM_MEMCPY(newptr, oldptr, copysize);
#else
        {
            size_t i;
            for (i = 0; i < copysize; ++i)
                ((char *)newptr)[i] = ((char *)oldptr)[i];
        }
#endif

        stbm_free(tc, heap, oldptr);
        return newptr;
    }
}

unsigned short stbm_get_userdata(void *ptr)
{
    stbm__uintptr prefix = ((stbm__uintptr *)ptr)[-1];

    if (STBM__TYPE(prefix) == STBM__TYPE_small)
        return 0;
    else
        return STBM__USERDATA_BITS(prefix);
}

STBM__API size_t stbm_get_allocation_size(void *ptr)
{
    stbm__uintptr prefix = ((stbm__uintptr *)ptr)[-1];

    if (STBM__TYPE(prefix) == STBM__TYPE_medium)
    {
        // this case is slightly complicated by the fact that a
        // TLSF allocation might return slightly more data,
        // and the extra isn't enough to make it to the next
        // sizeclass, and also isn't enough to split off a
        // separate block--leaving us to have to explicitly store
        // the extra. most allocators explicitly store the actual
        // size in the header so this isn't a problem, but we
        // want to leave room for userdata, so we only store
        // the sizeclass. (with a max of 1MB, 8-byte aligned,
        // we would need 17 bits to store it exactly.)
        static size_t extra[2] = { 0, STBM__EXTRA_SIZE };
        stbm__uintptr size = STBM__SIZE_DATA(prefix);
        return stbm__size_for_index_slot(STBM__SIZEDATA_index(size),
                                         STBM__SIZEDATA_slot(size)) +
               extra[STBM__SIZEDATA_extra(size)] - sizeof(stbm__medium_allocated);
    }

    if (STBM__TYPE(prefix) == STBM__TYPE_small)
        return STBM__SMALLEST_BLOCKSIZE - sizeof(stbm__small_allocated);

    if (STBM__TYPE(prefix) == STBM__TYPE_large)
        return ((stbm__large_allocated *)ptr)[-1].user_size_in_bytes;

    if (STBM__TYPE(prefix) == STBM__TYPE_debug)
        return ((stbm__debug_allocated *)ptr)[-1].user_size_in_bytes;

    if (STBM__TYPE(prefix) == STBM__TYPE_aligned)
    {
        // we have to compute this because we didn't store it explicitly
        // to save memory.

        // find the size of the underlying allocation:
        void *base = ((stbm__aligned_allocated *)ptr)[-1].underlying_allocation;
        size_t size = stbm_get_allocation_size(base);

        // the end of the underlying allocation is also the end of this allocation,
        // so the length of this allocation is computed as end - begin:
        return ((char *)base + size) - (char *)ptr;
    }

    STBM_ASSERT(!STBM__PREFIX_VALID(prefix));
    STBM_FAIL("Corrupt malloc header in stbm_get_allocation_size");
    return 0;
}

stbm_heap *stbm_get_heap(void *ptr)
{
    stbm__uintptr prefix = ((stbm__uintptr *)ptr)[-1];

    // the heap pointer is stored in the same spot for med&large, and we can test their type in one test:
    STBM_ASSERT(&((stbm__medium_allocated *)ptr)[-1].heap == &((stbm__large_allocated *)ptr)[-1].heap);
    if (STBM__TYPE_med_large(prefix))
        return ((stbm__medium_allocated *)ptr)[-1].heap;

    if (STBM__TYPE(prefix) == STBM__TYPE_small)
        return ((stbm__small_allocated *)ptr)[-1].u.chunk->heap;

    if (STBM__TYPE(prefix) == STBM__TYPE_aligned)
        return stbm_get_heap(((stbm__aligned_allocated *)ptr)[-1].underlying_allocation);

    if (STBM__TYPE(prefix) == STBM__TYPE_debug)
        return stbm_get_heap(((stbm__debug_allocated *)ptr)[-1].underlying_allocation.debug1);

    STBM_ASSERT(!STBM__PREFIX_VALID(prefix));
    STBM_FAIL("Corrupt malloc header in stbm_get_heap");
    return 0;
}

STBM__API size_t stbm_get_debug_data(void *ptr, void **data)
{
    if (ptr)
    {
        stbm__uintptr prefix = ((stbm__uintptr *)ptr)[-1];
        if (STBM__TYPE(prefix) == STBM__TYPE_debug)
        {
            if (STBM__DEBUG(prefix) == STBM__DEBUG_2)
            {
                int n;
                stbm__debug_allocated *da = ((stbm__debug_allocated *)ptr) - 1;
                stbm__debug_base2 *db2 = da->underlying_allocation.debug2;
                n = (int)db2->extra_debug_bytes;
                *data = n ? (db2 + 1) : 0;
                return n;
            }
        }
    }
    *data = 0;
    return 0;
}

STBM__API int stbm_get_fileline(void *ptr, char **file)
{
    if (ptr)
    {
        stbm__uintptr prefix = ((stbm__uintptr *)ptr)[-1];
        if (STBM__TYPE(prefix) == STBM__TYPE_debug)
        {
            stbm__debug_allocated *da = ((stbm__debug_allocated *)ptr) - 1;
            stbm__debug_base1 *db1 = da->underlying_allocation.debug1;
            *file = db1->file;
            return (int)db1->line;
        }
    }
    *file = 0;
    return 0;
}

STBM__API stbm_tc *stbm_tc_init(void *storage, size_t storage_bytes, stbm_heap *heap)
{
    int i;
    stbm_tc *tc;

    if (storage_bytes < sizeof(stbm_tc))
        return 0;

    tc = (stbm_tc *)storage;

    tc->bitmap = 0;
    tc->lru_counter = 0;
    tc->cached_bytes = 0;
    tc->cached_bytes_scavenge_threshhold = 1024 * 1024; // 1MB
    for (i = 0; i < STBM__NUM_TC_INDICES; ++i)
        tc->slot_for_sizeclass[i] = -1;
    tc->heap = heap;

    return tc;
}

STBM__API void stbm_heap_free(stbm_heap *heap)
{
    stbm__system_allocation *cur = heap->system_sentinel.next;
    while (cur != &heap->system_sentinel)
    {
        stbm__system_allocation *next = cur->next;
        STBM_ASSERT(cur->magic == STBM__SYSTEM_ALLOC_LARGE || cur->magic == STBM__SYSTEM_ALLOC_MEDIUM);
        heap->system_free(heap->user_context, cur, cur->size);
        cur = next;
    }
    if (heap->medium.cached_system_alloc)
        heap->system_free(heap->user_context, heap->medium.cached_system_alloc, heap->medium.cached_system_alloc->size);
}

static void stbm__heap_check_locked(stbm_heap *heap)
{
    stbm__medium_check_all(heap);
}

STBM__API void stbm_heap_check(stbm_heap *heap)
{
    STBM__ACQUIRE(heap->allocation_mutex);
    stbm__heap_check_locked(heap);
    STBM__RELEASE(heap->allocation_mutex);
    stbm_debug_iterate(heap, 0, 0);
}

STBM__API void *stbm_get_heap_user_context(stbm_heap *heap)
{
    return heap->user_context;
}

STBM__API stbm_heap *stbm_heap_init(void *storage, size_t storage_bytes, stbm_heap_config *hc)
{
    stbm_heap *heap = (stbm_heap *)storage;
    if (sizeof(*heap) > storage_bytes)
        return NULL;

#ifdef STBM__MUTEX
#ifdef STBM_ATOMIC_COMPARE_AND_SWAP32
    STBM_MEMSET(heap->crossthread_free_stack, 0, sizeof(heap->crossthread_free_stack));
    heap->crossthread_free_pointer_stack_pos = 0;
#endif

    heap->crossthread_mutex = hc->crossthread_free_mutex;
    heap->crossthread_free_list = NULL;
#endif

    heap->allocation_mutex = hc->allocation_mutex;

    heap->num_guard_bytes = 0;
    heap->guard_value = 0;
    heap->force_file_line = 0;
    heap->force_debug = 0;
    if (hc->minimum_alignment > STBM__SMALL_ALIGNMENT)
    {
        STBM_ASSERT(stbm__is_pow2(hc->minimum_alignment));
        heap->minimum_alignment = (stbm__uint32)hc->minimum_alignment;
    }
    else
    {
        heap->minimum_alignment = STBM__SMALL_ALIGNMENT;
    }
    heap->align_all_blocks_to_minimum = (stbm__uint8)hc->align_all_blocks_to_minimum;
    heap->full_stats = 0;
    heap->large_alloc_threshhold = 262144;
    heap->smallchunk_size = 4040;
    heap->cur_medium_chunk_size = 256 * (1 << 10);
    heap->max_medium_chunk_size = 2048 * (1 << 10);
    heap->cache_medium_chunks = STBM_MEDIUM_CACHE_none;

    heap->system_alloc = hc->system_alloc;
    heap->system_free = hc->system_free;
    heap->user_context = hc->user_context;

    stbm__medium_init(heap); // minimum_alignment must already be set when we call this
    stbm__small_init(heap);
    stbm__large_init(heap);
    stbm__system_init(heap);
    stbm__stats_init(heap);

    // @TODO create a medium block out of the left-over storage

    return heap;
}

STBM__API void stbm_heapconfig_set_trailing_guard_bytes(stbm_heap *heap, size_t num_bytes, unsigned char value)
{
    heap->num_guard_bytes = (stbm__uint32)num_bytes;
    heap->guard_value = value;
    heap->force_debug = 1;
}

STBM__API void stbm_heapconfig_capture_file_line(stbm_heap *heap, int enable_capture)
{
    heap->force_file_line = enable_capture ? 1 : 0;
}

STBM__API void stbm_heapconfig_gather_full_stats(stbm_heap *heap)
{
    heap->full_stats = 1;
}

STBM__API void stbm_heapconfig_set_largealloc_threshhold(stbm_heap *heap, size_t threshhold)
{
    heap->large_alloc_threshhold = (stbm__uint32)STBM__MIN(threshhold, STBM__SIZECLASS_MAX);
}

STBM__API void stbm_heapconfig_set_medium_chunk_size(stbm_heap *heap, size_t cur_size, size_t max_size)
{
    heap->cur_medium_chunk_size = cur_size;
    heap->max_medium_chunk_size = STBM__MIN(max_size, 2 << 20);
}

STBM__API void stbm_heapconfig_set_small_chunk_size(stbm_heap *heap, size_t size)
{
    heap->smallchunk_size = size;
}

STBM__API void stbm_heapconfig_cache_medium_chunks(stbm_heap *heap, int do_cache)
{
    heap->cache_medium_chunks = (stbm__uint8)do_cache;
}

// perform a callback on a pointer which may be a wrapped around an aligned/debug alloc
static void stbm__process_user_pointer(void *ptr, stbm_debug_iterate_func *callback, void *user_context, int maybe_wrapped)
{
    stbm__uintptr prefix = ((stbm__uintptr *)ptr)[-1];
    stbm_alloc_info info;

    if (maybe_wrapped && STBM__IS_WRAPPED(prefix))
    {
        stbm__aligned_base *ab = (stbm__aligned_base *)ptr;
        if (ab->wrapped_user_pointer == NULL)
            STBM_FAIL("Corrupt block in iterate");
        ptr = ab->wrapped_user_pointer;
    }

    info.ptr = ptr;

    info.size = stbm_get_allocation_size(ptr);
    info.userdata = stbm_get_userdata(ptr);
    info.is_aligned = (STBM__TYPE(((stbm__uintptr *)ptr)[-1]) == STBM__TYPE_aligned);
    info.line = stbm_get_fileline(ptr, &info.file);
    info.extra_data_bytes = (int)stbm_get_debug_data(ptr, &info.extra_data);

    if (callback)
        callback(user_context, &info);
}

STBM__API void stbm_debug_iterate(stbm_heap *heap, stbm_debug_iterate_func *callback, void *user_context)
{
    stbm__system_allocation *sa;
    STBM__ACQUIRE(heap->allocation_mutex);
    sa = heap->system_sentinel.next;
    while (sa != &heap->system_sentinel)
    {
        // every system allocation is either one large allocation, or one or more
        // user allocations
        if (sa->magic == STBM__SYSTEM_ALLOC_LARGE)
        {
            // process a potentially wrapped allocation
            stbm__process_user_pointer(sa->begin_pointer, callback, user_context, 1);
        }
        else if (sa->magic != STBM__SYSTEM_ALLOC_MEDIUM)
        {
            STBM_FAIL("Corrupt system allocation header");
        }
        else
        {
            // medium-block system allocation; walk through all the blocks
            void *user = sa->begin_pointer;
            while ((char *)user < (char *)sa->end_pointer)
            {
                stbm__uintptr prefix = ((stbm__uintptr *)user)[-1];
                if ((prefix & STBM__BLOCK_MASK) == STBM__BLOCK_free)
                {
                    // if the block is free, skip it
                    user = (char *)user + STBM__MED_FREE_BYTES(prefix);
                }
                else
                {
                    // if the block is allocated, compute where the next block is
                    void *next = (char *)user + stbm_get_allocation_size(user) + sizeof(stbm__medium_allocated);
                    // if it's wrapped, it might be a block used for small allocations,
                    // so check for the case where it's NOT
                    if (!STBM__IS_WRAPPED(prefix) || ((stbm__aligned_base *)user)->wrapped_user_pointer != NULL)
                        stbm__process_user_pointer(user, callback, user_context, 1);
                    else
                    {
                        // it's a chunk used for small allocations, so walk all the allocations
                        stbm__small_chunk *sc = (stbm__small_chunk *)user;
                        stbm__small_freeblock *pos = (stbm__small_freeblock *)sc->unused_storage + 1;
                        stbm__uint32 count = 0;
                        while (count < sc->num_alloc)
                        {
                            if ((char *)pos >= (char *)next)
                                STBM_FAIL("Small chunk invalid");
                            else if (pos->prefix == (stbm__uintptr)sc)
                            {
                                // the "wrapped" field of small blocks is invalid, so make sure we don't check it
                                stbm__process_user_pointer(pos->next_free, callback, user_context, 0);
                                ++count;
                            }
                            else if (pos->prefix != STBM__SMALL_FREE_BLOCK_MAGIC)
                            {
                                STBM_FAIL("Small block invalid");
                            }
                            pos++;
                        }
                    }
                    user = next;
                }
            }
            if (user != sa->end_pointer)
                STBM_FAIL("Heap walk lengths didn't validate");
        }
        sa = sa->next;
    }
    STBM__RELEASE(heap->allocation_mutex);
}

} // extern "C"

#ifdef STBM_UNITTEST
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <map>

static int stbm__num_sys_alloc, stbm__num_sys_free;
static int page_size;

typedef std::map<void *, size_t> alloc_size_hash_t;
static alloc_size_hash_t alloc_size_hash;

extern "C" void *vogl_realloc(const char *pFile_line, void *p, size_t new_size);

static void *stbm__mysystem_alloc(void *context, size_t size, size_t *outsize)
{
    if (!page_size)
        page_size = sysconf(_SC_PAGE_SIZE);

    size = (size + page_size - 1) & (~(page_size - 1));

    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);

    *outsize = 0;

    if (p)
    {
        *outsize = size;

        alloc_size_hash.insert(std::make_pair(p, size));
    }

    ++stbm__num_sys_alloc;

    return p;
}

static void stbm__mysystem_free(void *context, void *ptr, size_t size)
{
    if (ptr)
    {
        alloc_size_hash_t::iterator it(alloc_size_hash.find(ptr));
        if (it == alloc_size_hash.end())
        {
            STBM_FAIL("stbm__mysystem_free: ptr is invalid!\n");
        }
        if (it->second != size)
        {
            STBM_FAIL("stbm__mysystem_free: size is invalid!\n");
        }
        alloc_size_hash.erase(it);

        if (!size)
        {
            STBM_FAIL("stbm__mysystem_free: size is 0!\n");
        }

        int res = munmap(ptr, size);
        if (res != 0)
        {
            STBM_FAIL("munmap() failed!\n");
        }
    }

    ++stbm__num_sys_free;
}

static void stbm__test_heap(int alignment, int count)
{
    int i, j = 0, k;
    int num_outstanding;
    size_t size;
    stbm_heap_config hc; // = { 0 };
    stbm_heap *heap;
    static void *allocations[20000];
    static size_t sizes[20000];
    char storage[2100];

    STBM_MEMSET(&hc, 0, sizeof(hc));

    hc.system_alloc = stbm__mysystem_alloc;
    hc.system_free = stbm__mysystem_free;
    hc.user_context = NULL;
    hc.minimum_alignment = alignment;

    printf("Heap align=%d , count=%d\n", alignment, count);

    heap = stbm_heap_init(storage, sizeof(storage), &hc);

    // test medium (and small)
    size = 5;
    for (i = 0; i < 5000; ++i)
    {
        size_t r;
        allocations[i] = stbm_alloc(0, heap, size, 0);
        if (allocations[i] == 0)
            i = i;
        STBM_ASSERT(stbm_get_heap(allocations[i]) == heap);
        r = stbm_get_allocation_size(allocations[i]);
        STBM_ASSERT(r >= size);
        sizes[i] = size;
        size = size + 1 + (size >> 4);
        if (size > (96 << 10))
            size = 1;
        for (k = 0; k <= i; ++k)
            STBM_ASSERT(stbm_get_allocation_size(allocations[k]) >= sizes[k]);
    }
    stbm_debug_iterate(heap, 0, 0);
    for (i = 0; i < 5000; ++i)
        stbm_free(0, heap, allocations[i]);

    printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);

    // test small
    size = 1;
    for (i = 0; i < 5000; ++i)
    {
        size_t r;
        if (i == 501)
            i = i;
        allocations[i] = stbm_alloc(0, heap, size, 0);
        if (allocations[i] == 0)
            i = i;
        STBM_ASSERT(stbm_get_heap(allocations[i]) == heap);
        r = stbm_get_allocation_size(allocations[i]);
        STBM_ASSERT(r >= size);
        sizes[i] = size;
        size = (size + 1) % 5;
        if (size > (96 << 10))
            size = 1;
        for (k = 0; k <= i; ++k)
            STBM_ASSERT(stbm_get_allocation_size(allocations[k]) >= sizes[k]);
    }
    stbm_debug_iterate(heap, 0, 0);
    for (i = 0; i < 5000; ++i)
        stbm_free(0, heap, allocations[i]);

    printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);

    // test interleaving of small
    for (i = 0; i < 5000; ++i)
        allocations[i] = stbm_alloc(0, heap, 1, 0);
    stbm_debug_iterate(heap, 0, 0);
    for (i = 0; i < 5000; i += 2)
        stbm_free(0, heap, allocations[i]);
    for (i = 0; i < 5000; i += 2)
        stbm_free(0, heap, allocations[i + 1]);

    printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);

    num_outstanding = stbm__num_sys_alloc - stbm__num_sys_free;

    // test large blocks
    for (i = 0; i < 500; ++i)
        allocations[i] = stbm_alloc(0, heap, (i % 2 + 1) << 20, (unsigned short)i);

    printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);
    if (stbm__num_sys_alloc - stbm__num_sys_free != num_outstanding + 500)
        STBM_FAIL("Large allocations didn't trigger same number of system allocations");

    for (i = 0; i < 500; ++i)
        stbm_free(0, heap, allocations[i]);

    printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);

    // we allow for some fuzz in case we start caching system allocations to reduce system call overhead
    if (stbm__num_sys_alloc - stbm__num_sys_free > num_outstanding + 10)
        STBM_FAIL("Freeing large allocations didn't free (almost all) system allocations");

    // test that memory isn't trashed, and more complex
    // interleaving of free & alloc
    for (i = 0; i < 3000; ++i)
    {
        allocations[i] = stbm_alloc(0, heap, size, (unsigned short)i);
        sizes[i] = size;
        STBM_MEMSET(allocations[i], i, sizes[i]);
        size = size + 1 + (size >> 4);
        if (size > (96 << 10))
            size = 1;
    }
    stbm_debug_iterate(heap, 0, 0);
    size = 1;
    for (i = 0; i < count; ++i)
    {
        j = (unsigned)(j + i + (i >> 5) + (j >> 11)) % 3000;
        if (allocations[j])
        {
            for (k = 0; k < (int)sizes[j]; ++k)
                STBM_ASSERT(((char *)allocations[j])[k] == (char)j);
            STBM_ASSERT(stbm_get_userdata(allocations[j]) == (unsigned short)j);
            stbm_free(0, heap, allocations[j]);
        }
        sizes[j] = size;
        allocations[j] = stbm_alloc(0, heap, size, (unsigned short)j);
        STBM_ASSERT(stbm_get_userdata(allocations[j]) == (unsigned short)j);
        STBM_MEMSET(allocations[j], j, sizes[j]);
        size = (size * 367 + 1) % 30000;
        if (i % 3333 == 0)
            stbm_debug_iterate(heap, 0, 0);
    }

    printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);

    // interleave small again, should be able to be allocated from free blocks from above
    for (i = 0; i < 5000; ++i)
        allocations[i] = stbm_alloc(0, heap, 1, 0);
    stbm_debug_iterate(heap, 0, 0);
    for (i = 0; i < 5000; i += 2)
        stbm_free(0, heap, allocations[i]);
    for (i = 0; i < 5000; i += 2)
        stbm_free(0, heap, allocations[i + 1]);

    printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);

    if (stbm__num_sys_alloc == stbm__num_sys_free)
        STBM_FAIL("weird");
    stbm_heap_free(heap);
    if (stbm__num_sys_alloc != stbm__num_sys_free)
        STBM_FAIL("heap free failed to free everything");
}

static int stbm__ptr_compare(const void *p_, const void *q_)
{
    const void **p = (const void **)p_;
    const void **q = (const void **)q_;

    if ((char *)*p < (char *)*q)
        return -1;
    return (char *)*p > (char *)*q;
}

static void stbm__dump_heap_state(stbm_heap *heap)
{
    int i;
    printf("Free lists:\n");
    for (i = 0; i < STBM__NUM_INDICES * STBM__NUM_SLOTS; ++i)
    {
        stbm__medium_freeblock *b = heap->medium.lists[0][i];
        while (b)
        {
            printf("  Free: %8d %p\n", STBM__MED_FREE_BYTES(b->prefix), b);
            b = b->next_free;
        }
    }
}

static void stbm__test_heap_sorted(void)
{
    int i, j;
    size_t size;
    stbm_heap_config hc; // = { 0 };
    stbm_heap *heap;
    static void *allocations[20000];
    char storage[2100];

    STBM_MEMSET(&hc, 0, sizeof(hc));

    hc.system_alloc = stbm__mysystem_alloc;
    hc.system_free = stbm__mysystem_free;
    hc.user_context = NULL;

    heap = stbm_heap_init(storage, sizeof(storage), &hc);
    stbm_heapconfig_set_medium_chunk_size(heap, 16384, 65536);

    // first allocate and free a bunch to stabilize the system allocations

    size = 50;

    for (j = 0; j < 50; ++j)
    {
        for (i = 0; i < 3000; ++i)
            allocations[i] = stbm_alloc(0, heap, size + (i % 128), (unsigned short)i);

#if 0
		stbm__dump_heap_state(heap);
		for (i=0; i < 5; ++i)
			printf("Allocation: %d @ %p\n", stbm_get_allocation_size(allocations[i]), allocations[i]);
#endif

        for (i = 0; i < 3000; ++i)
        {
            stbm_free(0, heap, allocations[i]);
            //         stbm__dump_heap_state(heap);
        }

        printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);
    }

    for (j = 0; j < 10; ++j)
    {
        // reallocate, and try freeing every other one
        for (i = 0; i < 5000; ++i)
            allocations[i] = stbm_alloc(0, heap, size + (i % 128), (unsigned short)i);
        for (i = 0; i < 5000; i += 2)
            stbm_free(0, heap, allocations[i]);
        for (i = 1; i < 5000; i += 2)
            stbm_free(0, heap, allocations[i]);

        printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);
    }

    for (j = 0; j < 10; ++j)
    {
        // reallocate, and try freeing every other one in space, not time
        for (i = 0; i < 5000; ++i)
            allocations[i] = stbm_alloc(0, heap, size + (i % 128), (unsigned short)i);
        qsort(allocations, sizeof(allocations[0]), 5000, stbm__ptr_compare);
        for (i = 0; i < 5000; i += 2)
            stbm_free(0, heap, allocations[i]);
        for (i = 1; i < 5000; i += 2)
            stbm_free(0, heap, allocations[i]);
        printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);
    }

    // reallocate, and try freeing every third one in space, not time

    for (j = 0; j < 10; ++j)
    {
        for (i = 0; i < 5000; ++i)
            allocations[i] = stbm_alloc(0, heap, size + (i % 128), (unsigned short)i);
        qsort(allocations, sizeof(allocations[0]), 5000, stbm__ptr_compare);
        for (i = 0; i < 5000; i += 3)
            stbm_free(0, heap, allocations[i]);
        for (i = 1; i < 5000; i += 3)
            stbm_free(0, heap, allocations[i]);
        for (i = 2; i < 5000; i += 3)
            stbm_free(0, heap, allocations[i]);
        printf("Made %d system allocations total; %d outstanding\n", stbm__num_sys_alloc, stbm__num_sys_alloc - stbm__num_sys_free);
    }

    if (stbm__num_sys_alloc == stbm__num_sys_free)
        STBM_FAIL("weird");
    stbm_heap_free(heap);
    if (stbm__num_sys_alloc != stbm__num_sys_free)
        STBM_FAIL("heap free failed to free everything");
}

static void stbm__test(void)
{
    stbm__test_sizes();
    stbm__test_heap_sorted();
    stbm__test_heap(0, 50000);
    stbm__test_heap(16, 20000);
    stbm__test_heap(64, 20000);
    stbm__test_heap(256, 20000);
    stbm__test_heap(32, 20000);
    stbm__test_heap(0, 120000);
    stbm__test_heap(0, 500000);
    stbm__test_heap(0, 2500000);
}
#endif

#ifdef STBM_SELFTEST
int stb_malloc_selftest()
{
    stbm__test();
    return 0;
}
#endif
