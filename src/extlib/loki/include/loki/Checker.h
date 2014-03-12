////////////////////////////////////////////////////////////////////////////////
//
// The Loki Library
// Copyright (c) 2008 Rich Sposato
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

/// @file Checker.h This file provides Loki's Checker facility.


// ----------------------------------------------------------------------------

#ifndef LOKI_CHECKER_H_INCLUDED
#define LOKI_CHECKER_H_INCLUDED

#include <exception>  // needed for calls to uncaught_exception.
#include <assert.h>


namespace Loki
{

/** @par ContractChecker and StaticChecker Overview
 The ContractChecker and StaticChecker classes have two purposes:
 - provide a mechanism by which programmers can determine which functions
   violate class/data invariants,
 - and determine which exception safety a function provides.

 @par Class & Data Invariants
 The ContractChecker and StaticChecker define invariants as "expressions that
 are true for particular data".  They uses a function which returns true if all
 data are valid, and returns false if any datum is invalid.  This is called the
 validator function, and the host class or function provides a pointer to it.
 The validator could also assert for any invariant which fails rather than
 return false.  If the validator is a static member function, you can use it
 with checkers in any function, but especially standalone functions and class
 static functions.  If the validator is a non-static member function, you can
 use it only within non-static member functions.

 @par Exception Safety Levels
 Years ago, David Abrahams formalized a framework for assessing the exception
 safety level a function provides.  His framework describes three levels of
 guarantees.  Any function which does not provide any of these levels is
 considered unsafe.  ContractChecker and StaticChecker determine a function's
 safety level through the use of policy classes.  Checker's policy classes can
 show if a function provides any of these three guarantees.  Since there is no
 universal way to detect leaks, this facility provides no mechanism for finding
 leaks, but users may create their own validators which do.  StaticChecker's
 policy classes only provide direct checking for the no-throw and invariant
 guarantees.  With some finesse, a programmer can write a validator for
 StaticChecker that checks for the Strong guarantee.

 - No-throw guarantee: A function will not throw any exceptions.
 - Strong guarantee: A function will not change data if an exception occurs.
   (Which I call the no-change guarantee.)
 - Basic guarantee: A function will not leak resources and data will remain
   in a valid state if an exception occurs.  (Which I call either the no-leak
   or no-break guarantee depending on context.)
 */

// ----------------------------------------------------------------------------

/** @class CheckForNoThrow

 @par Exception Safety Level:
 This exception-checking policy class for ContractChecker asserts if an
 exception exists.  Host classes can use this to show that a member function
 provides the no-throw exception safety guarantees.

 @par Requirements For Host Class:
 This policy imposes no requirements on a host class.
 */
template < class Host >
class CheckForNoThrow
{
public:

	inline explicit CheckForNoThrow( const Host * ) {}

	inline bool Check( const Host * ) const
	{
		const bool okay = ( !::std::uncaught_exception() );
		assert( okay );
		return okay;
	}
};

// ----------------------------------------------------------------------------

/** @class CheckForNoChange

 @par Exception Safety Level:
 This exception-checking policy class for ContractChecker asserts only if a
 copy of the host differs from the host object when an exception occurs.  Host
 classes can use this policy to show which member functions provide the strong
 exception guarantee.

 @par Requirements:
 This policy requires hosts to provide both the copy-constructor and the
 equality operator, and is intended for classes with value semantics.
 equality operator.
 */

template < class Host >
class CheckForNoChange
{
public:

	inline explicit CheckForNoChange( const Host *host ) :
		m_compare( *host ) {}

	inline bool Check( const Host *host ) const
	{
		const bool okay = ( !::std::uncaught_exception() )
		                  || ( m_compare == *host );
		assert( okay );
		return okay;
	}

private:
	Host m_compare;
};

// ----------------------------------------------------------------------------

/** @class CheckForNoChangeOrThrow

 @par Exception Safety Level:
 This exception-checking policy class for ContractChecker asserts either if a
 copy of the host differs from the original host object, or if an exception
 occurs. Host classes can use this policy to show which member functions provide
 the no-throw exception guarantee, and would never change data anyway.

 @par Requirements For Host Class:
 This policy requires hosts to provide both the copy-constructor and the
 equality operator, and is intended for classes with value semantics.
 */

template < class Host >
class CheckForNoChangeOrThrow
{
public:

