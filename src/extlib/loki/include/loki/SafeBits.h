////////////////////////////////////////////////////////////////////////////////
// The Loki Library
// Copyright (c) 2009 by Fedor Pikus & Rich Sposato
// The copyright on this file is protected under the terms of the MIT license.
//
// Permission to use, copy, modify, distribute and sell this software for any
//     purpose is hereby granted without fee, provided that the above copyright
//     notice appear in all copies and that both that copyright notice and this
//     permission notice appear in supporting documentation.
//
// The author makes no claims about the suitability of this software for any
//     purpose. It is provided "as is" without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

// $Id$


#ifndef LOKI_INCLUDED_SAFE_BIT_FIELDS_H
#define LOKI_INCLUDED_SAFE_BIT_FIELDS_H

#include <cstdlib>
#include <assert.h>
#include <loki/static_check.h>


namespace Loki
{

/*
 ==========================================================================================================================================
   SafeBitField                    - type-safe class for bit fields.
   SafeBitConst                    - type-safe class for bit constants.
   SafeBitField is designed to be a [almost] drop-in replacement for integer flags and bit fields where individual bits are set and checked
   using symbolic names for flags:

      typedef unsigned long Labels_t;
      Labels_t labels;
      const Labels_t Label_A = 0x00000001;
      const Labels_t Label_B = 0x00000002;
      ...
      labels |= Label_B;
      if ( labels & Label_A ) { ... }

   Such code offers no protection against mismatching bit constants and bit fields:

      typedef unsigned long Kinds_t;
      Kinds_t kinds;
      const Kinds_t Kind_A = 0x00000004;
      ...
      if ( kinds & Label_A ) { ... } // Error but compiles

   SafeBitField is a drop-in replacement which generates a unique type for each bit field. Bit fields of different types cannot be applied
   to each other:

      LOKI_BIT_FIELD( unsigned long ) Labels_t;
      Labels_t labels;
      LOKI_BIT_CONST( Labels_t, Label_A, 1 );                                // 0x0001 - 1st bit is set
      LOKI_BIT_CONST( Labels_t, Label_B, 2 );                                // 0x0002 - 1st bit is set
      ...
      LOKI_BIT_FIELD( unsigned long ) Kinds_t;
      Kinds_t kinds;
      LOKI_BIT_CONST( Kinds_t, Kind_A, 3 );                                  // 0x0004 - 1st bit is set
      ...
      if ( kinds & Label_A ) { ... } // Does not compile

   Several other kinds of bit field misuse are caught by safe bit fields:

      if ( kinds & Kind_A == 0 ) { ... }
      if ( kinds && Kind_A ) { ... }

   There are few cases where drop-in replacement does not work:

   1. Operations involving bit fields and unnamed integers. Usually the integer in question is 0:

         Labels_t labels = 0;  // No longer compiles
         if ( ( labels & Label_A ) == 0 ) { ... } // Also does not compile

      The solution is to use named bit constants, including the one for 0:

         LOKI_BIT_CONST( Labels_t, Label_None, 0 );                               // 0x0000 - No bit is set
         Labels_t labels = Label_None; // Or just Labels_t labels; - constructor initializes to 0
         if ( ( labels & Label_A ) == Label_None ) { ... } // // Or just if ( labels & Label_A ) { ... }

   2. I/O and other operations which require integer variables and cannot be modified:

         void write_to_db( unsigned int word );
         Labels_t labels;
         write_to_db( labels ); // No longer compiles

      This problem is solved by reinterpreting the bit fields as an integer, the user is responsible for using the right
      type of integer:

         write_to_db( *((Labels_t::bit_word_t*)(&labels)) );

   ==========================================================================================================================================
*/

/// @par Non-Templated Initialization.
/// Not all compilers support template member functions where the template
/// arguments are not deduced but explicitly specified.  For these broken
/// compilers, a non-template make_bit_const() function is provided instead of
/// the template one. The only downside is that instead of compile-time checking
/// of the index argument, it does runtime checking.
#if defined(__SUNPRO_CC) || ( defined(__GNUC__) && (__GNUC__ < 3) )
#define LOKI_BIT_FIELD_NONTEMPLATE_INIT
#endif

/// @par Forbidding Conversions.
/// This incomplete type prevents compilers from instantiating templates for
/// type conversions which should not happen. This incomplete type must be a
/// template: if the type is incomplete at the point of template definition,
/// the  template is illegal (although the standard allows compilers to accept
/// or reject such code, §14.6/, so some compilers will not issue diagnostics
/// unless template is instantiated). The standard-compliant way is to defer
/// binding to the point of instantiation by making the incomplete type itself
/// a template.
template < typename > struct Forbidden_conversion;  // This struct must not be defined!

/// Forward declaration of the field type.
template <
unsigned int unique_index,
         typename word_t = unsigned long
         > class SafeBitField;

////////////////////////////////////////////////////////////////////////////////
/// \class SafeBitConst Bit constants.
///  This class defines a bit-field constant - a collection of unchanging bits
///  used to compare to bit-fields.  Instances of this class are intended to act
///  as labels for bit-fields.
///
/// \par Safety
///  - This class provides operations used for comparisons and conversions, but
///    no operations which may modify the value.
///  - As a templated class, it provides type-safety so bit values and constants
///    used for different reasons may not be unknowingly compared to each other.
///  - The unique_index template parameter insures the unique type of each bit
///    bit-field.  It shares the unique_index with a similar SafeBitField.
///  - Its operations only allow comparisons to other bit-constants and
///    bit-fields of the same type.
////////////////////////////////////////////////////////////////////////////////

template
<
unsigned int unique_index,
         typename word_t = unsigned long
         >
class SafeBitConst
{
public:

	/// Type of the bit field is available if needed.
	typedef word_t bit_word_t;
	/// Corresponding field type.
	typedef SafeBitField< unique_index, word_t > field_t;
	/// Typedef is not allowed in friendship declaration.
	friend class SafeBitField< unique_index, word_t >;

	// Static factory constructor, creates a bit constant with one bit set. The position of the bit is given by the template parameter,
	// bit 1 is the junior bit, i.e. make_bit_const<1>() returns 1. Bit index 0 is a special case and returns 0.
	// This function should be used only to initialize the static bit constant objects.
	// This function will not compile if the bit index is outside the vaild range.
	// There is also a compile-time assert to make sure the size of the class is the same as the size of the underlaying integer type.
	// This assert could go into the constructor, but aCC does not seem to understand sizeof(SafeBitConst) in the constructor.
	//
#ifndef LOKI_BIT_FIELD_NONTEMPLATE_INIT
	template < unsigned int i > static SafeBitConst make_bit_const()
	{
		LOKI_STATIC_CHECK( i <= ( 8 * sizeof(word_t) ), Index_is_beyond_size_of_data );
		LOKI_STATIC_CHECK( sizeof(SafeBitConst) == sizeof(word_t), Object_size_does_not_match_data_size );
		// Why check for ( i > 0 ) again inside the shift if the shift
		// can never be evaluated for i == 0? Some compilers see shift by ( i - 1 )
		// and complain that for i == 0 the number is invalid, without
		// checking that shift needs evaluating.
		return SafeBitConst( ( i > 0 ) ? ( word_t(1) << ( ( i > 0 ) ? ( i - 1 ) : 0 ) ) : 0 );
	}
#else
	static SafeBitConst make_bit_const( unsigned int i )
	{
		LOKI_STATIC_CHECK( sizeof(SafeBitConst) == sizeof(word_t), Object_size_does_not_match_data_size );
		assert( i <= ( 8 * sizeof(word_t) ) ); // Index is beyond size of data.
		// Why check for ( i > 0 ) again inside the shift if the shift
		// can never be evaluated for i == 0? Some compilers see shift by ( i - 1 )
		// and complain that for i == 0 the number is invalid, without
		// checking that shift needs evaluating.
		return SafeBitConst( ( i > 0 ) ? ( word_t(1) << ( ( i > 0 ) ? ( i - 1 ) : 0 ) ) : 0 );
	}
#endif

	/// Default constructor allows client code to construct bit fields on the stack.
	SafeBitConst() : word( 0 ) {}

	/// Copy constructor.
	SafeBitConst( const SafeBitConst &rhs ) : word( rhs.word ) {}

