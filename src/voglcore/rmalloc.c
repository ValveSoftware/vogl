/* =====================================================================
   File:	rmalloc.c
   Author:	Rammi
   Date:	11/16/1995 (started)

   License: (This is the open source ISC license, see
             http://en.wikipedia.org/wiki/ISC_license
             for more info)

    Copyright © 2010	Andreas M. Rammelt <rammi@hexco.de>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   Content: Debug wrapper functions for the malloc library.
      For more information see rmalloc.h

   Last Change: $Date: 2010/10/08 09:38:23 $
   History: 	$Log: rmalloc.c,v $
   History: 	Revision 1.16  2010/10/08 09:38:23	rammi
   History: 	Clarified license to ISC license
   History:
   History: 	Revision 1.15  2009/07/06 12:05:48	rammi
   History: 	Incremented version.
   History:
   History: 	Revision 1.14  2009/07/06 12:04:36	rammi
   History: 	Avoided abort when calling RM_STAT before something was allocated.
   History: 	Thanks to Matthias Bilger for pointing out the error.
   History: 	Some other messages when calling functions without proper initialization are added, too.
   History:
   History: 	Revision 1.13  2008/12/28 13:00:26	rammi
   History: 	Fixed Typo in rmalloc.h: unded --> undef
   History:
   History: 	Revision 1.12  2008/12/27 15:57:40	rammi
   History: 	Updated version
   History:
   History: 	Revision 1.11  2008/12/27 15:56:44	rammi
   History: 	Embedded patch from F. Culot to avoid overflow in Rcalloc()
   History:
   History: 	Revision 1.10  2008/09/30 12:37:23	rammi
   History: 	Added Peter Wehrfritz' changes: new SILENT mode and patch against compiler warning if TEST_DEPTH==0
   History:
   History: 	Revision 1.9  2006/01/29 18:56:12  rammi
   History: 	Streamlined various things due to a proposal by Brant L Gurganus. Thanks Brant!
   History:
   History: 	Revision 1.8  2003/01/31 15:47:52  rammi
   History: 	Changed signature of Rmalloc_set_flags() and Rmalloc_retag to return pointer.
   History:
   History: 	Revision 1.7  2003/01/31 15:04:20  rammi
   History: 	Fixed unclosed comment.
   History:
   History: 	Revision 1.6  2003/01/31 14:51:48  rammi
   History: 	Updated version to 1.16
   History:
   History: 	Revision 1.5  2003/01/31 14:49:00  rammi
   History: 	Unset RM_STATIC flag in realloc to avoid warning on free
   History:
   History: 	Revision 1.4  2002/04/22 17:39:34  rammi
   History: 	Added output of BREAK_GENERATION environment variable when used
   History:
   History: 	Revision 1.3  2002/04/22 16:28:06  rammi
   History: 	Finetuning of generations feature
   History:
   History: 	Revision 1.2  2002/04/22 15:26:16  rammi
   History: 	Added Karl Brace's generations feature.
   History:

   Pre-CVS history:
      04/11/1996 (Rammi)
      Changed to hashed table for faster access
      04/15/1996 (Rammi)
      Included statistics
      08/16/1997 (Rammi)
      Automatic output of used memory on exit
      08/25/1997 (Rammi)
      Catching signals in critical situations (partly)
      02/18/1998 (Rammi)
      Testing memory before outputting statistic
      Overworked signal handling in IsPossibleFilePos()
      Made it unnecessary of changing all mallocs etc
      to upper case
      02/19/1998 (Rammi)
      Little changes to compile on Alpha (64bit) without
      warnings
      03/10/1998 (Rammi)
      Added comments.
      03/24/1998 (Rammi)
      Allowed compilation without WITH_FLAGS
      04/07/1998 (Rammi)
      All output goes to stderr.
.		1.11beta is released as 1.11!
      05/28/1998 (Rammi)
      Changed comments to english for public release
      Added some more signal handling.
      06/01/1998 (Rammi)
      Added output of (flagged) strings in ELOQUENT mode.
      Changed all names from my_... to R...
      This is version 1.12!
      11/13/1998 (Rammi)
      Multiple defined label when using ELOQUENT mode now fixed.
      Added getcwd wrapper as a prototype how to handle library
      functions returning heap memory.
      This is version 1.13!
         06/10/99 (Rammi)
      The getcwd wrapper had a bug as Greg Silverman pointed
      out (I should better have tested it!). Also fixed a
      missing prototype and improved the signal handling in
      rtest to allow receiving more signals while handling
      one.
   ===================================================================== */

/* =========
   INCLUDEs:
   ========= */

#include <stdio.h>
#if defined(PLATFORM_POSIX)
    #include <unistd.h>
    #include <strings.h>
#endif
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>

#if defined(COMPILER_MSVC)
    #include <direct.h>
#endif

#if defined(COMPILER_MSVC)
    #define STATIC_INLINE static __inline
#else
    #define STATIC_INLINE static inline
#endif

#if defined(COMPILER_MSVC)
	#pragma warning(disable : 4267)
	#pragma warning(disable : 4244)
	#pragma warning(disable : 4090)
#endif



#undef MALLOC_DEBUG        /* here we need the correct malloc functions */
#define RM_NEED_PROTOTYPES /* but we want to compare prototypes */
#include "rmalloc.h"

#ifdef _MSC_VER
#define RMALLOC_NORETURN __declspec(noreturn)
#elif defined(__GNUC__)
#define RMALLOC_NORETURN __attribute__((noreturn))
#else
#define RMALLOC_NORETURN
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========
	   DEFINEs:
	   ======== */

/* Actual version */
#define VERSION "1.21"

/* ================================================================== */
/* ============ Switch settings for different behaviours ============ */
/* ============ 	   Please set as needed 			 ============ */
/* ================================================================== */

/* This switch sets, how and when the allocated blocks are tested
	 * on correctness. Each block is tested at least when
	 * reallocating/freeing it.
	 * Possible values:
	 *	0:		Minimum testing. Uses less memory, but
	 *					does not allow statistics.
	 *	1:		Extra testing possible by using RM_TEST
	 *					macro. Statistics possible.
	 *	2:		Testing ALL blocks on every malloc/free.
	 *					Statistics possible.
	 */
#ifndef RM_TEST_DEPTH
#define RM_TEST_DEPTH 1
#endif

