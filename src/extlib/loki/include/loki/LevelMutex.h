////////////////////////////////////////////////////////////////////////////////
//
// LevelMutex facility for the Loki Library
// Copyright (c) 2008 Richard Sposato
// The copyright on this file is protected under the terms of the MIT license.
//
// Permission to use, copy, modify, distribute and sell this software for any
// purpose is hereby granted without fee, provided that the above copyright
// notice appear in all copies and that both that copyright notice and this
// permission notice appear in supporting documentation.
//
// The author makes no representations about the suitability of this software
// for any purpose. It is provided "as is" without express or implied warranty.
//
////////////////////////////////////////////////////////////////////////////////

// $Id$

/// @file LevelMutex.h Defines classes and functions for LevelMutex facility.

#ifndef LOKI_LEVEL_MUTEX_H_INCLUDED
#define LOKI_LEVEL_MUTEX_H_INCLUDED


// ----------------------------------------------------------------------------

#include <vector>
#include <assert.h>
#include <time.h>

#if defined( _MSC_VER )
#include <Windows.h>
#else
#include <pthread.h>
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h> // declares sleep under Linux
#endif

/** @par thread_local Keyword
 The mutexes require compilers to provide thread local storage - meaning each
 thread gets its own copy of the data.  The next version of C++ will have a
 new keyword, thread_local for that purpose.  Some existing compilers already
 provide thread local storage using different syntax, so these lines use
 thread_local to mimic that syntax.  If your compiler provides thread local
 storage but using different syntax besides "thread_local", you may want to
 modify these lines.  If your compiler does not support thread local storage,
 you can't use LevelMutex.
 */
#ifndef LOKI_THREAD_LOCAL
#if defined( _MSC_VER )
#if ( _MSC_VER >= 1300 )
#define LOKI_THREAD_LOCAL __declspec( thread )
#else
#error "Only Visual Studio versions 7.0 and after supported."
#endif

#elif ( __GNUC__ )
#define LOKI_THREAD_LOCAL __thread

#else
#warning "Check if your compiler provides thread local storage."
#define LOKI_THREAD_LOCAL thread_local
#endif
#endif

#if defined( DEBUG ) || defined( _DEBUG )
#define LOKI_MUTEX_DEBUG_CODE( x ) x
#else
#define LOKI_MUTEX_DEBUG_CODE( x )
#endif


namespace Loki
{


// ----------------------------------------------------------------------------

class MutexErrors
{
public:

	/// @enum Type Possible error conditions detected by LevelMutex functions.
	enum Type
	{
		Success = 0,        ///< Operation occurred correctly.
		NoProblem,          ///< Pre-lock and pre-unlock checks passed.
		WrongLevel,         ///< All mutexes in container must have same level.
		LevelTooLow,        ///< Trying to unlock a mutex lower than current level.
		LevelTooHigh,       ///< Trying to lock a mutex higher than current level.
		TryFailed,          ///< TryLock call failed to lock mutex.
		NullMutexPointer,   ///< Container has a NULL pointer in it.
		DuplicateMutex,     ///< Container must have unique pointers - no duplicates.
		EmptyContainer,     ///< Container must have at least 1 pointer in it.
		AlreadyLocked,      ///< TryLock call failed because mutex already locked.
		WasntLocked,        ///< Unlock failed because mutex was not even locked.
		NotRecentLock,      ///< Mutex in container was not recently locked by this thread.
		NotLockedByThread,  ///< Can't unlock a mutex not locked by this thread.
		MultiUnlockFailed,  ///< MultiUnlock can't unlock at least 1 mutex in container.
		TimedOut,           ///< Wait time elapsed without locking mutex.
		TooMuchRecursion,   ///< Tried to relock a PThread mutex which is not re-entrant.
		NotInitialized,     ///< Tried to lock a PThread mutex which did not get setup.
		AlreadyInitialized, ///< PThread mutex initialized before ctor called.
		InvalidAttribute,   ///< PThread mutex improperly initialized.
		InvalidAddress,     ///< Bad pointer used to initialize a PThread mutex.
		ExceptionThrown,    ///< Exception caught in mutex operation.
		MayDeadlock,        ///< Locking this mutex may cause a deadlock.
		OtherError          ///< Unknown error occurred.
	};
};

// ----------------------------------------------------------------------------

/** @class LevelMutexInfo
 This monolithic base class stores common info for a template class used to
 control mutexes.  The template class, LevelMutex, is policy-based class.

 @par Implementation
 Each thread has a list of mutexes it locked.  When a mutex first gets locked, it
 gets added to the head of the list.  If locked again, LevelMutex merely increments
 a count.  When unlocked, the count gets decremented until it reaches zero, and
 then it gets removed from the list.  Each mutex has a pointer to the mutex most
 recently locked by the current thread.  The current level of a thread is always
 the level of the most recently locked mutex, or UnlockedLevel if the thread does
 not have any mutexes locked now.  A mutex is considered "recently" locked if it is at
 the head of the list, or the same level as the current mutex and also locked by the
 current thread.

 @par Class Invariants
 This class maintains invariants for each LevelMutexInfo so that no function
 calls corrupt a mutex.  Each function makes a call to IsValid at the start so
 that LevelMutex knows it acts on valid internal data.  Many functions call
 IsValid again when they return to insure the function did not leave any data in
 an invalid state.  The exit call to IsValid occurs through a tiny helper class
 called Checker to insure all data remain valid even when exceptions occur.
 Another helper class, MutexUndoer, unlocks mutexes in a container if an
 exception occurs during calls to MultiLock.

 @par Error Results
 Many functions return an enum value to indicate an error status.  Many enum values
 indicate errors detected within LevelMutex, but some indicate errors found in policy
 classes, SpinLevelMutex and SleepLevelMutex.
 */

class LevelMutexInfo
{
public:

	/** Level for thread that has not locked any mutex. Maximum possible level
	 for a mutex is UnlockedLevel-1;  No mutex may have a level of UnlockedLevel.
	 */
	static const unsigned int UnlockedLevel = 0xFFFFFFFF;

	/// Container for locking multiple mutexes at once.
	typedef ::std::vector< volatile LevelMutexInfo * > MutexContainer;
	typedef MutexContainer::iterator LevelMutexContainerIter;
	typedef MutexContainer::const_iterator LevelMutexContainerCIter;
	typedef MutexContainer::reverse_iterator LevelMutexContainerRIter;
	typedef MutexContainer::const_reverse_iterator LevelMutexContainerCRIter;

	/** Locks several mutexes at once.  Requires O(m + n*n) actions where m is the
	 number of mutexes currently locked by the thread and n is the number of mutexes
	 in the container. This provides strong exception safety. If an exception occurs,
	 any mutexes that were locked during this call will get unlocked.
	 @param mutexes Container of pointers to mutexes.  Container must have at
	  least 1 mutex, all mutexes must have the same level, no NULL pointers, and all
	  mutexes must not exceed the thread's current level.  This sorts the container
	  by address order.
	 @return Enum value indicating success or error.
	 */
	static MutexErrors::Type MultiLock( MutexContainer &mutexes );

	/** Locks several mutexes at once.  Requires O(m + n*n + n*t) actions where m is
	 the number of mutexes currently locked by the thread, n is the number of mutexes
	 in the container, and t is the wait time for each mutex. This provides strong
	 exception safety.  If an exception occurs, any mutexes that were locked during
	 this call will ge unlocked.
	 @param mutexes Container of pointers to mutexes.  Container must have at
	  least 1 mutex, all mutexes must have the same level, no NULL pointers, and all
	  mutexes must not exceed the thread's current level.  This sorts the container
	  by address order.
	 @param milliSeconds Amount of time to wait for each mutex.
	 @return Enum value indicating success or error.
	 */
	static MutexErrors::Type MultiLock( MutexContainer &mutexes,
	                                    unsigned int milliSeconds );

	/** Unlocks several mutexes at once.  Requires O(m) actions where m is the number of
	 mutexes in the container. This provides strong exception safety. If an exception
	 occurs when unlocking one mutex, other mutexes in the container get unlocked anyway.
	 @param mutexes Container of pointers to mutexes.  Container must have at least 1
	  mutex, all mutexes must have the same level, no NULL pointers, and all mutexes must
	  be locked by the current thread.  This sorts the container dby address order.
	 @return Enum value indicating success or error.
	 */
	static MutexErrors::Type MultiUnlock( MutexContainer &mutexes );

	/** Gives pointer to most recently locked mutex, or NULL if nothing locked.
	 The pointer is for a const mutex so the mutex can't be modified inappropriately.
	 The pointer is for a volatile mutex so callers can call volatile member
	 functions to get info about the mutex.
	 */
	static const volatile LevelMutexInfo *GetCurrentMutex( void );

	/// Returns the level of this mutex.
	inline unsigned int GetLevel( void ) const volatile
	{
		return m_level;
	}

	/// Returns true if this mutex was locked at least once.
	inline bool IsLocked( void ) const volatile
	{
		return ( 0 < m_count );
	}

	/// Returns count of how many times this mutex got locked.
	inline unsigned int GetLockCount( void ) const volatile
	{
		return m_count;
	}

	/// Returns pointer to mutex previously locked by the thread which locked this.
	inline const volatile LevelMutexInfo *GetPrevious( void ) const volatile
	{
		return m_previous;
	}

	/** Tries to lock mutex, and returns immediately if mutex already locked by
	 another thread.  It will return immediately with a value of AlreadyLocked
	 if the mutex was locked by a different thread.  It may throw an exception
	 or assert when errors occur if the ErrorPolicy class implements that behavior.
	 @return An error condition if any occurred, else Success.
	 */
	virtual MutexErrors::Type TryLock( void ) volatile = 0;

	/** Blocking call will attempt to lock mutex and wait until it can lock.
	 This may throw an exception if the lock failed or an error occurred - if
	 that is what the error policy specifies.
	 @return An error condition if any occurred, else Success.
	 */
	virtual MutexErrors::Type Lock( void ) volatile = 0;

	/** Attempts to lock mutex, but only waits for a limited amount of time
	 before it gives up.  Will return quickly if an error occurs before any
	 attempt to lock.  This may throw an exception if the lock failed or an
	 error occurred - if that is what the error policy specifies.
	 @param milliSeconds How long to wait.
	 @return An error condition if any occurred, else Success.
	 */
	virtual MutexErrors::Type Lock( unsigned int milliSeconds ) volatile = 0;

	/** Unlocks the mutex, or returns an error condition.  This may throw an
	 exception if the lock failed or an error occurred - if that is what the
	 error policy specifies.
	 @return An error condition if any occurred, else Success.
	 */
	virtual MutexErrors::Type Unlock( void ) volatile = 0;

	/** Returns true if this mutex was locked by current thread, and level is the same
	 as the current thread's level.  Which means this was the most recently locked
	 mutex, or it was locked along with several others of the same level recently.
	 */
	bool IsRecentLock( void ) const volatile;

	/** Returns true if this mutex was locked within the last count mutexes.
	 @param count How many recent mutexes to look through to find this mutex.
	 */
	bool IsRecentLock( unsigned int count ) const volatile;