	inline explicit CheckForNoChangeOrThrow( const Host *host ) :
		m_compare( *host ) {}

	inline bool Check( const Host *host ) const
	{
		bool okay = ( !::std::uncaught_exception() );
		assert( okay );
		okay = ( m_compare == *host );
		assert( okay );
		return okay;
	}

private:
	Host m_compare;
};

// ----------------------------------------------------------------------------

/** @class CheckForEquality

 @par Exception Safety Level:
 This exception-checking policy class for ContractChecker asserts if a copy of the host differs from the host object regardless of whether an exception occurs.
 Host classes can use this policy to show which member functions never change
 data members, and thereby provide the strong exception safety level by default.

 @par Requirements For Host Class:
 This policy requires hosts to provide both the copy-constructor and the
 equality operator, and is intended for classes with value semantics.
 */

template < class Host >
class CheckForEquality
{
public:

	inline explicit CheckForEquality( const Host *host ) :
		m_compare( *host ) {}

	inline bool Check( const Host *host ) const
	{
		const bool okay = ( m_compare == *host );
		assert( okay );
		return okay;
	}

private:
	Host m_compare;
};

// ----------------------------------------------------------------------------

/** @class CheckForNothing

 @par Exception Safety Level:
 This exception-checking policy class for ContractChecker does nothing when
 called.  Host classes can use this to show which member functions provide
 neither the strong nor no-throw exception guarantees.  The best guarantee such
 functions can provide is that nothing gets leaked.

 @par Requirements For Host Class:
 This policy imposes no requirements on a host class.
 */

template < class Host >
class CheckForNothing
{
public:
	inline explicit CheckForNothing( const Host * ) {}
	inline bool Check( const Host * ) const
	{
		return true;
	}
};

// ----------------------------------------------------------------------------

/** @class ContractChecker
 This class determines if a function violated any class invariant, but it also
 determines if a function fulfills its contract with client code.  In the
 "Design by Contract" paradigm, each function has certain pre-conditions and
 post-conditions which may differ from the class invariants.  This asserts if a
 check for an invariant fails as well as if any pre- or post-condition fails.
 It also demonstrate which exception safety level a function provides.

 @par Usage
 -# Implement a function that checks each class invariant.  The function must
    have the signature similar to the Validator type.  Something like:
    "bool Host::IsValid( void ) const;"
    - The function should return true if everything is okay, but false if
      something is wrong.
    - Or it could assert if anything is wrong.
    - Ideally, it should be private.
 -# Implement similar functions to check for pre-conditions and post-conditions.
    Functions which verify pre-conditions and post-conditions do not need to
    check all class invariants, just conditions specific to certain public
    functions in the host class.
 -# Declare some typedef's inside the class declaration like these.  Make one
    typedef for each exception policy you use.  I typedef'ed the CheckForNothing
    policy as CheckInvariants because even if a function can't provide either the
    no-throw nor the no-change policies, it should still make sure the object
    remains in a valid state.
    - typedef ::Loki::ContractChecker< Host, ::Loki::CheckForNoThrow  > CheckForNoThrow;
    - typedef ::Loki::ContractChecker< Host, ::Loki::CheckForNoChange > CheckForNoChange;
    - typedef ::Loki::ContractChecker< Host, ::Loki::CheckForEquality > CheckForEquality;
    - typedef ::Loki::ContractChecker< Host, ::Loki::CheckForNothing  > CheckInvariants;
 -# Construct a checker near the top of each member function - except in the
    validator member function.  Pass the this pointer and the address of your
    validator function into the checker's constructor.  You may also pass in pointers
    to function which check pre- and post-conditions.
    - If the function never throws, then use the CheckForNoThrow policy.
    - If the function never changes any data members, then use CheckForEquality
      policy.
    - If the function's normal execution flow changes data, but must make sure
      data remains unchanged when any exceptions occur, then use the
      CheckForNoChange policy.
    - Otherwise use the CheckInvariants policy.
 -# Recompile a debug version of your program, run the program and all the unit
    tests, and look for which assertions failed.
 */

template
<
class Host,
      template < class > class ExceptionPolicy
      >
class ContractChecker : public ExceptionPolicy< Host >
{
	/// Shorthand for the ExceptionPolicy class.
	typedef ExceptionPolicy< Host > Ep;

public:

	/// Signature for the validation function.
	typedef bool ( Host:: * Validator )( void ) const;

	/** The constructor makes sure the host is valid at the time the checker
	 was created, thus insuring the host object was not corrupt from the start.
	 @par host Pointer to host object.
	 @par validator Pointer to function that checks class invariants.
	 @par pre Optional pointer to function that checks pre-conditions.
	 @par post Optional pointer to function that checks post-conditions.
	 */
	inline ContractChecker( const Host *host, Validator validator,
	                        Validator pre = 0, Validator post = 0 ) :
		Ep( host ),
		m_host( host ),
		m_validator( validator ),
		m_pre( pre ),
		m_post( post )
	{
		assert( Check() );
		if ( 0 != m_pre )
			assert( ( m_host->*( m_pre ) )() );
	}

	/** The destructor checks if any Host invariants failed, and then calls the
	 ExceptionPolicy's Check function to determine what to do in case of an
	 exception.
	 */
	inline ~ContractChecker( void )
	{
		assert( Check() );
		if ( 0 != m_post )
			assert( ( m_host->*( m_post ) )() );
		assert( Ep::Check( m_host ) );
	}

	/** This first checks the invariants for ContractChecker, and then calls the
	 validator function for the host to make sure no class invariants were
	 broken by the host within the Host's member function body.  The host
	 member function can call Check directly to verify the object remains valid
	 at any time.  This does not care if the pre- and post-condition validator
	 pointers are null since a host class may pass in NULL pointers for either
	 to indicate the pre-conditions or post-conditions are the same as the
	 overall class invariants.
	 */
	inline bool Check( void ) const
	{
		assert( 0 != this );
		assert( 0 != m_host );
		assert( 0 != m_validator );
		// Now that this confirms the pointers to the host and validation
		// functions are not null, go ahead and validate the host object.
		const bool okay = ( m_host->*( m_validator ) )();
		assert( okay );
		return okay;
	}

private:

	/// Default constructor is not implemented.
	ContractChecker( void );
	/// Copy constructor is not implemented.
	ContractChecker( const ContractChecker & );
	/// Copy-assignment operator is not implemented.
	ContractChecker &operator = ( const ContractChecker & );

	/// Pointer to the host object.
	const Host *m_host;

	/// Pointer to member function that checks Host object's invariants.
	Validator m_validator;

	/// Pointer to member function that checks Host object's pre-conditions.
	Validator m_pre;