#ifndef RM_TEST_ALL_FREQUENCY
// Controls the frequency of heap testing when RM_TEST_DEPTH==2, where 1 is on every operation
#define RM_TEST_ALL_FREQUENCY 10000
#endif

/* This switch sets whether Karl's generations feature should be used
	 * See the HTML doc for a indepth explanation of generations.
	 * If set generations are used, otherwise no generations are included.
	 */
#define GENERATIONS

#ifdef GENERATIONS
/* BREAK_GENERATION_COND is the condition to find the generation  you are
	 * interested in.
	 * Set your debugger to the function rmalloc_generation() to find out which
	 * function stack creates that generation.
	 * You can either set it as a comparision directly to the number of a
	 * generation (known from a previous run), a comparision to the function
	 * GetBreakGenerationEnv() which reads the break generation from the environment
	 * variable BREAK_GENERATION or don't set it so the break feature is not used.
	 * The macro form allows for more complicated conditions, see example below.
	 */
#define BREAK_GENERATION_COND(nr) ((nr) == GetBreakGenerationEnv())
/* #define BREAK_GENERATION_COND(nr) ((nr) == 125) */
/* #define BREAK_GENERATION_COND(nr) ((nr) == 42  ||  (nr) == 4711) */

/* The maximum number of generations you'd like to see in the statistics */
#define MAX_STAT_GENERATIONS 3

#endif /* GENERATIONS */

/* Switch on EXTENTED alloc information. (Makes sense if an actual
	 * error is observed such as a multiple free)
	 */
/*	#define ELOQUENT */

/* Only show errors. This doesn't output the initializing output and the
	 * end statistic if there was a leak encountered.
	 */
/*	#define SILENT */

/* Allows setting of special flags with the RM_SET_FLAGS macro.
	 * Needs more memory.
	 */
#define WITH_FLAGS

/* Allows realloc(NULL, ...)
	 * Posix allows this but there are some old malloc libraries
	 * which crash on this. Switch on if you want to be compatible.
	 */
#define ALLOW_REALLOC_NULL

/* Allows free(NULL)
	 * I still consider this an error in my progs because I use
	 * NULL always as a very special value.
	 */
#define ALLOW_FREE_NULL

/* ================================================================== */
/* ========================  Other Defines	========================= */
/* ================================================================== */

/* Alignment */
#define ALIGNMENT sizeof(void *) * 2

/* Output message header: */
#define HEAD "<MALLOC_DEBUG>\t"

/* Alignment padding: */
//#define ALIGN(s)	(((s+ALIGNMENT-1)/ALIGNMENT)*ALIGNMENT)
#define ALIGN(s) ((s + ALIGNMENT - 1) & (~(ALIGNMENT - 1)))

/* Magic marker for block start: */
#define PREV_STOP 0x55555555

/* Additional space for block begin to keep alignment: */
#define START_SPACE ALIGN(sizeof(begin))

/* Additional space at end of block */
#define END_SPACE (sizeof(End))

/* Overall additional space per block: */
#define EXTRA_SPACE (START_SPACE + END_SPACE)

/* Hashtable size: */
//#define HASHSIZE	257
//#define HASHSIZE	65537
#define HASHSIZE 98947

/* Hash function: */
//#define HASH(p)		((((unsigned long)(p))/ALIGNMENT)%HASHSIZE)
STATIC_INLINE unsigned int bitmix32(unsigned int a)
{
    a -= (a << 6);
    a ^= (a >> 17);
    a -= (a << 9);
    a ^= (a << 4);
    a -= (a << 3);
    a ^= (a << 10);
    a ^= (a >> 15);
    return a;
}

#define HASH(p) (bitmix32((((size_t)(p)) / ALIGNMENT)) % HASHSIZE)

/* ==========================
	   STRUCTs, TYPEDEFs & ENUMs:
	   ========================== */

/* This Information is added to the beginning of every
	 * allocated block.
	 */
typedef struct _begin
{
    unsigned StpA; /* Magic bytes */
#if RM_TEST_DEPTH > 0
    struct _begin *Next, /* for linking in forward */
        *Prev;           /* and backward direction */
#endif
    const char *File; /* Filepos of allocation command */
    size_t Size;      /* Size demanded */
#ifdef GENERATIONS
    unsigned Generation; /* All mallocs are numbered */
#endif
#ifdef WITH_FLAGS
    unsigned Flags; /* Special flags */
#endif
    unsigned StpB; /* Magic bytes */
} begin;

/*
	 * Global data.
	 */
typedef struct _global
{
    unsigned isInitialized; /* Flag: already initialized? */
    unsigned BlockCount;    /* Number of allocated blocks */
} global;

/* =======
	   CONSTs:
	   ======= */

/* Magic block end: */
static unsigned char End[] =
    {
        0xA5, 0xA5, 0xA5, 0xA5, /* odd */
        0x5B, 0x5B, 0x5B, 0x5B, /* odd */
        0xAB, 0xAB, 0xAB, 0xAB, /* odd */
        0xAA, 0x55, 0xAA, 0x55  /* odd */
    };

/* ========
	   GLOBALs:
	   ======== */

#ifdef GENERATIONS
/* The current generation. This is simply incremented which each call. */
static unsigned cur_generation = 0;
#endif

/* =======
	   LOCALs:
	   ======= */

#if RM_TEST_DEPTH > 0
/* Stop marker for linked list of allocated blocks: */
static begin ChainTempl =
    {
        PREV_STOP,
        &ChainTempl,
        &ChainTempl,
        "<Special>",
        0,
#ifdef GENERATIONS
        0,
#endif
#ifdef WITH_FLAGS
        0,
#endif
        PREV_STOP
    };

/* Hashed linked lists: */
static begin Chain[HASHSIZE];

/* Global data used: */
static global Global =
    {
        0, /* is initialized?	*/
        0  /* number of blocks */
    };
#endif /* RM_TEST_DEPTH */

/* Internally used longjmp target used if errors occure. */
static jmp_buf errorbuf;

/* ========
	   FORWARD:
	   ======== */

static int FindBlk(const unsigned char *P, const char *file);

/* ===============================================================
	         IMPLEMENTATION
	   =============================================================== */

static void *DefMalloc(size_t size, void *pUser)
{
    (void)pUser;
    return malloc(size);
}

static void DefFree(void *p, void *pUser)
{
    (void)pUser;
    free(p);
}

static void *DefRealloc(void *ptr, size_t size, void *pUser)
{
    (void)pUser;
    return realloc(ptr, size);
}