	/// Returns true if this was locked by current thread.
	bool IsLockedByCurrentThread( void ) const volatile;

	/// Returns true if this was locked by another thread.
	bool IsLockedByAnotherThread( void ) const volatile;

protected:

	/** @class Checker Performs validity check on mutex to insure no class invariants
	 were violated inside any member function.  This class only gets used in debug
	 builds, and any instance of it gets optimized away in release builds.  A checker
	 is created inside many of member functions so that it's destructor gets called
	 when the function exits.  It determines if any class invariants were violated
	 during the function call.
	 */
	class Checker
	{
	public:
		inline explicit Checker( const volatile LevelMutexInfo *mutex ) :
			m_mutex( mutex ) {}
		inline ~Checker( void )
		{
			m_mutex->IsValid();
		}
	private:
		Checker( void );
		Checker( const Checker & );
		Checker &operator = ( const Checker & );
		const volatile LevelMutexInfo *m_mutex;
	};

	/** @class MutexUndoer
	 Undoes actions by MultiLock if an exception occurs.  It keeps track of
	 which mutexes in a container got locked, and if an exception occurs, then
	 the destructor unlocks them.  If MultiLock succeeds, then it cancels the
	 undoer so nothing gets unlocked inadvertently.
	 */
	class MutexUndoer
	{
	public:

		explicit MutexUndoer( MutexContainer &mutexes );
		~MutexUndoer( void );
		void SetPlace( LevelMutexContainerIter &here );
		void Cancel( void );

	private:

		MutexUndoer( void );
		MutexUndoer( const MutexUndoer & );
		MutexUndoer &operator = ( const MutexUndoer & );

		MutexContainer &m_mutexes;
		LevelMutexContainerIter m_here;
	};

	/** Returns true if linked-list of locked mutexes in this thread is valid.
	 Which means the list has no loops, and each previous mutex on the list has a
	 higher or same level as the current mutex.  Called by IsValid.
	 */
	static bool IsValidList( void );

	/** This is the only available constructor, and it forces any derived class to set
	 a level for each mutex.
	 */
	explicit LevelMutexInfo( unsigned int level );

	/// The destructor only gets called by the derived class.
	virtual ~LevelMutexInfo( void );

	MutexErrors::Type PreLockCheck( bool forTryLock ) volatile;

	MutexErrors::Type PreUnlockCheck( void ) volatile;

	/** This gets called after each call to DoLock and DoTryLock to make sure the data
	 members in this object get set correctly.
	 */
	void PostLock( void ) volatile;

	/// Gets called just before an attempt to unlock a mutex.
	void PreUnlock( void ) volatile;

	/// Called to relock a mutex already locked by the current thread.
	void IncrementCount( void ) volatile;

	/// Called to unlock a mutex locked multiple times by the current thread.
	void DecrementCount( void ) volatile;

	/** Returns true if no class invariant broken, otherwise asserts.  This function
	only gets called in debug builds.
	 */
	bool IsValid( void ) const volatile;

private:

	/// Copy constructor is not implemented.
	LevelMutexInfo( const LevelMutexInfo & );
	/// Copy-assignment operator is not implemented.
	LevelMutexInfo &operator = ( const LevelMutexInfo & );

	/** Called only by MultiLock & MultiUnlock to pass a result through an
	 error checking policy.
	 @param result What error condition to check.
	 @return Result or assertion or an exception - depending on error policy.
	 */
	virtual MutexErrors::Type DoErrorCheck( MutexErrors::Type result ) const volatile = 0;

	/// Called only by MultiLock to Lock each particular mutex within a container.
	virtual MutexErrors::Type LockThis( void ) volatile = 0;

	/** Called only by MultiLock to lock each particular mutex within a container.
	 @param milliSeconds How much time to wait before giving up on locking a mutex.
	 */
	virtual MutexErrors::Type LockThis( unsigned int milliSeconds ) volatile = 0;

	/// Called only by MultiUnlock to unlock each particular mutex within a container.
	virtual MutexErrors::Type UnlockThis( void ) volatile = 0;

	/// Pointer to singly-linked list of mutexes locked by the current thread.
	static LOKI_THREAD_LOCAL volatile LevelMutexInfo *s_currentMutex;

	/// Level of this mutex.
	const unsigned int m_level;

	/// How many times this mutex got locked.
	unsigned int m_count;

