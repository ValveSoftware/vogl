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

#ifndef INCLUDE_STB_MALLOC_H
#define INCLUDE_STB_MALLOC_H

// #define STBM_MUTEX_HANDLE to be the datatype of a pointer
// to a mutex or else the code will not be thread-safe
#ifndef STBM_MUTEX_HANDLE
// create a dummy pointer as the handle type
#define STBM_MUTEX_HANDLE void *
#define STBM__NO_MUTEX_HANDLE
#endif

#ifdef STBM_STATIC
#define STBM__API static
#else
#define STBM__API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h> // size_t

typedef struct stbm_heap stbm_heap;
typedef struct stbm_tc stbm_tc;

STBM__API void *stbm_alloc(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16);
// allocate a block of memory of 'size' bytes from the heap 'heap';
// userdata16 is an arbitrary 16-bit number stored in the allocation.
//
// For small blocks, the efficient storage format is only used
// if userdata16 == 0.
//
// if tc is not NULL, tc may not be accessed by any other thread at
// the same time, and the function may allocate from tc and cache
// additional blocks into tc
//
// If heap does not have an allocation mutex, then this function
// is allowed to manipulate it without taking any mutexes.

STBM__API void *stbm_alloc_align(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, size_t alignment, size_t align_offset);
// allocate a block of memory of 'size' bytes from the heap 'heap'
// which is aligned to a multiple of 'alignment' (which must be a
// power of two).
//
// userdata16 is an arbitrary 16-bit number stored in the allocation.
//
// if tc is not NULL, tc may not be accessed by any other thread at
// the same time, and the function may allocate from tc and cache
// additional blocks into tc
//
// If heap does not have an allocation mutex, then this function
// is allowed to manipulate it without taking any mutexes.

STBM__API void *stbm_alloc_debug(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, size_t alignment, size_t align_offset, char *file, int line, size_t extra_debug_size);
// allocate a block of memory of 'size' bytes from the heap 'heap';
// userdata16 is an arbitrary 16-bit number stored in the allocation.
// an additional 'extra_debug_size' bytes are allocated which are
// separate from the main block, accessible through stbm_get_debug_data.
//
// if tc is not NULL, tc may not be accessed by any other thread at
// the same time, and the function may allocate from tc and cache
// additional blocks into tc
//
// If heap does not have an allocation mutex, then this function
// is allowed to manipulate it without taking any mutexes.

STBM__API void *stbm_alloc_fileline(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, char *file, int line);
// allocate a block of memory of 'size' bytes from the heap 'heap';
// userdata16 is an arbitrary 16-bit number stored in the allocation.
//
// if stbm_heapconfig_capture_file_line() has been enabled for this
// heap, then extra data is allocated in the block and the file/line
// are recorded; otherwise file/line are ignored
//
// For small blocks, the efficient storage format is only used
// if userdata16 == 0 and stbm_heapconfig_capture_file_line is not
// enabled.
//
// if tc is not NULL, tc may not be accessed by any other thread at
// the same time, and the function may allocate from tc and cache
// additional blocks into tc
//
// If heap does not have an allocation mutex, then this function
// is allowed to manipulate it without taking any mutexes.

STBM__API void *stbm_alloc_align_fileline(stbm_tc *tc, stbm_heap *heap, size_t size, unsigned short userdata16, size_t alignment, size_t align_offset, char *file, int line);
// allocate a block of memory of 'size' bytes from the heap 'heap'
// which is aligned to a multiple of 'alignment' (which must be a
// power of two).
//
// if stbm_heapconfig_capture_file_line() has been enabled for this
// heap, then extra data is allocated in the block and the file/line
// are recorded; otherwise file/line are ignored
//
// userdata16 is an arbitrary 16-bit number stored in the allocation.
//
// if tc is not NULL, tc may not be accessed by any other thread at
// the same time, and the function may allocate from tc and cache
// additional blocks into tc
//
// If heap does not have an allocation mutex, then this function
// is allowed to manipulate it without taking any mutexes.

STBM__API void stbm_free(stbm_tc *tc, stbm_heap *preferred_heap, void *ptr);
// frees a previously-allocated block of memory addressed by 'ptr' from
// whichever heap it was allocated from.
//
// if tc is non-NULL and associated with the heap the ptr was
// allocated from, the block may be cached in tc.
//
// If the allocation came from preferred_heap, it will be properly freed
// into the general allocation pool for preferred_heap. Otherwise, it will
// be added to the cross-heap free list for the source heap, where it will
// only be fully freed the next time an allocation is made from that heap.
//
// If preferred_heap does not have an allocation mutex, then this function
// is allowed to manipulate preferred_heap without taking any mutexes.

STBM__API void stbm_tc_flush(stbm_tc *tc, stbm_heap *preferred_heap, void *ptr);
// any pointers cached in tc are flushed back to the relevant heap, using
// the same locking logic described for stbm_free.

STBM__API void *stbm_realloc(stbm_tc *tc, stbm_heap *heap, void *ptr, size_t newsize, unsigned short userdata16);
// "resizes" a block of memory, effectively equivalent to a call to stbm_alloc/stbm_alloc_tc
// and a call to stbm_free_tc/stbm_free, all using tc & heap.
//
// Note that this function will allocate from heap, so heap must be a "preferred"
// heap.

STBM__API size_t stbm_get_allocation_size(void *ptr);
// query the available memory pointed to by the pointer

STBM__API unsigned short stbm_get_userdata(void *ptr);
// query the arbitrary 16-bit identifier associated with the pointer

STBM__API stbm_heap *stbm_get_heap(void *ptr);
// determine which heap it was allocated from