static rmalloc_malloc_func_t g_malloc_func = DefMalloc;
static rmalloc_free_func_t g_free_func = DefFree;
static rmalloc_realloc_func_t g_realloc_func = DefRealloc;
static void *g_pMalloc_user_ptr;

void Rmalloc_set_callbacks(rmalloc_malloc_func_t pMalloc, rmalloc_free_func_t pFree, rmalloc_realloc_func_t pRealloc, void *pUser)
{
    g_malloc_func = pMalloc;
    g_free_func = pFree;
    g_realloc_func = pRealloc;
    g_pMalloc_user_ptr = pUser;
}

#define CALL_MALLOC(s) (*g_malloc_func)(s, g_pMalloc_user_ptr)
#define CALL_FREE(p) (*g_free_func)(p, g_pMalloc_user_ptr)
#define CALL_REALLOC(p, s) (*g_realloc_func)(p, s, g_pMalloc_user_ptr)

/* =============================================================================
	   Function:		FatalSignal // local //
	   Author:		Rammi
	   Date:		08/25/1997

	   Return:		---

	   Parameter:		signum	  signal number

	   Purpose: 	Signal handler für fatal signals (SIGBUS, SIGSEGV)
	   ============================================================================= */
RMALLOC_NORETURN static void FatalSignal(int signum)
{
    /* --- jump to a save place --- */
    longjmp(errorbuf, signum);
}

/* =============================================================================
	   Function:		IsPossibleFilePos	// local //
	   Author:		Rammi
	   Date:		11/30/1996

	   Return:		!= 0	possibly ok
	                  0		seems not so

	   Parameter:		file	possible filename
	         size	possible size

	   Purpose: 	Decide whether file could be a filename and
	         size a block size.
	   ============================================================================= */
static int IsPossibleFilePos(const char *file, int size)
{
    void (*old_sigsegv_handler)(int) = SIG_DFL;
    void (*old_sigbus_handler)(int) = SIG_DFL;
    char *dp;
    int ret;

    if (setjmp(errorbuf))
    {
        /* uh oh, we got a kick in the ass */
        signal(SIGSEGV, old_sigsegv_handler);
        #if defined(SIGBUS)
            signal(SIGBUS, old_sigbus_handler);
        #endif
        return 0;
    }

    /* --- the following is dangerous! So catch signals --- */
    old_sigsegv_handler = signal(SIGSEGV, FatalSignal);
    #if defined(SIGBUS)
        old_sigbus_handler = signal(SIGBUS, FatalSignal);
    #endif

    if (!file)
    {
        ret = 0;
    }
    else
    {
        dp = strchr(file, ':'); /* file pos needs : */

        ret = (dp && dp - file > 3 &&
               (!strncmp(dp - 2, ".c", 2) || !strncmp(dp - 2, ".h", 2) || !strncmp(dp - 4, ".inc", 4) || !strncmp(dp - 4, ".inl", 4) || !strncmp(dp - 4, ".cpp", 4) || !strncmp(dp - 4, ".cxx", 4) || !strncmp(dp - 4, ".hpp", 4)) && atoi(dp + 1) > 0 && size >= 0);
    }

    /* --- danger is over! --- */
    signal(SIGSEGV, old_sigsegv_handler);
    #if defined(SIGBUS)
        signal(SIGBUS, old_sigbus_handler);
    #endif

    return ret;
}

#ifdef GENERATIONS
/* =============================================================================
	   Function:		GetBreakGenerationEnv	// lokal //
	   Author:		Rammi
	   Date:		04/22/2002

	   Return:			the content of the BREAK_GENERATION environment variable
	                  or 0

	   Parameter:		---

	   Purpose: 	Return the content of the environment variable
	                  BREAK_GENERATION (a number indicating which is the
	                  generation you want to break with the debugger).

	   ============================================================================= */
static unsigned GetBreakGenerationEnv(void)
{
    /* The environment variable is buffered here for faster access. */
    static char *breakGenerationEnv = (char *)-1;
    /** The result of the conversion to unsigned is buffered here. */
    static unsigned result = 0;

    if (breakGenerationEnv == (char *)-1)
    {
        /* 1st call: get the environment variable */
        breakGenerationEnv = getenv("BREAK_GENERATION");
        if (breakGenerationEnv != NULL)
        {
            /* try conversion */
            result = atoi(breakGenerationEnv);
            fprintf(stderr,
                    HEAD "Using environment variable BREAK_GENERATION=%d\n",
                    result);
        }
    }

    return result;
}
#endif /* GENERATIONS */

/* =============================================================================
	   Function:		ControlBlock	// lokal //
	   Author:		Rammi
	   Date:		11/16/1995

	   Return:		---
	   Parameter:		Bkl Pos of allocated block (original)
	         file	file pos from where initial lib function
	            was called

	   Purpose: 	Control integrity of block

	   ============================================================================= */