	/// Pointer to member function that checks Host object's post-conditions.
	Validator m_post;

};

// ----------------------------------------------------------------------------

/** @class CheckStaticForNoThrow

 @par Exception Safety Level:
 This exception-checking policy class for StaticChecker asserts if an exception
 exists.  Functions can use this to show they provide the no-throw exception
 safety guarantee.
 */
class CheckStaticForNoThrow
{
public:
	inline bool Check( void )
	{
		const bool okay = !::std::uncaught_exception();
		assert( okay );
		return okay;
	}
};

// ----------------------------------------------------------------------------

/** @class CheckStaticForNothing

 @par Exception Safety Level:
 This exception-checking policy class for StaticChecker does nothing when called.
 Functions can use this to show they might provide the weak exception guarantee.
 The best guarantee such functions can provide is that nothing gets leaked.
 */
class CheckStaticForNothing
{
public:
	inline bool Check( void )
	{
		return true;
	}
};

// ----------------------------------------------------------------------------

/** @class StaticChecker
 This class checks if a function provides the no-throw exception safety level
 and if the function violated any invariants.  Invariants for stand-alone and
 static functions act as pre-conditions and post-conditions.

 @par Usage
 -# Implement a function that checks the invariants associated with a function,
    or with the static data for a class.  The function must
    have the signature similar to the Validator type.  Something like:
    "static bool Host::StaticIsValid( void );" or "bool IsOkay( void );"
    - The function should return true if everything is okay, but false if
      something is wrong.
    - Or it could assert if anything is wrong.
 -# If the checker is for static functions within a class, declare typedef's
    inside the class declaration like these.  Make one typedef for each policy
    you use.  I typedef'ed the CheckForNothing policy as CheckInvariants because
    even if a function can't provide the no-throw guarantee, it should still
    make sure that static data remains in a valid state.
    - typedef ::Loki::StaticChecker< ::Loki::CheckForNoThrow > CheckStaticForNoThrow;
    - typedef ::Loki::StaticChecker< ::Loki::CheckForNothing > CheckStaticInvariants;
 -# Construct a checker near the top of each member function - except in the
    validator member function.  Pass the address of your validator function into
    the checker's constructor.
    - If the function never throws, then use the CheckForNoThrow policy.
    - Otherwise use the CheckInvariants policy.
 -# Recompile a debug version of your program, run it, and see if an assertion
    fails.
 */

template
<
class ExceptionPolicy
>
class StaticChecker : public ExceptionPolicy
{
	/// Shorthand for the ExceptionPolicy class.
	typedef ExceptionPolicy Ep;

public:

	/// Signature for the validation function.
	typedef bool ( * Validator )( void );

	/** The constructor makes sure the host is valid at the time the checker
	 was created, thus insuring the host object was not corrupt from the start.
	 @par validator Pointer to function that checks class invariants.
	 @par pre Optional pointer to function that checks pre-conditions.
	 @par post Optional pointer to function that checks post-conditions.
	 */
	inline explicit StaticChecker( Validator validator,
	                               Validator pre = 0, Validator post = 0 ) :
		Ep(),
		m_validator( validator ),
		m_pre( pre ),
		m_post( post )
	{
		assert( Check() );
		if ( 0 != m_pre )
			assert( m_pre() );
	}

	/** The destructor checks if any Host invariants failed, and then calls the
	 ExceptionPolicy's Check function to determine what to do in case of an
	 exception.
	 */
	inline ~StaticChecker( void )
	{
		assert( Check() );
		if ( 0 != m_post )
			assert( m_post() );
		assert( Ep::Check() );
	}

	/** This first checks its own invariants, and then calls the validator
	 function to make sure no invariants were broken by the function which
	 created this checker.  That function can call Check directly to verify the
	 data remains valid at any time.  This does not care if the pre- and post-
	 condition validator pointers are null since a host class may pass in NULL
	 pointers for either to indicate the pre-conditions or post-conditions are
	 the same as the overall class invariants.
	 */
	inline bool Check( void ) const
	{
		assert( 0 != this );
		assert( 0 != m_validator );
		// Now that this confirms the pointers to the host and validation
		// functions are not null, go ahead and validate the host object.
		const bool okay = m_validator();
		assert( okay );
		return okay;
	}

private:

	/// Default constructor is not implemented.
	StaticChecker( void );
	/// Copy constructor is not implemented.
	StaticChecker( const StaticChecker & );
	/// Copy-assignment operator is not implemented.
	StaticChecker &operator = ( const StaticChecker & );

	/// Pointer to member function that checks Host object's invariants.
	Validator m_validator;

	/// Pointer to member function that checks Host object's pre-conditions.
	Validator m_pre;

	/// Pointer to member function that checks Host object's post-conditions.
	Validator m_post;

};

// ----------------------------------------------------------------------------

}; // end namespace Loki

#endif