	/// Pointer to mutex locked before this one.
	volatile LevelMutexInfo *m_previous;

};

// ----------------------------------------------------------------------------

/** @class ThrowOnAnyMutexError
 Implements the ErrorPolicy for LevelMutex and throws an exception for any
 error condition.  Only allows MutexErrors::Success and MutexErrors::NoProblem
 to get through.  Useful for release builds.
 */
class ThrowOnAnyMutexError
{
public:
	static MutexErrors::Type CheckError( MutexErrors::Type error,
	                                     unsigned int level );
};

// ----------------------------------------------------------------------------

/** @class ThrowOnBadDesignMutexError
 Implements the ErrorPolicy for LevelMutex and throws an exception if the error
 indicates the programmer did not levelize the calls to mutexes.  Otherwise
 returns the error result.  Useful for release builds.
 */
class ThrowOnBadDesignMutexError
{
public:
	static MutexErrors::Type CheckError( MutexErrors::Type error,
	                                     unsigned int level );
};

// ----------------------------------------------------------------------------

/** @class AssertAnyMutexError
 Implements the ErrorPolicy for LevelMutex and asserts for any error condition.
 Only allows MutexErrors::Success and MutexErrors::NoProblem to get through.
 Useful for testing mutexes in debug builds.
 */
class AssertAnyMutexError
{
public:
	static inline MutexErrors::Type CheckError( MutexErrors::Type error,
	        unsigned int level )
	{
		(void)level;
		assert( ( error == MutexErrors::Success )
		        || ( error == MutexErrors::NoProblem ) );
		return error;
	}
};

// ----------------------------------------------------------------------------

/** @class AssertBadDesignMutexError
 Implements the ErrorPolicy for LevelMutex and asserts if the error
 indicates the programmer did not levelize the calls to mutexes.  Otherwise
 returns the error result.  Useful for testing mutexes in debug builds.
 */
class AssertBadDesignMutexError
{
public:
	static inline MutexErrors::Type CheckError( MutexErrors::Type error,
	        unsigned int level )
	{
		(void)level;
		assert( ( error != MutexErrors::LevelTooHigh )
		        && ( error != MutexErrors::LevelTooLow  ) );
		return error;
	}
};

// ----------------------------------------------------------------------------

/** @class JustReturnMutexError
 Implements the ErrorPolicy for LevelMutex and does nothing no matter how bad
 the error condition.  Only recommended use is for automated unit-testing of
 mutex policies.
 */
class JustReturnMutexError
{
public:
	static inline MutexErrors::Type CheckError( MutexErrors::Type error,
	        unsigned int level )
	{
		(void)level;
		return error;
	}
};

// ----------------------------------------------------------------------------

/** @class NoMutexWait
 Implements the WaitPolicy for LevelMutex.  Does nothing at all so it turns
 all wait loops into spin loops.  Useful for low-level mutexes.
 */
class NoMutexWait
{
public:
	static inline void Wait( void ) {}
};

// ----------------------------------------------------------------------------

/** @class MutexSleepWaits
 Implements the WaitPolicy for LevelMutex.  Sleeps for a moment so thread won't
 consume idle CPU cycles.  Useful for high-level mutexes.
 */
class MutexSleepWaits
{
public:
	static void Wait( void );
	static unsigned int sleepTime;
};

// ----------------------------------------------------------------------------

/** @class SpinLevelMutex
 Implements a spin-loop to wait for the mutex to unlock.  Since this class makes
 the thread wait in a tight spin-loop, it can cause the thread to remain busy
 while waiting and thus consume CPU cycles.  For that reason, this mutex is best
 used only for very low-level resources - especially resources which do not
 require much CPU time to exercise.  Rule of thumb: Use this only if all actions
 on the resource consume a very small number of CPU cycles.  Otherwise, use the
 SleepLevelMutex instead.
 */
class SpinLevelMutex
{
public:

	/// Constructs a spin-level mutex.
	explicit SpinLevelMutex( unsigned int level );

	/// Destructs the mutex.
	virtual ~SpinLevelMutex( void );

	virtual MutexErrors::Type Lock( void ) volatile;

	virtual MutexErrors::Type TryLock( void ) volatile;

	virtual MutexErrors::Type Unlock( void ) volatile;

	inline unsigned int GetLevel( void ) const volatile
	{
		return m_level;
	}

private:

	/// Copy constructor is not implemented.
	SpinLevelMutex( const SpinLevelMutex & );
	/// Copy-assignment operator is not implemented.
	SpinLevelMutex &operator = ( const SpinLevelMutex & );

#if defined( _MSC_VER )
#if ( _MSC_VER >= 1300 )
	/// The actual mutex.
	CRITICAL_SECTION m_mutex;
#else
#error "Only Visual Studio versions 7.0 and after supported."
#endif

#elif ( __GNUC__ )
	/// The actual mutex.
	pthread_mutex_t m_mutex;

#else
#error "Check if any mutex libraries are compatible with your compiler."
#endif

	/// Keep a copy of the mutex level around for error reporting.
	const unsigned int m_level;

}; // end class SpinLevelMutex

// ----------------------------------------------------------------------------

/** @class SleepLevelMutex
 Implements a sleeping loop to wait for the mutex to unlock.

 @par Purpose
 Since this class puts the thread to sleep for short intervals, you can use this
 class for most of your mutexes. Especially for locking any high level resources
 where any one operation on the resouce consumes many CPU cycles.  The purpose of
 this mutex is to reduce the number of CPU cycles spent in idle loops.  All
 SleepLevelMutex's should have higher levels than all your SpinLevelMutex's.

 @par Dependence on SpinLevelMutex
 This utilizes SpinLevelMutex so it does not have to re-implement the DoTryLock
 and DoUnlock functions the same way.  All it really needs is a DoLock function
 and the amount of time it should sleep if an attempt to lock a function fails.
 */
class SleepLevelMutex : public SpinLevelMutex
{
public:

	/** Constructs a levelized mutex that puts threads to sleep while they wait
	 for another thread to unlock the mutex.
	 @param level Level of this mutex.
	 */
	explicit SleepLevelMutex( unsigned int level );

	SleepLevelMutex( unsigned int level, unsigned int sleepTime );

	/// Destructs the mutex.
	virtual ~SleepLevelMutex( void );

	inline unsigned int GetSleepTime( void ) const volatile
	{
		return m_sleepTime;
	}

	inline void SetSleepTime( unsigned int sleepTime ) volatile
	{
		if ( 0 != sleepTime )
			m_sleepTime = sleepTime;
	}

#if defined( _MSC_VER )
	inline bool GetWakable( void ) const volatile
	{
		return m_wakable;
	}
	inline void SetWakable( bool wakable ) volatile { m_wakable = wakable; }
#endif