static void ControlBlock(begin *B, const char *file)
{
    unsigned char *p = (((unsigned char *)B) + START_SPACE);
#if RM_TEST_DEPTH > 0
    int DoAbort = 0;
#endif
    /* === the very beginning === */
    if (B->StpA != PREV_STOP)
    {
#if RM_TEST_DEPTH > 0
        DoAbort = 1;
#endif
        fprintf(stderr, HEAD
                "Corrupted block begin (overwritten from elsewhere)\n"
                "\tshould be: %08x\n"
                "\tis:		  %08x\n"
#ifdef GENERATIONS
                "\tblock was allocated in %s [%u Bytes, generation %u]\n"
#else
                "\tblock was allocated in %s [%u Bytes]\n"
#endif
                "\terror was detected in  %s\n",
                PREV_STOP,
                B->StpA,
                B->File,
                (unsigned)B->Size,
#ifdef GENERATIONS
                B->Generation,
#endif
                file);
    }

    /* === begin of user data === */
    if (B->StpB != PREV_STOP)
    {
#if RM_TEST_DEPTH > 0
        DoAbort = 1;
#endif
        fprintf(stderr, HEAD
                "Corrupted block begin (possibly written back)\n"
                "\tshould be: %08x\n"
                "\tis:		  %08x\n"
#ifdef GENERATIONS
                "\tblock was allocated in %s [%u Bytes, generation %u]\n"
#else
                "\tblock was allocated in %s [%u Bytes]\n"
#endif
                "\terror was detected in  %s\n",
                PREV_STOP,
                B->StpB,
                B->File,
                (unsigned)B->Size,
#ifdef GENERATIONS
                B->Generation,
#endif
                file);
    }

    /* === end of user data === */
    if (memcmp(p + B->Size, &End, END_SPACE) != 0)
    {
        unsigned char *E = (unsigned char *)(p + B->Size);
        unsigned i;
        int found = 0;
#if RM_TEST_DEPTH > 0
        DoAbort = 1;
#endif
        fprintf(stderr, HEAD
                "Corrupted block end (possibly written past the end)\n"
                "\tshould be:");
        for (i = 0; i < END_SPACE; i++)
        {
            fprintf(stderr, i % 4 ? "%02x" : " %02x", End[i]);
        }

        fprintf(stderr,
                "\n\tis:	   ");
        for (i = 0; i < END_SPACE; i++)
        {
            fprintf(stderr, i % sizeof(int) ? "%02x" : " %02x", E[i]);
        }
        fprintf(stderr, "\n"
#ifdef GENERATIONS
                        "\tblock was allocated in %s [%u Bytes, generation %u]\n"
#else
                        "\tblock was allocated in %s [%u Bytes]\n"
#endif
                        "\terror was detected in %s\n",
                B->File,
                (unsigned)B->Size,
#ifdef GENERATIONS
                B->Generation,
#endif
                file);

#if RM_TEST_DEPTH > 0
        if (!((unsigned long)E % sizeof(void *)) &&
            !(*(unsigned long *)E % sizeof(void *))) /* because of alignment */
        {
            /* Special service: look if memory was overwritten with pointer */
            if (FindBlk(*(unsigned char **)E, file))
            {
                begin *b = (begin *)((*(unsigned char **)E) - START_SPACE);
                if (IsPossibleFilePos(b->File, b->Size))
                {
                    fprintf(stderr,
                            "\tFirst %d bytes of overwritten memory can be interpreted\n"
                            "\t\tas a pointer to a block "
                            " allocated in:\n"
#ifdef GENERATIONS
                            "\t\t%s [%u Bytes, generation %u]\n",
#else
                            "\t\t%s [%u Bytes]\n",
#endif
                            (unsigned int)sizeof(void *),
                            b->File,
                            (unsigned)b->Size
#ifdef GENERATIONS
                            ,
                            b->Generation
#endif
                            );
                    found = 1;
                }
            }
        }
        if (!found)
#endif
        {
            /* Look, what we can find... */
            int j;

            for (j = END_SPACE - 1; j >= 0; j--)
            {
                if (E[j] != End[j])
                {
                    break;
                }
            }
            if (j >= 0 && !E[j])
            {
                /* Ends with '\0', so it's possibly a string */
                if (j > 0)
                {
                    while (--j >= 0)
                    {
                        if (!E[j])
                        {
                            break;
                        }
                    }
                    if (j < 0)
                    {
                        fprintf(stderr,
                                "\tLooks somewhat like a too long string,\n"
                                "\t\tending with \"%s\"\n",
                                E);
                    }
                }
                else
                {
                    /* Off by one? */
                    fprintf(stderr,
                            "\tLooks like string allocated one byte too short\n"
                            "\t\t(forgetting the nul byte)\n");
                }
            }
        }
    }

#if RM_TEST_DEPTH > 0
    /* Die LOUD */
    if (DoAbort)
    {
        abort();
    }
#endif
}

#if RM_TEST_DEPTH > 0
void Rmalloc_stat(const char *file);

/* =============================================================================
	   Function:		Exit // local //
	   Author:		Rammi
	   Date:		08/19/1997

	   Return:		---
	   Parameter:		---

	   Purpose: 	Function called on exit
	   ============================================================================= */
static void Exit(void)
{
#ifdef SILENT
    if (!Global.BlockCount)
    {
        return;
    }
#endif

    Rmalloc_stat("[atexit]"); /* show statistics */
}

/* =============================================================================
	   Function:		Initialize	// local //
	   Author:		Rammi
	   Date:		11.04.1996

	   Return:		---
	   Parameter:		---

	   Purpose: 	Necessary initializations

	   ============================================================================= */
static void Initialize(void)
{
    int i;

#ifndef SILENT
    fprintf(stderr,
            HEAD "rmalloc -- malloc wrapper V " VERSION "\n"
                 "\tby Rammi <mailto:rammi@hexco.de>\n"
                 "\tCompiled with following options:\n"
#if RM_TEST_DEPTH == 1
                 "\t\ttesting:\tonly actual block\n"
#elif RM_TEST_DEPTH == 2
                 "\t\ttesting:\tall allocated blocks\n"
#else
                 "\t\ttesting:\tcomment missing in " __FILE__ ":" INT2STRING(__LINE__) "\n"
#endif
#ifdef GENERATIONS
                 "\t\tgenerations:\tON\n"
#else
                 "\t\tgenerations:\tOFF\n"
#endif
#ifdef ELOQUENT
                 "\t\teloquence:\tON\n"
#else
                 "\t\teloquence:\tOFF\n"
#endif
#ifdef ALLOW_REALLOC_NULL
                 "\t\trealloc(0):\tALLOWED\n"
#else
                 "\t\trealloc(0):\tNOT ALLOWED\n"
#endif
#ifdef ALLOW_FREE_NULL
                 "\t\tfree(0):\tALLOWED\n"
#else
                 "\t\tfree(0):\tNOT ALLOWED\n"
#endif
#ifdef WITH_FLAGS
                 "\t\tflags:  \tUSED\n"
#else
                 "\t\tflags:  \tUNUSED\n"
#endif
                 "\t\talignment:\t" INT2STRING(ALIGNMENT) "\n"
                                                          "\t\tpre space:\t%d\n"
                                                          "\t\tpost space:\t%d\n"
                                                          "\t\thash tab size:\t" INT2STRING(HASHSIZE) "\n\n",
            (int)START_SPACE, (int)END_SPACE);
#endif /* ndef SILENT */

    /* --- init list heads --- */
    for (i = 0; i < HASHSIZE; i++)
    {
        memcpy(Chain + i, &ChainTempl, sizeof(begin));
        Chain[i].Next = Chain[i].Prev = Chain + i;
    }

    /* --- show statistics at exit --- */
    (void)atexit(Exit);

    Global.isInitialized = 1;
}