	/// Comparison operators which take a constant bit value.
	bool operator == ( const SafeBitConst &rhs ) const
	{
		return word == rhs.word;
	}
	bool operator != ( const SafeBitConst &rhs ) const
	{
		return word != rhs.word;
	}
	bool operator <  ( const SafeBitConst &rhs ) const
	{
		return word <  rhs.word;
	}
	bool operator >  ( const SafeBitConst &rhs ) const
	{
		return word >  rhs.word;
	}
	bool operator <= ( const SafeBitConst &rhs ) const
	{
		return word <= rhs.word;
	}
	bool operator >= ( const SafeBitConst &rhs ) const
	{
		return word >= rhs.word;
	}

	/// Comparision operators for mutable bit fields.
	bool operator == ( const field_t &rhs ) const
	{
		return word == rhs.word;
	}
	bool operator != ( const field_t &rhs ) const
	{
		return word != rhs.word;
	}
	bool operator <  ( const field_t &rhs ) const
	{
		return word <  rhs.word;
	}
	bool operator >  ( const field_t &rhs ) const
	{
		return word >  rhs.word;
	}
	bool operator <= ( const field_t &rhs ) const
	{
		return word <= rhs.word;
	}
	bool operator >= ( const field_t &rhs ) const
	{
		return word >= rhs.word;
	}

	/// Bitwise operations.  Operation-assignment operators are not needed,
	/// since bit constants cannot be changed after they are initialized.
	const SafeBitConst operator | ( const SafeBitConst &rhs ) const
	{
		return SafeBitConst( word | rhs.word );
	}
	const SafeBitConst operator & ( const SafeBitConst &rhs ) const
	{
		return SafeBitConst( word & rhs.word );
	}
	const SafeBitConst operator ^ ( const SafeBitConst &rhs ) const
	{
		return SafeBitConst( word ^ rhs.word );
	}
	const SafeBitConst operator ~ ( void ) const
	{
		return SafeBitConst( ~word );
	}

	/// These bitwise operators return a bit-field instead of a bit-const.
	field_t operator | ( const field_t &rhs ) const
	{
		return field_t( word | rhs.word );
	}
	field_t operator & ( const field_t &rhs ) const
	{
		return field_t( word & rhs.word );
	}
	field_t operator ^ ( const field_t &rhs ) const
	{
		return field_t( word ^ rhs.word );
	}

	/// The shift operators move bits inside the bit field.  These are useful in
	/// loops which act over bit fields and increment them.
	const SafeBitConst operator << ( unsigned int s ) const
	{
		return SafeBitConst( word << s );
	}
	const SafeBitConst operator >> ( unsigned int s ) const
	{
		return SafeBitConst( word >> s );
	}

	/// Word size is also the maximum number of different bit fields for a given word type.
	static size_t size()
	{
		return ( 8 * sizeof( word_t ) );
	}

private:

	/// Copy-assignment operator is not implemented since it does not make sense
	/// for a constant object.
	SafeBitConst operator = ( const SafeBitConst &rhs );

	// Private constructor from an integer type.
	explicit SafeBitConst( word_t init ) : word( init ) {}

	/// This data stores a single bit value.  It is declared const to enforce
	// constness for all functions of this class.
	const word_t word;

	// Here comes the interesting stuff: all the operators designed to
	// trap unintended conversions and make them not compile.
	// Operators below handle code like this:
	//    SafeBitField<1> label1;
	//    SafeBitField<2> label2;
	//    if ( label1 & label2 ) { ... }