STBM__API int stbm_get_fileline(void *ptr, char **file);
// get the stored file & line, or 0 if none stored

STBM__API size_t stbm_get_debug_data(void *ptr, void **data);
// get a pointer to the extra data specified by stbm_alloc_debug

#ifndef STBM_CALLBACK
#define STBM_CALLBACK
#endif

typedef void *STBM_CALLBACK stbm_system_alloc(void *user_context, size_t size_requested, size_t *size_provided);
typedef void STBM_CALLBACK stbm_system_free(void *user_context, void *ptr, size_t size);

#define STBM_TC_SIZEOF (sizeof(void *) * 2 * 32 + 64 + 64 + sizeof(void *) + 4 * 4 + 257 + sizeof(void *))

STBM__API stbm_tc *stbm_tc_init(void *storage, size_t storage_bytes, stbm_heap *heap);
// initialize a new thread-cache referring to 'heap';
// storage_size_in_bytes must be as large as the minimum
// thread cache size.

STBM__API void stbm_heap_free(stbm_heap *);
// Frees all system allocations made by this heap back to the
// system allocator. Any outstanding allocations from this heap
// become invalid. (For example, outstanding cached allocations
// in any corresponding stbm_tc's.) The heap itself becomes
// invalid (you must init it to use it again).

STBM__API void *stbm_get_heap_user_context(stbm_heap *);
// get back the user context set in the creation

typedef struct
{
    // You pass in allocators which are used to get additional storage.
    // If they are NULL, only the storage passed in above is used to satisfy
    // allocations, but also allocations larger than 1MB are NOT supported.
    stbm_system_alloc *system_alloc;
    stbm_system_free *system_free;
    void *user_context;

    // All allocations have a default alignment of 8 (32-bit) or 16 (64-bit).
    // You can choose a higher power-of-two default alignment here.
    size_t minimum_alignment;

    // If you do not set the following flag, then blocks which are smaller
    // than the requested minimum alignment may get a smaller alignment
    // (namely, the original default 8/16 alignment). If you set this flag,
    // then EVERY allocation, not matter how small, will get the alignment.
    // Currently this disables the smallest-block optimizations and will waste
    // lots of memory for those, but has no effect on intermediate sizes.
    int align_all_blocks_to_minimum;

    // This mutex is used to prevent multiple threads from manipulating
    // the heap data structure atstbm_heap_free the same time. It should be non-NULL if
    // you will have multiple thread allocating from this heap. If only
    // one thread will be allocating from the heap it can be null.
    STBM_MUTEX_HANDLE allocation_mutex;

    // If this mutex is provided, then frees which do not have the correct
    // heap passed to the free call will be put on a special 'crossthread'
    // free list. This prevents them from contending with threads that are
    // allocating using the allocation mutex, and also allows cross-thread
    // freeing even if there is no allocation mutex.
    STBM_MUTEX_HANDLE crossthread_free_mutex;
} stbm_heap_config;

STBM__API stbm_heap *stbm_heap_init(void *storage, size_t storage_bytes, stbm_heap_config *hc);
// initialize a new heap from memory you provide which must be
// at least STBM_HEAP_SIZEOF; the data in the stbm_heap_config
// is copied, so you don't need to preserve it after this returns.

#define STBM_HEAP_SIZEOF \
(\
/* alignment     */ 60 \
/* ct_free_stack */ + sizeof(void *) * 16 + 64 \
/* ct_free_list  */ + 64 \
/* alloc_lock    */ + sizeof(STBM_MUTEX_HANDLE) \
/* configuration */ + 3 * sizeof(size_t) + 6 * 4 \
/* system_alloc  */ + 5 * sizeof(void *) + 3 * sizeof(void *) \
/* stats         */ + 2 * 4 + 4 * sizeof(void *) \
/* small         */ + 2 * (6 * sizeof(void *) + 2 * 4) + sizeof(void *) \
/* medium        */ + 3 * 4 + 13 * 4 + 13 * 32 * sizeof(void *) + 4 * sizeof(void *)\
)

STBM__API void stbm_heapconfig_set_trailing_guard_bytes(stbm_heap *heap, size_t num_bytes, unsigned char value);
STBM__API void stbm_heapconfig_capture_file_line(stbm_heap *heap, int enable_capture);
STBM__API void stbm_heapconfig_gather_full_stats(stbm_heap *heap);
STBM__API void stbm_heapconfig_set_largealloc_threshhold(stbm_heap *heap, size_t threshhold);
STBM__API void stbm_heapconfig_set_medium_chunk_size(stbm_heap *heap, size_t cur_size, size_t max_size);
STBM__API void stbm_heapconfig_set_small_chunk_size(stbm_heap *heap, size_t size);
STBM__API void stbm_heap_check(stbm_heap *heap);

#define STBM_MEDIUM_CACHE_none 0
#define STBM_MEDIUM_CACHE_one 1
#define STBM_MEDIUM_CACHE_all 2

STBM__API void stbm_heapconfig_cache_medium_chunks(stbm_heap *heap, int cache_mode);

typedef struct
{
    void *ptr;
    unsigned short userdata;
    size_t size;
    int is_aligned;
    char *file;
    int line;
    void *extra_data;
    int extra_data_bytes;
} stbm_alloc_info;

typedef void STBM_CALLBACK stbm_debug_iterate_func(void *usercontext, stbm_alloc_info *data);

STBM__API void stbm_debug_iterate(stbm_heap *heap, stbm_debug_iterate_func *callback, void *user_context);

#ifdef __cplusplus
}
#endif

#endif //INCLUDE_STB_MALLOC_H