/* =============================================================================
	   Function:		TestAll 	// local //
	   Author:		Rammi
	   Date:		16.11.1995

	   Return:		---
	   Parameter:		file		file pos where lib function was
	               called

	   Purpose: 		Test all allocated blocks for inconsistencies
	   ============================================================================= */
static void TestAll(const char *file)
{
    begin *B; /* Block iterator */
    int i;    /* Hash iterator */

    /* make sure everything is initialized */
    if (!Global.isInitialized)
    {
        Initialize();
    }

    for (i = 0; i < HASHSIZE; i++)
    {
        B = Chain[i].Next;

        /* === Once around the circle === */
        while (B != &Chain[i])
        {
            ControlBlock(B, file);
            B = B->Next;
        }
    }
}

static unsigned int gTestAllCounter;

static void TestAllConditionally(const char *file)
{
    if (!gTestAllCounter)
    {
        TestAll(file);
        gTestAllCounter = RM_TEST_ALL_FREQUENCY;
    }
    gTestAllCounter--;
}

/* =============================================================================
	   Function:		AddBlk		// local //
	   Author:		Rammi
	   Date:		16.11.1995

	   Return:		---
	   Parameter:		Blk 	New block (original pos.)
	         file		called from

	   Purpose: 	Add new block to the list
	   ============================================================================= */
static void AddBlk(begin *Blk, const char *file)
{
    int hash = HASH(Blk); /* hash val */
    (void)file;

    /* make sure everything is initialized */
    if (!Global.isInitialized)
    {
        Initialize();
    }

#if RM_TEST_DEPTH > 1
    TestAllConditionally(file);
#else
    /* prevent compiler warnings about unused variables */
    file = NULL;
#endif
    /* --- insert it --- */
    Blk->Next = Chain[hash].Next;
    Blk->Prev = &Chain[hash];
    Chain[hash].Next->Prev = Blk;
    Chain[hash].Next = Blk;

    Global.BlockCount++;
}

/* =============================================================================
	   Function:		DelBlk		// local //
	   Author:		Rammi
	   Date:		16.11.1995

	   Return:		---

	   Parameter:		Blk 	block to remove
	         file		called from

	   Purpose: 	Remove block from list.
	         React angry if block is unknown
	   ============================================================================= */
static void DelBlk(begin *Blk, const char *file)
{
    begin *B;             /* run var	*/
    int hash = HASH(Blk); /* hash val */

    if (!Global.isInitialized)
    {
        fprintf(stderr, HEAD
                "Calling free without having allocated block via rmalloc\n"
                "in call from %s",
                file);
        abort();
    }

    /* look if block is known */
    for (B = Chain[hash].Next; B != &Chain[hash]; B = B->Next)
    {
        if ((B->StpA != PREV_STOP) || (B->StpB != PREV_STOP))
        {
            ControlBlock(B, file);
        }

        if (B == Blk)
        {
            goto found_actual_block; /* friendly goto */
        }
    }

    /* not found */
    fprintf(stderr, HEAD
            "Double or false delete\n"
            "\tHeap adress of block: %p\n"
            "\tDetected in %s\n",
            ((char *)Blk) + START_SPACE, file);
    {
        void (*old_sigsegv_handler)(int) = SIG_DFL;
        void (*old_sigbus_handler)(int) = SIG_DFL;

        if (setjmp(errorbuf))
        {
            /* uh oh, we got a kick in the ass */
            signal(SIGSEGV, old_sigsegv_handler);
            #if defined(SIGBUS)
                signal(SIGBUS, old_sigbus_handler);
            #endif
        }
        else
        {
            /* --- the following is dangerous! So catch signals --- */
            old_sigsegv_handler = signal(SIGSEGV, FatalSignal);
            #if defined(SIGBUS)
                old_sigbus_handler = signal(SIGBUS, FatalSignal);
            #endif

            if (IsPossibleFilePos(Blk->File, Blk->Size))
            {
                fprintf(stderr,
                        "\tTrying identification (may be incorrect!):\n"
                        "\t\tAllocated in %s [%u Bytes]\n",
                        Blk->File, (unsigned)Blk->Size);
#ifdef GENERATIONS
                fprintf(stderr, "\t\tGeneration: %u\n", (unsigned)Blk->Generation);
#endif
            }
            signal(SIGSEGV, old_sigsegv_handler);
            #if defined(SIGBUS)
                signal(SIGBUS, old_sigbus_handler);
            #endif
        }
    }
    abort(); /* die loud */

found_actual_block:
#if RM_TEST_DEPTH > 1
    /* check everything */
    TestAllConditionally(file);
#else
    /* test integrity of actual block */
    ControlBlock(Blk, file);
#endif

    /* remove: */
    Blk->Next->Prev = Blk->Prev;
    Blk->Prev->Next = Blk->Next;

    Global.BlockCount--;

#ifdef ELOQUENT
    fprintf(stderr,
            HEAD "Delete: %d Bytes allocated in %s (from %s)\n",
            Blk->Size, Blk->File, file);
#ifdef WITH_FLAGS
    if (Blk->Flags & RM_STRING)
    {
        char *c;
        /* look for eos */
        for (c = (char *)Blk + START_SPACE;
             c - (char *)Blk + START_SPACE < Blk->Size;
             c++)
        {
            if (!*c)
            {
                fprintf(stderr,
                        HEAD "\tContains string: \"%s\"\n",
                        (char *)Blk + START_SPACE);
                goto found_old_block;
            }
        }
        /* not found */
        fprintf(stderr,
                HEAD "\tContains string without null byte\n");
    found_old_block:
        ;
    }
#endif /* WITH_FLAGS */
#endif /* ELOQUENT */

#ifdef WITH_FLAGS
    if (Blk->Flags & RM_STATIC)
    {
        fprintf(stderr,
                HEAD "WARNING: freeing block marked as STATIC (in %s)\n", file);
    }
#endif /* WITH_FLAGS */
}

/* =============================================================================
	   Function:		FindBlk 	// local //
	   Author:		Rammi
	   Date:		11/30/1996

	   Return:		0				not found
	                  1				found

	   Parameter:		P		block (user pos)

	   Purpose: 	look if block is known
	   ============================================================================= */