	// These operators are private, and will not instantiate in any
	// event because of the incomplete Forbidden_conversion struct.
	template < typename T > SafeBitConst operator|( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitConst operator&( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitConst operator^( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitConst operator|=( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitConst operator&=( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitConst operator^=( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}

	// And the same thing for comparisons: private and unusable.
	//    if ( label1 == label2 ) { ... }
	template < typename T > bool operator==( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator!=( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator<( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator>( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator<=( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator>=( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
};


////////////////////////////////////////////////////////////////////////////////
/// \class SafeBitConst Bit constants.
///  This class defines a bit-field constant - a collection of unchanging bits
///  used to compare to bit-fields.  Instances of this class are intended to
///  store bit values.
///
/// \par Safety
///  - This class provides operations used for comparisons and conversions, and
///    also operations which may safely modify the value.
///  - As a templated class, it provides type-safety so bit values and constants
///    used for different reasons may not be unknowingly compared to each other.
///  - The unique_index template parameter insures the unique type of each bit
///    bit-field.  It shares the unique_index with a similar SafeBitConst.
///  - Its operations only allow comparisons to other bit-constants and
///    bit-fields of the same type.
////////////////////////////////////////////////////////////////////////////////

template
<
unsigned int unique_index,
         typename word_t
         >
class SafeBitField
{
public:

	/// Type of the bit field is available if needed.
	typedef word_t bit_word_t;
	/// Corresponding field type.
	typedef SafeBitConst< unique_index, word_t > const_t;
	/// Typedef is not allowed in friendship declaration.
	friend class SafeBitConst<unique_index, word_t>;

	/// Default constructor allows client code to construct bit fields on the stack.
	SafeBitField() : word( 0 ) {}

	/// Copy constructor and assignment operators.
	SafeBitField( const SafeBitField &rhs ) : word( rhs.word ) {}
	SafeBitField &operator = ( const SafeBitField &rhs )
	{
		word = rhs.word;
		return *this;
	}

	/// Copy constructor and assignment operators from constant bit fields.
	SafeBitField( const const_t &rhs ) : word( rhs.word ) {}
	SafeBitField &operator = ( const const_t &rhs )
	{
		word = rhs.word;
		return *this;
	}

	/// These comparison operators act on bit-fields of the same type.
	bool operator == ( const SafeBitField &rhs ) const
	{
		return word == rhs.word;
	}
	bool operator != ( const SafeBitField &rhs ) const
	{
		return word != rhs.word;
	}
	bool operator <  ( const SafeBitField &rhs ) const
	{
		return word <  rhs.word;
	}
	bool operator >  ( const SafeBitField &rhs ) const
	{
		return word >  rhs.word;
	}
	bool operator <= ( const SafeBitField &rhs ) const
	{
		return word <= rhs.word;
	}
	bool operator >= ( const SafeBitField &rhs ) const
	{
		return word >= rhs.word;
	}

	/// These comparison operators act on bit-constants of a similar type.
	bool operator == ( const const_t &rhs ) const
	{
		return word == rhs.word;
	}
	bool operator != ( const const_t &rhs ) const
	{
		return word != rhs.word;
	}
	bool operator <  ( const const_t &rhs ) const
	{
		return word <  rhs.word;
	}
	bool operator >  ( const const_t &rhs ) const
	{
		return word >  rhs.word;
	}
	bool operator <= ( const const_t &rhs ) const
	{
		return word <= rhs.word;
	}
	bool operator >= ( const const_t &rhs ) const
	{
		return word >= rhs.word;
	}

	/// Bitwise operations that use bit-fields.
	SafeBitField operator |  ( const SafeBitField &rhs ) const
	{
		return SafeBitField( word | rhs.word );
	}
	SafeBitField operator &  ( const SafeBitField &rhs ) const
	{
		return SafeBitField( word & rhs.word );
	}
	SafeBitField operator ^  ( const SafeBitField &rhs ) const
	{
		return SafeBitField( word ^ rhs.word );
	}
	SafeBitField operator ~  ( void ) const
	{
		return SafeBitField( ~word );
	}
	SafeBitField operator |= ( const SafeBitField &rhs )
	{
		word |= rhs.word;
		return SafeBitField( *this );
	}
	SafeBitField operator &= ( const SafeBitField &rhs )
	{
		word &= rhs.word;
		return SafeBitField( *this );
	}
	SafeBitField operator ^= ( const SafeBitField &rhs )
	{
		word ^= rhs.word;
		return SafeBitField( *this );
	}

	/// Bitwise operators that use bit-constants.
	SafeBitField operator |  ( const_t rhs ) const
	{
		return SafeBitField( word | rhs.word );
	}
	SafeBitField operator &  ( const_t rhs ) const
	{
		return SafeBitField( word & rhs.word );
	}
	SafeBitField operator ^  ( const_t rhs ) const
	{
		return SafeBitField( word ^ rhs.word );
	}
	SafeBitField operator |= ( const_t rhs )
	{
		word |= rhs.word;
		return SafeBitField( *this );
	}
	SafeBitField operator &= ( const_t rhs )
	{
		word &= rhs.word;
		return SafeBitField( *this );
	}
	SafeBitField operator ^= ( const_t rhs )
	{
		word ^= rhs.word;
		return SafeBitField( *this );
	}

	// Conversion to bool.
	// This is a major source of headaches, but it's required to support code like this:
	//    const static SafeBitConst<1> Label_value = SafeBitConst<1>::make_bit_const<1>();
	//    SafeBitField<1> label;
	//    if ( label & Label_value ) { ... } // Nice...
	//
	// The downside is that this allows all sorts of nasty conversions. Without additional precautions, bit fields of different types
	// can be converted to bool and then compared or operated on:
	//    SafeBitField<1> label1;
	//    SafeBitField<2> label2;
	//    if ( label1 == label2 ) { ... } // Yuck!
	//    if ( label1 & label2 ) { ... } // Blech!
	//
	// It is somewhat safer to convert to a pointer, at least pointers to different types cannot be readilly compared, and there are no
	// bitwise operations on pointers, but the conversion from word_t to a pointer can have run-time cost if they are of different size.
	//
	operator const bool() const
	{
		return ( 0 != word );
	}

	// Shift operators shift bits inside the bit field. Does not make
	// sense, most of the time, except perhaps to loop over labels and
	// increment them.
	SafeBitField operator <<  ( unsigned int s )
	{
		return SafeBitField( word << s );
	}
	SafeBitField operator >>  ( unsigned int s )
	{
		return SafeBitField( word >> s );
	}
	SafeBitField operator <<= ( unsigned int s )
	{
		word <<= s;
		return *this;
	}
	SafeBitField operator >>= ( unsigned int s )
	{
		word >>= s;
		return *this;
	}

	// Word size is also the maximum number of different bit fields for
	// a given word type.
	static size_t size( void )
	{
		return ( 8 * sizeof( word_t ) );
	}

private:

	/// Private constructor from an integer type. Don't put too much stock into
	/// explicit declaration, it's better than nothing but does not solve all
	/// problems with undesired conversions because SafeBitField coverts to bool.
	explicit SafeBitField( word_t init ) : word( init ) {}

	/// This stores the bits.
	word_t word;

	// Here comes the interesting stuff: all the operators designed to
	// trap unintended conversions and make them not compile.
	// Operators below handle code like this:
	//    SafeBitField<1> label1;
	//    SafeBitField<2> label2;
	//    if ( label1 & label2 ) { ... }

	// These operators are private, and will not instantiate in any
	// event because of the incomplete Forbidden_conversion struct.
	template < typename T > SafeBitField operator |  ( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitField operator &  ( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitField operator ^  ( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitField operator |= ( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitField operator &= ( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}
	template < typename T > SafeBitField operator ^= ( T ) const
	{
		Forbidden_conversion< T > wrong;
		return *this;
	}

	// And the same thing for comparisons:
	//    if ( label1 == label2 ) { ... }
	template < typename T > bool operator == ( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator != ( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator <  ( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator >  ( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator <= ( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
	template < typename T > bool operator >= ( const T ) const
	{
		Forbidden_conversion< T > wrong;
		return true;
	}
};

// The above template member operators catch errors when the first
// argument to a binary operator is a label, but they don't work when
// the first argument is an integer and the second one is a label: the
// label converts to bool and the operator is performed on two integers.
// These operators catch errors like this:
//    SafeBitField<1> label1;
//    SafeBitField<2> label2;
//    if ( !label1 & label2 ) { ... }
// where the first label is converted to bool (these errors cannot be
// caught by member operators of SafeBitField class because the first
// argument is not SafeBitField but bool.
//
// If used, these operators will not instantiate because of the
// incomplete Forbidden_conversion struct.

template < unsigned int unique_index, typename word_t >
inline SafeBitField< unique_index, word_t > operator & ( bool, SafeBitField< unique_index, word_t > rhs )
{
	Forbidden_conversion<word_t> wrong;
	return rhs;
}

template < unsigned int unique_index, typename word_t >
inline SafeBitField< unique_index, word_t > operator | ( bool, SafeBitField< unique_index, word_t > rhs )
{
	Forbidden_conversion< word_t > wrong;
	return rhs;
}

template < unsigned int unique_index, typename word_t >
inline SafeBitField< unique_index, word_t > operator ^ ( bool, SafeBitField< unique_index, word_t > rhs )
{
	Forbidden_conversion< word_t > wrong;
	return rhs;
}

template < unsigned int unique_index, typename word_t >
inline SafeBitField< unique_index, word_t > operator == ( bool, SafeBitField< unique_index, word_t > rhs )
{
	Forbidden_conversion< word_t > wrong;
	return rhs;
}

template < unsigned int unique_index, typename word_t >
inline SafeBitField< unique_index, word_t > operator != ( bool, SafeBitField< unique_index, word_t > rhs )
{
	Forbidden_conversion< word_t > wrong;
	return rhs;
}

// Finally, few macros. All macros are conditionally defined to use the SafeBitField classes if LOKI_SAFE_BIT_FIELD is defined. Otherwise,
// the macros fall back on the use of typedefs and integer constants. This provides no addititonal safety but allows the code to support the
// mixture of compilers which are broken to different degrees.
#define LOKI_SAFE_BIT_FIELD

// The first macro helps to declare new bit field types:
// LOKI_BIT_FIELD( ulong ) field_t;
// This creates a typedef field_t for SafeBitField<unique_index, ulong> where index is the current line number. Since line numbers __LINE__ are counted
// separately for all header files, this ends up being the same type in all files using the header which defines field_t.
#ifdef LOKI_SAFE_BIT_FIELD
#define LOKI_BIT_FIELD( word_t ) typedef SafeBitField<__LINE__, word_t>
#else
#define LOKI_BIT_FIELD( word_t ) typedef word_t
#endif // LOKI_SAFE_BIT_FIELD

// The second macro helps to declare static bit constants:
// LOKI_BIT_CONST( field_t, Label_1, 1 );
// creates new bit field object named Label_1 of type field_t which represents the field with the 1st (junior) bit set.
#ifdef LOKI_SAFE_BIT_FIELD
#ifndef LOKI_BIT_FIELD_NONTEMPLATE_INIT
#define LOKI_BIT_CONST( field_t, label, bit_index ) \
            static const field_t::const_t label = field_t::const_t::make_bit_const<bit_index>()
#else
#define LOKI_BIT_CONST( field_t, label, bit_index ) \
            static const field_t::const_t label = field_t::const_t::make_bit_const( bit_index )
#endif // LOKI_BIT_FIELD_NONTEMPLATE_INIT
#else
inline size_t make_bit_const( size_t i )
{
	return ( i > 0 ) ? ( size_t(1) << ( ( i > 0 ) ? ( i - 1 ) : 0 ) ) : 0;
}
#define LOKI_BIT_CONST( field_t, label, bit_index ) static const field_t label = make_bit_const( bit_index )
#endif // LOKI_SAFE_BIT_FIELD

// The third macro helps to declare complex bit constants which are combination of several bits:
// LOKI_BIT_CONSTS( field_t, Label12 ) = Label_1 | Label_2;
#ifdef LOKI_SAFE_BIT_FIELD
#define LOKI_BIT_CONSTS( field_t, label ) static const field_t::const_t label
#else
#define LOKI_BIT_CONSTS( field_t, label ) static const field_t label
#endif // LOKI_SAFE_BIT_FIELD

// The fourth macro helps to declare the maximum number of bit constants for a given type:
// static const size_t count = LOKI_BIT_FIELD_COUNT( field_t );
// declared a variable "count" initialized to field_t::size()
#ifdef LOKI_SAFE_BIT_FIELD
#define LOKI_BIT_FIELD_COUNT( field_t ) field_t::size()
#else
#define LOKI_BIT_FIELD_COUNT( field_t ) ( 8 * sizeof(field_t) )
#endif // LOKI_SAFE_BIT_FIELD

} // namespace Loki

#endif // LOKI_INCLUDED_SAFE_BIT_FIELDS_H