	/** Attempts to lock a mutex, and if it fails, then sleeps for a while
	 before attempting again.
	 */
	virtual MutexErrors::Type Lock( void ) volatile;

private:

	/// Default constructor is not implemented.
	SleepLevelMutex( void );
	/// Copy constructor is not implemented.
	SleepLevelMutex( const SleepLevelMutex & );
	/// Copy-assignment operator is not implemented.
	SleepLevelMutex &operator = ( const SleepLevelMutex & );

#if defined( _MSC_VER )
#if ( _MSC_VER >= 1300 )
	/// True if operating system may wake thread to respond to events.
	bool m_wakable;
#else
#error "Only Visual Studio versions 7.0 and after supported."
#endif
#endif

	/// How many milli-seconds to sleep before trying to lock mutex again.
	unsigned int m_sleepTime;

}; // end class SleepLevelMutex

// ----------------------------------------------------------------------------

/** @class LevelMutex
 Levelized mutex class prevents deadlocks by requiring programs to lock mutexes in
 the same order, and unlock them in reverse order.  This is accomplished by forcing
 each mutex to have a level and forcing code to lock mutexes with higher levels
 before locking mutexes at lower levels.  If you want to lock several mutexes, they
 must be locked in decreasing order by level, or if they are all of the same level,
 then locked by LevelMutex::MultiLock.

 @par Features
 - Immune: Very unlikely to deadlock since all mutexes are locked in the same
   order and unlocked in reverse order.
 - Scalable: Can handle any number of mutexes.
 - Efficient: Many operations occur in constant time, and most operations require
   no more than O(m) steps.
 - Exception safe: All operations provide strong safety or don't throw.
 - Extendable: Can work with existing mutexes through policy-based design.
 - Easily Extended: Derived classes only need to implement 5 functions and a mutex
   to get all the features of this class.
 - Re-Entrant: Allows for re-entrancy even if mutexes in policy classes don't.
 - Cost-Free: No resource allocations occur in LevelMutex - although user-defined
   policy classes may allocate resources.
 - Compact: Each LevelMutex object is small.
 - Portable: As long as your compiler and libraries can meet the requirements.
 - Robust: Maintains data integrity even if exceptions occur in policy classes.
 - Affording: Several functions provide information about a mutex which allows
   client code to easily choose correct actions.

 @par Requirements
 - Your compiler must allow for thread-specific data.
 - You must have a threading or mutex library.

 @par Policy-Based Design
 This class hosts 3 policies and a default level.  The policy-based design allows
 users to write their own policies to extend the behaviors of LevelMutex.  The
 paragraphs below say how to design a class for each policy.
 - MutexPolicy The mutex policy class.
 - defaultLevel A level for existing client code that calls a default constructor.
 - ErrorPolicy How the mutex should handle error conditions.
 - WaitPolicy Whether a thread should wait, and how long in some internal loops.

 @par MutexPolicy
 A policy class that wraps a low-level mutex. Loki provides two policy classes
 for the actual mutex (SpinLevelMutex and SleepLevelMutex), both of which wrap
 either pthreads or the Windows CRITICAL_SECTION. If you want to use a mutex
 mechanism besides one of those, then all you have to do is provide a class
 which wraps the mutex and implements these functions.
    explicit SpinLevelMutex( unsigned int level );
    virtual ~SpinLevelMutex( void );
    virtual MutexErrors::Type Lock( void ) volatile;
    virtual MutexErrors::Type TryLock( void ) volatile;
    virtual MutexErrors::Type Unlock( void ) volatile;
 Indeed, since the base class does most of the work, and provides all the interace
 and functionality to client classes, a derived class has very few requirements.
 It only needs to implement a single constructor, the destructor, some virtual
 functions, and whatever data members it requires.  You don't actually need to
 declare those functions as virtual if the policy class is not a base or child
 class.  In the parlance of design patterns, LevelMutex is a Template, and the
 MutexPolicy is a Strategy.

 @par DefaultLevel
 The template class requires a default level to use inside the default constructor.
 Some existing code calls instantiates mutexes with a default constructor, so the
 mutex must know what level to use there.  Please do not use zero or UnlockedLevel
 as the default level.

 @par ErrorPolicy
 This policy specifies how to handle error conditions.  The mutexes can return
 errors, assert, or throw exceptions.  I recommend that debug code use asserts,
 release code use exceptions, and unit-testing code just return errors.  The
 error policy class only needs to implement one function:
   static MutexErrors::Type CheckError( MutexErrors::Type error, unsigned int level );

 @par WaitPolicy
 This states whether the mutex should wait within some tight internal loops,
 how the waiting is done, and for how long.  A wait policy class could sleep,
 do nothing, check if other objects need attention, or check if the program
 received events or notices from the operating system.  It only needs to
 implement one function:
   static void Wait( void );

 @par Per-Function Usage
 If you implement a function with a static local mutex, then you have to insure
 the function is not called from a lower level via call-backs, virtual functions in
 interface classes.  If the function does get called from a lower level, you are
 setting up a potential deadlock.  LevelMutex will detect that by checking the
 current level and the local mutex's level, so it will refuse to lock the local mutex.

 @par Per-Object Usage
 If you use a mutex as a data member of an object to protect that object, then I
 recommend specifying which functions are volatile and which are not, and then only
 use the mutex within the volatile functions.  You may also want to provide accessor
 functions so that client code can lock and unlock the mutex either to allow for
 calling multiple operations without having to lock and unlock before and after each
 operation, or so they can lock it along with several other objects at the same
 level.

 @par Per-Class Usage
 If you make a static data member within a class, you can use that to lock any
 resources shared by those objects, or to require threads to act on only one object
 at a time.  You may also want to provide static accessor functions so that client
 code can lock several other resources at the same level.
 */

template
<
class MutexPolicy,
      unsigned int DefaultLevel,
      class ErrorPolicy = ::Loki::ThrowOnBadDesignMutexError,
      class WaitPolicy  = ::Loki::NoMutexWait
      >
class LevelMutex : public LevelMutexInfo
{
public:

	typedef ErrorPolicy EP;
	typedef WaitPolicy  WP;
	typedef MutexPolicy MP;

	/** This constructor allows callers to replace the default level with another
	 value.  It also acts as the default constructor for existing code which uses
	 default construction for mutexes.  This is the only time the DefaultLevel
	 template parameter gets used.
	 */
	explicit LevelMutex( unsigned int level = DefaultLevel ) :
		LevelMutexInfo( level ),
		m_mutex( level )
	{
		assert( IsValid() );
	}

	/// The destructor.
	~LevelMutex( void )
	{
		assert( IsValid() );
	}

	/** These functions allow callers to access the mutex in case they need to
	 modify specific values in the MutexPolicy (e.g. - sleep time, functors to
	 call as tasks, etc...)  There is one function for every combination of
	 const and volatile qualifiers so callers get a reference to a MutexPolicy
	 with the proper qualifiers.
	 */
	inline const volatile MutexPolicy &GetMutexPolicy( void ) const volatile
	{
		return m_mutex;
	}
	inline       volatile MutexPolicy &GetMutexPolicy( void )       volatile { return m_mutex; }
	inline const          MutexPolicy &GetMutexPolicy( void ) const
	{
		return m_mutex;
	}
	inline                MutexPolicy &GetMutexPolicy( void )
	{
		return m_mutex;
	}

	virtual MutexErrors::Type TryLock( void ) volatile
	{
		assert( IsValid() );
		LOKI_MUTEX_DEBUG_CODE( Checker checker( this ); (void)checker; )

		MutexErrors::Type result = LevelMutexInfo::PreLockCheck( true );
		if ( MutexErrors::Success == result )
			return MutexErrors::Success;
		else if ( MutexErrors::AlreadyLocked == result )
			return result;
		else if ( MutexErrors::NoProblem != result )
			return EP::CheckError( result, GetLevel() );

		assert( 0 == LevelMutexInfo::GetLockCount() );
		result = m_mutex.TryLock();
		if ( MutexErrors::Success != result )
			return EP::CheckError( result, GetLevel() );
		LevelMutexInfo::PostLock();

		return MutexErrors::Success;
	}

	virtual MutexErrors::Type Lock( void ) volatile
	{
		assert( IsValid() );
		LOKI_MUTEX_DEBUG_CODE( Checker checker( this ); (void)checker; )

		MutexErrors::Type result = LevelMutexInfo::PreLockCheck( false );
		if ( MutexErrors::Success == result )
			return MutexErrors::Success;
		else if ( MutexErrors::NoProblem != result )
			return EP::CheckError( result, GetLevel() );

		assert( !LevelMutexInfo::IsLockedByCurrentThread() );
		result = m_mutex.Lock();
		if ( MutexErrors::Success != result )
			return EP::CheckError( result, GetLevel() );
		PostLock();

		return MutexErrors::Success;
	}

	virtual MutexErrors::Type Lock( unsigned int milliSeconds ) volatile
	{
		assert( IsValid() );
		LOKI_MUTEX_DEBUG_CODE( Checker checker( this ); (void)checker; )

		MutexErrors::Type result = LevelMutexInfo::PreLockCheck( false );
		if ( MutexErrors::Success == result )
			return MutexErrors::Success;
		else if ( MutexErrors::NoProblem != result )
			return EP::CheckError( result, GetLevel() );

		assert( !LevelMutexInfo::IsLockedByCurrentThread() );
		clock_t timeOut = clock() + milliSeconds;
		while ( clock() < timeOut )
		{
			WP::Wait();
			result = m_mutex.TryLock();
			switch ( result )
			{
			case MutexErrors::Success:
			{
				PostLock();
				return MutexErrors::Success;
			}
			case MutexErrors::AlreadyLocked:
				return MutexErrors::AlreadyLocked;
			case MutexErrors::TryFailed:
				break;
			default:
				return EP::CheckError( result, GetLevel() );
			}
		}

		return MutexErrors::TimedOut;
	}

	virtual MutexErrors::Type Unlock( void ) volatile
	{
		assert( IsValid() );
		LOKI_MUTEX_DEBUG_CODE( Checker checker( this ); (void)checker; )

		MutexErrors::Type result = LevelMutexInfo::PreUnlockCheck();
		if ( MutexErrors::Success == result )
			return MutexErrors::Success;
		else if ( MutexErrors::NoProblem != result )
			return EP::CheckError( result, GetLevel() );

		LevelMutexInfo::PreUnlock();
		result = MutexErrors::OtherError;
		try
		{
			result = m_mutex.Unlock();
			if ( MutexErrors::Success != result )
				PostLock();
		}
		catch ( ... )
		{
			PostLock();
			result = MutexErrors::ExceptionThrown;
		}

		return result;
	}

private:

	/// Copy constructor is not implemented since mutexes don't get copied.
	LevelMutex( const LevelMutex & );
	/// Copy-assignment operator is not implemented since mutexes don't get copied.
	LevelMutex &operator = ( const LevelMutex & );

	virtual MutexErrors::Type DoErrorCheck( MutexErrors::Type result ) const volatile
	{
		return EP::CheckError( result, GetLevel() );
	}