static int FindBlk(const unsigned char *P, const char *file)
{
    begin *B;
    const begin *Blk = (const begin *)(P - START_SPACE);
    int hash = HASH(Blk);

    /* look if block is known */
    for (B = Chain[hash].Next; B != &Chain[hash]; B = B->Next)
    {
        if ((B->StpA != PREV_STOP) || (B->StpB != PREV_STOP))
        {
            ControlBlock(B, file);
        }

        if (B == Blk)
        {
            return 1;
        }
    }
    return 0;
}
#endif /* RM_TEST_DEPTH > 0 */

#ifdef GENERATIONS
/* =============================================================================
	   Function:		rmalloc_generation		// local //
	   Author:		Karl Brace
	   Date:		04/22/2002

	   Return:		---

	   Parameter:		Blk 	pointer to block

	   Purpose: 	Breakpoint for debugger if using Karl's generations
	                  feature.
	   ============================================================================= */
void rmalloc_generation(void *Blk)
{
    fprintf(stderr, HEAD "Allocating Block with generation %u...\n",
            ((begin *)Blk)->Generation);
}

#endif /* GENERATIONS */

/* =============================================================================
	   Function:		SetBlk		// local //
	   Author:		Rammi
	   Date:		11/16/1995

	   Return:		pointer to block (user pos.)

	   Parameter:		Blk 	pointer to block (original pos.)
	         size		size (user)
	         file		called from
	         flags			flags (when compiled WITH_FLAGS)

	   Purpose: 	Set our internal information

	   ============================================================================= */
#ifdef WITH_FLAGS
static void *SetBlk(void *Blk, size_t size, const char *file, unsigned flags)
#else
static void *SetBlk(void *Blk, size_t size, const char *file)
#endif
{

    ((begin *)Blk)->StpA = PREV_STOP;
#ifdef GENERATIONS
    ((begin *)Blk)->Generation = ++cur_generation;
#endif
    ((begin *)Blk)->File = file;
    ((begin *)Blk)->Size = size;
#ifdef WITH_FLAGS
    ((begin *)Blk)->Flags = flags;
#endif
    ((begin *)Blk)->StpB = PREV_STOP;
    memcpy(((char *)Blk) + START_SPACE + size, End, END_SPACE);

#if RM_TEST_DEPTH > 0
    AddBlk((begin *)Blk, file);
#endif
#ifdef ELOQUENT
    fprintf(stderr,
            HEAD "Adding: %p, %d Bytes (from %s)\n",
            ((char *)Blk) + START_SPACE, size, file);
#endif /* ELOQUENT */
#ifdef GENERATIONS
    if (BREAK_GENERATION_COND(cur_generation))
    {
        rmalloc_generation(Blk);
    }
#endif
    return ((char *)Blk) + START_SPACE;
}

/* =============================================================================
	   Function:		Rmalloc // external //
	   Author:		Rammi
	   Date:		11/16/1995

	   Return:		New prepared memory block with size size (user)

	   Parameter:		size		demanded size
	         file		called from where?

	   Purpose: 	wrapper for malloc
	   ============================================================================= */
void *Rmalloc(size_t size, const char *file)
{
    void *ret; /* ret val */

    if (size == 0)
    {
        fprintf(stderr, HEAD "WARNING: malloc() demands 0 Bytes (in %s)\n", file);
    }

    ret = CALL_MALLOC(size + EXTRA_SPACE); /* get extended block  */

    if (ret)
    {
/* initialize */
#ifdef WITH_FLAGS
        return SetBlk(ret, size, file, 0);
#else
        return SetBlk(ret, size, file);
#endif
    }
    else
    {
        fprintf(stderr,
                HEAD "WARNING: Out of memory! Returning NULL (in %s)\n", file);
        return NULL;
    }
}

/* =============================================================================
	   Function:		Rcalloc 	// external //
	   Author:		Rammi
	   Date:		11/16/1995

	   Return:		New (cleared) memory block of size nelem*size

	   Parameter:		nelem		nr blocks (as stupid as calloc)
	         size		size of one block
	         file		called from

	   Purpose: 	Wrapper function for calloc
	   ============================================================================= */
void *Rcalloc(size_t nelem, size_t size, const char *file)
{
    void *ret;

    /* check for overflow here (patch from Frédéric Culot) */
    if (size && nelem > SIZE_MAX / size)
    {
        fprintf(stderr,
                HEAD "WARNING: calloc() overflow! Returning NULL (in %s)\n", file);
        return NULL;
    }

    /* calculate correct size now */
    size *= nelem;

    if (size == 0)
    {
        fprintf(stderr,
                HEAD "WARNING: calloc() demands 0 Bytes (in %s)\n", file);
    }

    /* Rmalloc makes nearly all the work */
    ret = Rmalloc(size, file);

    if (ret)
    {
        /* clear */
        memset(ret, 0, size);
        return ret;
    }
    else
    {
        fprintf(stderr,
                HEAD "WARNING: Out of memory! Returning NULL (in %s)\n", file);
        return NULL;
    }
}

/* =============================================================================
	   Function:		Rrealloc	// external //
	   Author:		Rammi
	   Date:		11/16/1995

	   Return:		New block of size size (user pos.)

	   Parameter:		p		previous pointer
	         size		new size
	         file		called from

	   Purpose: 	Wrapper function for realloc
	   ============================================================================= */
void *Rrealloc(void *p, size_t size, const char *file)
{
    void *ret;
#ifdef WITH_FLAGS
    unsigned flags = 0;
#endif

    if (p == NULL)
    {
#ifndef ALLOW_REALLOC_NULL
        fprintf(stderr, HEAD "Realloc of NULL pointer (in %s)\n", file);
        abort();
#else /* ALLOW_REALLOC_NULL */
#ifdef ELOQUENT
        fprintf(stderr,
                HEAD "WARNING: realloc of NULL pointer (in %s)\n", file);
#endif
        return Rmalloc(size, file);
#endif /* ALLOW_REALLOC_NULL */
    }
#ifdef WITH_FLAGS
    else
    {
        /* keep flags */
        flags = ((begin *)(((char *)p) - START_SPACE))->Flags;
        ((begin *)(((char *)p) - START_SPACE))->Flags &= ~RM_STATIC; /* unset static flag to avoid warning */
    }
#endif /* WITH_FLAGS */

    if (size == 0)
    {
        fprintf(stderr,
                HEAD "WARNING: realloc() demands 0 Bytes (in %s)\n", file);
    }

#if RM_TEST_DEPTH > 0
    /* remove old block from list */
    DelBlk((begin *)(((char *)p) - START_SPACE), file);
#endif
    /* get everything new */
    ret = CALL_REALLOC(((char *)p) - START_SPACE, size + EXTRA_SPACE);

    if (ret)
    {
/* Initialize new block */
#ifdef WITH_FLAGS
        return SetBlk(ret, size, file, flags);
#else
        return SetBlk(ret, size, file);
#endif
    }
    else
    {
        fprintf(stderr,
                HEAD "WARNING: Out of memory! Returning NULL (in %s)\n", file);
        return NULL;
    }
}