	/** Called only by MultiLock to lock each particular mutex within a container.
	 This does not do pre-lock error checking since MultiLock does that.  Since
	 this skips the error checking, that means that callers of LevelMutex should
	 not call this function directly, and so this will not be publicly available.
	 @return Error status indicating success or reason for failure.
	 */
	virtual MutexErrors::Type LockThis( void ) volatile
	{
		assert( IsValid() );
		LOKI_MUTEX_DEBUG_CODE( Checker checker( this ); (void)checker; )
		assert( this != LevelMutexInfo::GetCurrentMutex() );

		const MutexErrors::Type result = m_mutex.Lock();
		if ( MutexErrors::Success != result )
			return result;
		PostLock();

		return MutexErrors::Success;
	}

	/** Called only by MultiLock to lock each particular mutex within a container.
	 This does not do pre-lock error checking since MultiLock does that.  Since
	 this skips the error checking, callers of LevelMutex should not call this
	 function directly, and so this will not be publicly available.
	 @param milliSeconds How much time to wait before giving up on locking a mutex.
	 @return Error status indicating success or reason for failure.
	 */
	virtual MutexErrors::Type LockThis( unsigned int milliSeconds ) volatile
	{
		assert( IsValid() );
		LOKI_MUTEX_DEBUG_CODE( Checker checker( this ); (void)checker; )

		clock_t timeOut = clock() + milliSeconds;
		while ( clock() < timeOut )
		{
			WP::Wait();
			const bool locked = ( MutexErrors::Success == m_mutex.TryLock() );
			if ( locked )
			{
				PostLock();
				return MutexErrors::Success;
			}
		}

		return MutexErrors::TimedOut;
	}

	/** Called only by MultiUnlock to unlock each mutex within a container.
	 This does not do pre-unlock error checking since MultiLock does that.  Since
	 this skips the error checking, callers of LevelMutex should not call this
	 function directly, and so this will not be publicly available.
	 @return Error status indicating success or reason for failure.
	 */
	virtual MutexErrors::Type UnlockThis( void ) volatile
	{
		assert( IsValid() );
		assert( NULL != LevelMutexInfo::GetCurrentMutex() );
		LOKI_MUTEX_DEBUG_CODE( Checker checker( this ); (void)checker; )

		if ( 1 < LevelMutexInfo::GetLockCount() )
		{
			LevelMutexInfo::DecrementCount();
			return MutexErrors::Success;
		}

		LevelMutexInfo::PreUnlock();
		MutexErrors::Type result = m_mutex.Unlock();

		return result;
	}

	/// An instance of an unleveled mutex wrapped to match LevelMutex's needs.
	MutexPolicy m_mutex;

}; // end class LevelMutex

// ----------------------------------------------------------------------------

/** Returns level of most recently locked mutex by this thread, or UnlockedLevel
 if no mutexes are locked.  Runs in constant time, and never throws exceptions.
 */
unsigned int GetCurrentThreadsLevel( void );

/** Returns count of how mutexes the current thread locked.  Requires O(m)
 actions where m is the number of mutexes in the thread.  Never throws exceptions.
 */
unsigned int CountMutexesInCurrentThread( void );

/** Returns count of how mutexes the current thread locked.  The lock count
 exceeds the number of mutexes locked by current thread if any mutex got locked
 more than once.  Requires O(m) actions where m is the number of mutexes in the
 thread.  Never throws exceptions.
 */
unsigned int CountLocksInCurrentThread( void );

/** Returns count of mutexes locked by current thread which have the same level
 as GetCurrentThreadsLevel.  Requires O(m) actions where m is the number of
 mutexes in the thread at current level.  Never throws exceptions.
 */
unsigned int CountMutexesAtCurrentLevel( void );

/** Determines if container of mutexes matches the recently locked mutexes.
 If they do match, it returns success, otherwise an error condition.
 */
MutexErrors::Type DoMutexesMatchContainer( const LevelMutexInfo::MutexContainer &mutexes );

// ----------------------------------------------------------------------------

/** @class MutexException
 Exception class used to throw error statuses about LevelMutex's up to code that
 can respond to mutex problems.  This class exists because it conveys more info
 about the error condition than just ::std::exception.
 */
class MutexException : public ::std::exception
{
public:

	/** Constructs an exception which stores information about a mutex and the
	 reason an attempt to use a mutex failed.
	 */
	MutexException( const char *message, unsigned int level, MutexErrors::Type reason );

	/// Copy constructor performs a member-by-member copy of an exception.
	MutexException( const MutexException &that ) throw ();

	/// Copy-assignment operator performs a member-by-member copy of an exception.
	MutexException &operator = ( const MutexException &that ) throw ();

	/// Destroys the exception.
	virtual ~MutexException( void ) throw();

	/// Returns a simple message about which operation failed.
	virtual const char *what( void ) const throw();

	/// Returns level of mutex(es) used when problem occurred.
	unsigned int GetLevel( void ) const
	{
		return m_level;
	}

	/// Returns an error status for why operation failed.
	MutexErrors::Type GetReason( void ) const
	{
		return m_reason;
	}

private:

	/// Default constructor is not implemented.
	MutexException( void ) throw ();

	/// Simple message about operation that failed.
	const char *m_message;
	/// Level of mutex(es) used when problem occurred.
	unsigned int m_level;
	/// Error status for why operation failed.
	MutexErrors::Type m_reason;

}; // end class MutexException

// ----------------------------------------------------------------------------

/** @class MutexLocker
 You can place an instance of this as a local variable inside a function to lock
 a single mutex.  It will lock the mutex if no error occurs, or throw if one
 does happen.  When the function ends, the destructor will determine if it needs
 to unlock the mutex.  This RAII technique insures the mutex gets unlocked even
 when exceptions occur.
 */
class MutexLocker
{
public:

	/** Creates an object to lock an unlock a mutex for a function.  This
	 will throw if an attempt to lock the mutex fails.
	 @param mutex Reference to the mutex.
	 @param lock True if function wants to lock the mutex as this gets
	  constructed.
	 */
	explicit MutexLocker( volatile LevelMutexInfo &mutex, bool lock = true );

	/** Creates an object to lock an unlock a mutex for a function.  This waits
	 a specified amount of time for another thread to unlock the mutex if it is
	 locked.  This will throw if an attempt to lock the mutex fails.
	 @param mutex Reference to the mutex.
	 @param milliSeconds Amount of time to wait for another thread to unlock
	  the mutex.
	 @param lock True if function wants to lock the mutex as this gets
	  constructed.
	 */
	MutexLocker( volatile LevelMutexInfo &mutex, unsigned int milliSeconds,
	             bool lock = true );

	/// Destructs the locker, and determines if it needs to unlock the mutex.
	~MutexLocker( void );

	/** You can call this to lock (or relock) a mutex.  In theory, you can lock
	 and unlock a mutex several times within a function in order to give other
	 threads access to a resource while this function does not need it.
	 @return True if mutex is locked by this, else false if not locked.
	 */
	bool Lock( void );

	/** You can call this to unlock a mutex before the destructor does it.
	 By unlocking the mutexes before returning, the function can do other
	 operations without making other threads wait too long.
	 @return True if unlocked by this, else false if not unlocked by this.
	 (Which is not the same as whether the mutex itself is locked or not by
	 another thread.)
	 */
	bool Unlock( void );

	/// Returns true if the mutex is locked by this object.
	inline bool IsLocked( void ) const
	{
		return m_locked;
	}

	/// Provides access to mutex controlled by this.
	const volatile LevelMutexInfo &GetMutex( void ) const
	{
		return m_mutex;
	}

private:

	/// Default constructor is not implemented.
	MutexLocker( void );
	/// Copy constructor is not implemented.
	MutexLocker( const MutexLocker & );
	/// Copy-assignment operator is not implemented.
	MutexLocker &operator = ( const MutexLocker & );

	/// True if mutex got locked.
	bool m_locked;

	/// Reference to mutex.
	volatile LevelMutexInfo &m_mutex;
};

// ----------------------------------------------------------------------------

/** @class MultiMutexLocker
 You can place an instance of this as a local variable inside a function to lock
 a collection of mutexes.  It locks them if no error occurs, or throws an
 exception if one does happen.  When the function ends, the destructor determines
 if it needs to unlock the mutexes.  This RAII technique insures the mutexes get
 unlocked even when exceptions occur.  You will also have to construct a
 MutexContainer as a local object within the same function.
 */
class MultiMutexLocker
{
public:

	/** Creates an object to lock and unlock a collection of mutexes for a function.
	 This will throw if an attempt to lock any mutex fails. If an exception occurs,
	 it unlocks mutexes it previously locked.
	 @param mutex Reference to a collection of mutexes.
	 @param lock True if function wants to lock the mutex as this gets
	  constructed.
	 */
	explicit MultiMutexLocker( LevelMutexInfo::MutexContainer &mutexes,
	                           bool lock = true );

	/** Creates an object to lock and unlock a collection of mutexes for a function.
	 This waits a specified amount of time for other threads to unlock each mutex
	 that is locked.  This will throw if an attempt to lock any mutex fails. If an
	 exception occurs, it unlocks mutexes it previously locked.
	 @param mutexes Reference to a collection of mutexes.
	 @param milliSeconds Amount of time to wait for another thread to unlock
	  the mutex.
	 @param lock True if function wants to lock the mutexes as this gets
	  constructed.
	 */
	MultiMutexLocker( LevelMutexInfo::MutexContainer &mutexes,
	                  unsigned int milliSeconds, bool lock = true );

	/// Destructs the locker, and determines if it needs to unlock the mutexes.
	~MultiMutexLocker( void );

	/** You can call this to lock (or relock) the mutexes.  In theory, you can lock
	 and unlock mutexes several times within a function in order to give other
	 threads access to resources while this function does not need them.
	 @return True if mutex is locked by this, else false if not locked.
	 */
	bool Lock( void );

	/** You can call this to unlock the mutexes before the destructor does it.
	 By unlocking the mutexes before returning, the function can do other
	 operations without making other threads wait too long.
	 @return True if unlocked by this, else false if not unlocked by this.
	 (Which is not the same as whether the mutex itself is locked or not by
	 another thread.)
	 */
	bool Unlock( void );

	/// Returns true if the mutexes are locked by this object.
	inline bool IsLocked( void ) const
	{
		return m_locked;
	}

	/// Provides access to the collection of mutexes controlled by this.
	const LevelMutexInfo::MutexContainer &GetMutexes( void ) const
	{
		return m_mutexes;
	}

private:

	/// Default constructor is not implemented.
	MultiMutexLocker( void );
	/// Copy constructor is not implemented.
	MultiMutexLocker( const MultiMutexLocker & );
	/// Copy-assignment operator is not implemented.
	MultiMutexLocker &operator = ( const MultiMutexLocker & );

	/// True if mutexes got locked.
	bool m_locked;

	/// Reference to external container of mutexes;
	LevelMutexInfo::MutexContainer &m_mutexes;
};

// ----------------------------------------------------------------------------

} // end namespace Loki

#endif  // end file guardian