/* =============================================================================
	   Function:		Rfree		// external //
	   Author:		Rammi
	   Date:		11/16/1995

	   Return:		---

	   Parameter:		p		block to free (user pos.)
	         file		called from

	   Purpose: 	Wrapper function for free()

	   ============================================================================= */
void Rfree(void *p, const char *file)
{
#ifdef ELOQUENT
    fprintf(stderr, HEAD "Free: %p (called from: %s)\n", p, file);
#endif /* ELOQUENT */
    if (p == NULL)
    {
#ifdef ALLOW_FREE_NULL
#ifdef ELOQUENT
        fprintf(stderr, HEAD "WARNING: Freeing NULL pointer (in %s)\n", file);
#endif
        return;
#else  /* !ALLOW_FREE_NULL */
        fprintf(stderr, HEAD "Trying to free NULL pointer (in %s)\n", file);
        abort();
#endif /* !ALLOW_FREE_NULL */
    }
#if RM_TEST_DEPTH > 0
    /* Remove block from list */
    DelBlk((begin *)(((char *)p) - START_SPACE), file);
#endif
    /* free block */
    CALL_FREE(((char *)p) - START_SPACE);
}

size_t Rmalloc_usable_size(void *p, const char *file)
{
#ifdef ELOQUENT
    fprintf(stderr, HEAD "UsableSize: %p (called from: %s)\n", p, file);
#endif /* ELOQUENT */

    if (!p)
        return 0;

    struct _begin *info = (struct _begin *)(((char *)p) - START_SPACE);

    ControlBlock(info, file);

    return info->Size;
}

/* =============================================================================
	   Function:		Rstrdup 	// external //
	   Author:		Rammi
	   Date:		11/16/1995

	   Return:		New memory with copied string.

	   Parameter:		s		string to copy
	         file		called from

	   Purpose: 	Wrapper function for strdup()
	   ============================================================================= */
char *Rstrdup(const char *s, const char *file)
{
    size_t size; /* needed memory */
    char *ret;

    if (s == NULL)
    {
        fprintf(stderr, HEAD "Calling strdup(NULL) (in %s)\n", file);
        abort();
    }
    size = strlen(s) + 1;

    /* Rmalloc() does nearly all the work */
    ret = Rmalloc(size, file);

    if (ret)
    {
        /* copy string */
        strcpy(ret, s);
#ifdef WITH_FLAGS
        Rmalloc_set_flags(ret, RM_STRING, "<by strdup>");
#endif
        return ret;
    }
    else
    {
        fprintf(stderr,
                HEAD "WARNING: Out of memory! Returning NULL (in %s)\n", file);
        return NULL;
    }
}

/* =============================================================================
	   Function:		Rgetcwd 	// external //
	   Author:		Rammi
	   Date:		11/13/1998

	   Return:		New memory with copied string depending on input (if
	                  buffer == NULL)

	   Parameter:			buffer		buffer for write (or NULL)
	                  size		buffer size
	         file		called from

	   Purpose: 	Wrapper function for getcwd() which sometimes returns
	         memory from heap.
	   ============================================================================= */
char *Rgetcwd(char *buffer, size_t size, const char *file)
{
#if defined(PLATFORM_WINDOWS)
    char *ret = _getcwd(buffer, size);
#else
    char *ret = getcwd(buffer, size);
#endif

    if (ret && !buffer)
    {
        /* create new memory to get internals fixed */
        char *newret = Rstrdup(ret, file);
        CALL_FREE(ret); /* free old stuff */
        ret = newret;   /* this was missing before 1.14 */
                        /* thanks to Greg Silverman who discovered it! */
    }

    return ret;
}

/* =============================================================================
	   Function:		Rmalloc_test		// external //
	   Author:		Rammi
	   Date:		04/11/1995

	   Return:		---

	   Parameter:		file		called from

	   Purpose: 	Explicitely test all blocks for integrity

	   ============================================================================= */
void Rmalloc_test(const char *file)
{
#if RM_TEST_DEPTH > 0
    TestAll(file);
#else
    fprintf(stderr, HEAD __FILE__
            " not compiled with RM_TEST_DEPTH > 0, call in %s senseless.\n",
            file);
#endif
}

/* =============================================================================
	   Function:		BlockSort		// local //
	   Author:		Rammi
	   Date:		04/15/1995

	   Return:		< 0 	A < B
	         0		A == B
	         > 0 	A > B

	   Parameter:		A, B

	   Purpose: 	sort function for qsort

	   ============================================================================= */
static int BlockSort(const begin **A, const begin **B)
{
    int ret;

    /* Sort for adress of string (tricky!) */
    if ((ret = (*A)->File - (*B)->File))
    {
        return ret;
    }

    /* sort for size */
    return ((int)(*A)->Size - (int)(*B)->Size);
}

#ifdef GENERATIONS
/* =============================================================================
	   Function:		BlockSortGenerations		// local //
	   Author:		Rammi
	   Date:		04/22/2002

	   Return:		< 0 	A < B
	         0		A == B
	         > 0 	A > B

	   Parameter:		A, B

	   Purpose: 	sort function for qsort, using the generations counter
	                  for sorting, too

	   ============================================================================= */
static int BlockSortGenerations(const begin **A, const begin **B)
{
    int ret = BlockSort(A, B);

    if (ret)
    {
        return ret;
    }

    /* sort for generation */
    return (*A)->Generation - (*B)->Generation;
}

#endif

/* =============================================================================
	   Function:		Rmalloc_stat		// extern //
	   Author:		Rammi
	   Date:		04/15/1995

	   Return:		---

	   Parameter:		file		caled from

	   Purpose: 	Show statistic
	   ============================================================================= */
void Rmalloc_stat(const char *file)
{
#if RM_TEST_DEPTH > 0
    TestAllConditionally(file);

#define STAT_HEAD "<MALLOC_STATS>\t"
    fprintf(stderr,
            STAT_HEAD "============ STATISTICS (%s) =============\n", file);
    if (!Global.BlockCount)
    {
        fprintf(stderr, STAT_HEAD "Nothing allocated.\n");
    }
    else
    {
        const begin **BlockVec;

        if ((BlockVec = (const begin **)CALL_MALLOC(Global.BlockCount * sizeof(begin *))) == NULL)
        {
            fprintf(stderr, STAT_HEAD "Couldn't allocate enough memory for statistics. Going on...\n");
        }
        else
        {
            unsigned i = 0;
            unsigned j;
            begin *B;
            unsigned count;
            size_t Mem = 0;
            unsigned nrBlocks;
#ifdef WITH_FLAGS
            size_t StaticMem = 0;
#endif
#ifdef GENERATIONS
            unsigned gen;
#endif

            /* add all blocks to vector */
            for (j = 0; j < HASHSIZE; j++)
            {
                for (B = Chain[j].Next; B != &Chain[j]; B = B->Next)
                {
#ifdef WITH_FLAGS
                    if (B->Flags & RM_STATIC)
                    {
                        StaticMem += B->Size;
                    }
                    else
                    {
                        BlockVec[i++] = B;
                    }
#else
                    BlockVec[i++] = B;
#endif
                }
            }
#ifdef WITH_FLAGS
            assert(i <= Global.BlockCount);
#else
            assert(i == Global.BlockCount);
#endif
            nrBlocks = i;

/* --- sort --- */
#ifdef GENERATIONS
            qsort(BlockVec, nrBlocks,
                  sizeof(begin *),
                  (int (*)(const void *, const void *))BlockSortGenerations);
#else
            qsort(BlockVec, nrBlocks,
                  sizeof(begin *),
                  (int (*)(const void *, const void *))BlockSort);
#endif

            for (i = 0; i < nrBlocks; i = j)
            {
                count = 1;
                for (j = i + 1; j < nrBlocks; j++)
                {
                    if (BlockSort(BlockVec + i, BlockVec + j) != 0)
                    {
                        break;
                    }
                    /* are equal */
                    count++;
                }
#ifdef GENERATIONS
                fprintf(stderr,
                        STAT_HEAD "%6d x %8u Bytes in %s, generations:",
                        count,
                        (unsigned)BlockVec[i]->Size,
                        BlockVec[i]->File);
                for (gen = 0; gen < count; gen++)
                {
                    if (gen == MAX_STAT_GENERATIONS)
                    {
                        fprintf(stderr, " ...");
                        break;
                    }
                    fprintf(stderr, " %d", BlockVec[gen + i]->Generation);
                }
                fprintf(stderr, "\n");
#else
                fprintf(stderr,
                        STAT_HEAD "%6d x %8u Bytes in %s\n",
                        count,
                        (unsigned)BlockVec[i]->Size,
                        BlockVec[i]->File);
#endif
                Mem += count * BlockVec[i]->Size;
            }

            /* and give free */
            CALL_FREE(BlockVec);

#ifdef WITH_FLAGS
            fprintf(stderr, STAT_HEAD "*Variable*\t%12u Bytes\n",
                    (unsigned)Mem);
            fprintf(stderr, STAT_HEAD "*Static*  \t%12u Bytes\n",
                    (unsigned)StaticMem);
            fprintf(stderr, STAT_HEAD "*Total*   \t%12u Bytes\n",
                    (unsigned)(Mem + StaticMem));
#else
            fprintf(stderr, STAT_HEAD "*Total*\t%u Bytes\n",
                    (unsigned)Mem);
#endif
        }
    }
    fprintf(stderr, STAT_HEAD "============ END OF STATISTICS =============\n");
#else
    fprintf(stderr, HEAD __FILE__ " not compiled with RM_TEST_DEPTH > 0, call in %s senseless.\n", file);
#endif
}

/* =============================================================================
	   Function:		Rmalloc_retag		// external //
	   Author:		Rammi
	   Date:		12/12/1997

	   Return:		the pointer p for possible chaining

	   Parameter:		p		pointer to allocated block (user)
	         file		called from

	   Purpose: 	Change file position in header.
	   ============================================================================= */
void *Rmalloc_retag(void *p, const char *file)
{
    if (p)
    {

#if RM_TEST_DEPTH > 0
        if (!Global.isInitialized)
        {
            fprintf(stderr, HEAD
                    "Calling RM_RETAG without having allocated block via rmalloc in\n%s",
                    file);
            abort();
        }
#endif

        begin *info = (begin *)(((char *)p) - START_SPACE);

        /* --- test integrity --- */
        ControlBlock(info, file);

        /* --- change file pos --- */
        info->File = file;
    }

    return p;
}

/* =============================================================================
	   Function:		Rmalloc_set_flags		// external //
	   Author:		Rammi
	   Date:		12/12/1997

	   Return:		the pointer p for possible chaining

	   Parameter:		p				pointer to allocated block (user)
	                  flags			Flags to set
	                  file		called from

	   Purpose: 		Set flags in header
	   ============================================================================= */
void *Rmalloc_set_flags(void *p, unsigned flags, const char *file)
{
#ifdef WITH_FLAGS
    if (p)
    {

#if RM_TEST_DEPTH > 0
        if (!Global.isInitialized)
        {
            fprintf(stderr, HEAD
                    "Calling RM_SET without having allocated block via rmalloc in\n%s",
                    file);
            abort();
        }
#endif

        begin *info = (begin *)(((char *)p) - START_SPACE);

        /* --- test integrity --- */
        ControlBlock(info, file);

        /* --- change flags --- */
        info->Flags |= flags;
    }
#endif

    return p;
}

/* =============================================================================
	   Function:		Rmalloc_reinit		// external //
	   Author:		Rammi
	   Date:		05/28/1998

	   Return:		---

	   Parameter:		---

	   Purpose: 		This reinits the lists. This is only for test purposes.
	         DON'T USE THIS FUNCTION!
	   ============================================================================= */
void Rmalloc_reinit(void)
{
#if RM_TEST_DEPTH > 0
    int i;
    /* --- init list heads (discarding everything!) --- */
    for (i = 0; i < HASHSIZE; i++)
    {
        memcpy(Chain + i, &ChainTempl, sizeof(begin));
        Chain[i].Next = Chain[i].Prev = Chain + i;
    }
#endif
#ifdef GENERATIONS
    cur_generation = 0;
#endif
}

#ifdef __cplusplus
}
#endif
