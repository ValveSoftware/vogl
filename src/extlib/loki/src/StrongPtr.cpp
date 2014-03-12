////////////////////////////////////////////////////////////////////////////////
// The Loki Library
// Copyright (c) 2006 Rich Sposato
// Permission to use, copy, modify, distribute and sell this software for any
//     purpose is hereby granted without fee, provided that the above copyright
//     notice appear in all copies and that both that copyright notice and this
//     permission notice appear in supporting documentation.
// The author makes no representations about the
//     suitability of this software for any purpose. It is provided "as is"
//     without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

// $Id: StrongPtr.cpp 819 2007-03-07 00:30:12Z rich_sposato $


#include <loki/StrongPtr.h>

#include <memory.h>
#ifdef DO_EXTRA_LOKI_TESTS
#include <cassert>
#endif

#include <loki/SmallObj.h>


// ----------------------------------------------------------------------------

namespace Loki
{

// ----------------------------------------------------------------------------

TwoRefCounts::TwoRefCounts( bool strong )
	: m_counts( NULL )
{
	void *temp = SmallObject<>::operator new(
	                 sizeof(Loki::Private::TwoRefCountInfo) );
#ifdef DO_EXTRA_LOKI_TESTS
	assert( temp != 0 );
#endif
	m_counts = new ( temp ) Loki::Private::TwoRefCountInfo( strong );
}

// ----------------------------------------------------------------------------

TwoRefCounts::TwoRefCounts( const void *p, bool strong )
	: m_counts( NULL )
{
	void *temp = SmallObject<>::operator new(
	                 sizeof(Loki::Private::TwoRefCountInfo) );
#ifdef DO_EXTRA_LOKI_TESTS
	assert( temp != 0 );
#endif
	void *p2 = const_cast< void * >( p );
	m_counts = new ( temp ) Loki::Private::TwoRefCountInfo( p2, strong );
}

// ----------------------------------------------------------------------------

void TwoRefCounts::Increment( bool strong )
{
	if ( strong )
	{
		m_counts->IncStrongCount();
	}
	else
	{
		m_counts->IncWeakCount();
	}
}

// ----------------------------------------------------------------------------

bool TwoRefCounts::Decrement( bool strong )
{
	if ( strong )
	{
		m_counts->DecStrongCount();
	}
	else
	{
		m_counts->DecWeakCount();
	}
	return !m_counts->HasStrongPointer();
}

// ----------------------------------------------------------------------------

void TwoRefCounts::Swap( TwoRefCounts &rhs )
{
	std::swap( m_counts, rhs.m_counts );
}

// ----------------------------------------------------------------------------

void TwoRefCounts::ZapPointer( void )
{
#ifdef DO_EXTRA_LOKI_TESTS
	assert( !m_counts->HasStrongPointer() );
#endif
	if ( m_counts->HasWeakPointer() )
	{
		m_counts->ZapPointer();
	}
	else
	{
		SmallObject<>::operator delete ( m_counts,
		                                 sizeof(Loki::Private::TwoRefCountInfo) );
		m_counts = NULL;
	}
}


// ----------------------------------------------------------------------------

TwoRefLinks::TwoRefLinks( const void *p, bool strong )
	: m_pointer( const_cast< void * >( p ) )
	, m_strong( strong )
{
	m_prev = m_next = this;
#ifdef DO_EXTRA_LOKI_TESTS
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
#endif
}

// ----------------------------------------------------------------------------

TwoRefLinks::TwoRefLinks( const TwoRefLinks &rhs, bool strong )
	: m_pointer( rhs.m_pointer )
	, m_prev( const_cast< TwoRefLinks * >( &rhs ) )
	, m_next( rhs.m_next )
	, m_strong( strong )
{
	m_prev->m_next = this;
	m_next->m_prev = this;

#ifdef DO_EXTRA_LOKI_TESTS
	assert( m_prev->HasPrevNode( this ) );
	assert( m_next->HasNextNode( this ) );
	assert( rhs.m_next->HasNextNode( this ) );
	assert( rhs.m_prev->HasPrevNode( this ) );
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
	assert( CountPrevCycle( this ) == CountNextCycle( &rhs ) );
	assert( AllNodesHaveSamePointer() );
#endif
}

// ----------------------------------------------------------------------------

void TwoRefLinks::SetPointer( void *p )
{
	TwoRefLinks *node = m_prev;
	if ( ( this == node ) || ( 0 == node ) )
		return;

#ifdef DO_EXTRA_LOKI_TESTS
	assert( m_prev->HasPrevNode( this ) );
	assert( m_next->HasNextNode( this ) );
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
	assert( AllNodesHaveSamePointer() );
#endif

	while ( node != this )
	{
		node->m_pointer = p;
		node = node->m_next;
	}
	m_pointer = node;

#ifdef DO_EXTRA_LOKI_TESTS
	assert( m_prev->HasPrevNode( this ) );
	assert( m_next->HasNextNode( this ) );
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
	assert( AllNodesHaveSamePointer() );
#endif
}

// ----------------------------------------------------------------------------

bool TwoRefLinks::Release( bool strong )
{

	(void)strong;
#ifdef DO_EXTRA_LOKI_TESTS
	assert( strong == m_strong );
	assert( m_prev->HasPrevNode( this ) );
	assert( m_next->HasNextNode( this ) );
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
	assert( AllNodesHaveSamePointer() );
#endif

	if ( NULL == m_next )
	{
#ifdef DO_EXTRA_LOKI_TESTS
		assert( NULL == m_prev );
#endif
		// Return false so it does not try to destroy shared object
		// more than once.
		return false;
	}
	else if (m_next == this)
	{
#ifdef DO_EXTRA_LOKI_TESTS
		assert(m_prev == this);
#endif
		// Set these to NULL to prevent re-entrancy.
		m_prev = NULL;
		m_next = NULL;
		// Last one in the cycle has to release the pointer.
		return true;
	}

#ifdef DO_EXTRA_LOKI_TESTS
	assert( this != m_prev );
	assert( NULL != m_prev );
	assert( m_prev->HasPrevNode( this ) );
	assert( m_next->HasNextNode( this ) );
#endif

	// If a single node is strong, then return false so it won't release.
	if ( HasStrongPointer() )
	{
		// A cyclic chain of pointers is only as strong as the strongest link.
		m_prev->m_next = m_next;
		m_next->m_prev = m_prev;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------------

void TwoRefLinks::ZapAllNodes( void )
{
	TwoRefLinks *p = m_prev;
	if ( ( this == p ) || ( 0 == p ) )
		return;
#ifdef DO_EXTRA_LOKI_TESTS
	assert( AllNodesHaveSamePointer() );
#endif

	while ( p != this )
	{
		TwoRefLinks *p1 = p->m_prev;
		p->m_pointer = 0;
		p->m_next = p;
		p->m_prev = p;
		p = p1;
	}
	m_pointer = 0;
}

// ----------------------------------------------------------------------------

void TwoRefLinks::Swap( TwoRefLinks &rhs )
{

#ifdef DO_EXTRA_LOKI_TESTS
	assert( m_prev->HasPrevNode( this ) );
	assert( m_next->HasNextNode( this ) );
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
	assert( AllNodesHaveSamePointer() );
	assert( rhs.AllNodesHaveSamePointer() );
#endif

	std::swap( rhs.m_pointer, m_pointer );
	if (m_next == this)
	{
#ifdef DO_EXTRA_LOKI_TESTS
		// This is in a cycle by itself.
		assert(m_prev == this);
#endif
		if (rhs.m_next == &rhs)
		{
#ifdef DO_EXTRA_LOKI_TESTS
			assert(rhs.m_prev == &rhs);
#endif
			// both are in 1-node cycles - thus there is nothing to do.
			return;
		}
		m_prev = rhs.m_prev;
		m_next = rhs.m_next;
		m_prev->m_next = m_next->m_prev = this;
		rhs.m_next = rhs.m_prev = &rhs;
		return;
	}
	if (rhs.m_next == &rhs)
	{
#ifdef DO_EXTRA_LOKI_TESTS
		// rhs is in a cycle by itself.
		assert( rhs.m_prev == &rhs );
#endif
		rhs.m_prev = m_prev;
		rhs.m_next = m_next;
		m_prev->m_next = m_next->m_prev = &rhs;
		m_next = m_prev = this;
		return;
	}
	if (m_next == &rhs ) // rhs is next neighbour
	{
		if ( m_prev == &rhs )
			return;  // cycle of 2 pointers - no need to swap.
		std::swap(m_prev, m_next);
		std::swap(rhs.m_prev, rhs.m_next);
		std::swap(rhs.m_prev, m_next);
		std::swap(rhs.m_prev->m_next,m_next->m_prev);
	}
	else if ( m_prev == &rhs ) // rhs is prev neighbor
	{
		if ( m_next == &rhs )
			return;  // cycle of 2 pointers - no need to swap.
		std::swap( m_prev, m_next );
		std::swap( rhs.m_next, rhs.m_prev );
		std::swap( rhs.m_next, m_prev );
		std::swap( rhs.m_next->m_prev, m_prev->m_next );
	}
	else // not neighhbors
	{
		std::swap(m_prev, rhs.m_prev);
		std::swap(m_next, rhs.m_next);
		std::swap(m_prev->m_next, rhs.m_prev->m_next);
		std::swap(m_next->m_prev, rhs.m_next->m_prev);
	}

#ifdef DO_EXTRA_LOKI_TESTS
	assert( m_next == this ? m_prev == this : m_prev != this);
	assert( m_prev == this ? m_next == this : m_next != this);
	assert( m_prev->HasPrevNode( this ) );
	assert( m_next->HasNextNode( this ) );
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
	assert( rhs.m_prev->HasPrevNode( &rhs ) );
	assert( rhs.m_next->HasNextNode( &rhs ) );
	assert( CountPrevCycle( &rhs ) == CountNextCycle( &rhs ) );
	assert( AllNodesHaveSamePointer() );
	assert( rhs.AllNodesHaveSamePointer() );
#endif

}

// ----------------------------------------------------------------------------

bool TwoRefLinks::AllNodesHaveSamePointer( void ) const
{
	const TwoRefLinks *next = m_next;
	if ( NULL == next )
		return true;
	do
	{
		if ( next->m_pointer != m_pointer )
			return false;
		next = next->m_next;
	}
	while ( next != this );
	return true;
}

// ----------------------------------------------------------------------------

unsigned int TwoRefLinks::CountPrevCycle( const TwoRefLinks *pThis )
{
	if ( NULL == pThis )
		return 0;
	const TwoRefLinks *p = pThis->m_prev;
	if ( NULL == p )
		return 0;
	if ( pThis == p )
		return 1;

	unsigned int count = 1;
	do
	{
		p = p->m_prev;
		++count;
	}
	while ( p != pThis );

	return count;
}

// ----------------------------------------------------------------------------

unsigned int TwoRefLinks::CountNextCycle( const TwoRefLinks *pThis )
{
	if ( NULL == pThis )
		return 0;
	const TwoRefLinks *p = pThis->m_next;
	if ( NULL == p )
		return 0;
	if ( pThis == p )
		return 1;

	unsigned int count = 1;
	while ( p != pThis )
	{
		p = p->m_next;
		++count;
	}

	return count;
}

// ----------------------------------------------------------------------------

bool TwoRefLinks::HasPrevNode( const TwoRefLinks *p ) const
{
	if ( this == p )
		return true;
	const TwoRefLinks *prev = m_prev;
	if ( NULL == prev )
		return false;
	while ( prev != this )
	{
		if ( p == prev )
			return true;
		prev = prev->m_prev;
	}
	return false;
}

// ----------------------------------------------------------------------------

bool TwoRefLinks::HasNextNode( const TwoRefLinks *p ) const
{
	if ( this == p )
		return true;
	const TwoRefLinks *next = m_next;
	if ( NULL == next )
		return false;
	while ( next != this )
	{
		if ( p == next )
			return true;
		next = next->m_next;
	}
	return false;
}

// ----------------------------------------------------------------------------

bool TwoRefLinks::HasStrongPointer( void ) const
{
	const TwoRefLinks *next = m_next;
	if ( NULL == next )
		return false;
	while ( next != this )
	{
		if ( next->m_strong )
			return true;
		next = next->m_next;
	}
	return false;
}

// ----------------------------------------------------------------------------

bool TwoRefLinks::Merge( TwoRefLinks &rhs )
{

	if ( NULL == m_next )
	{
#ifdef DO_EXTRA_LOKI_TESTS
		assert( NULL == m_prev );
#endif
		return false;
	}
	TwoRefLinks *prhs = &rhs;
	if ( prhs == this )
		return true;
	if ( NULL == prhs->m_next )
	{
#ifdef DO_EXTRA_LOKI_TESTS
		assert( NULL == prhs->m_prev );
#endif
		return true;
	}

#ifdef DO_EXTRA_LOKI_TESTS
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
	assert( CountPrevCycle( prhs ) == CountNextCycle( prhs ) );
#endif
	// If rhs node is already in this cycle, then no need to merge.
	if ( HasPrevNode( &rhs ) )
	{
#ifdef DO_EXTRA_LOKI_TESTS
		assert( HasNextNode( &rhs ) );
#endif
		return true;
	}

	if ( prhs == prhs->m_next )
	{
		/// rhs is in a cycle with 1 node.
#ifdef DO_EXTRA_LOKI_TESTS
		assert( prhs->m_prev == prhs );
#endif
		prhs->m_prev = m_prev;
		prhs->m_next = this;
		m_prev->m_next = prhs;
		m_prev = prhs;
	}
	else if ( this == m_next )
	{
		/// this is in a cycle with 1 node.
#ifdef DO_EXTRA_LOKI_TESTS
		assert( m_prev == this );
#endif
		m_prev = prhs->m_prev;
		m_next = prhs;
		prhs->m_prev->m_next = this;
		prhs->m_prev = this;
	}
	else
	{
		m_next->m_prev = prhs->m_prev;
		prhs->m_prev->m_next = m_prev;
		m_next = prhs;
		prhs->m_prev = this;
	}


#ifdef DO_EXTRA_LOKI_TESTS
	assert( CountPrevCycle( this ) == CountNextCycle( this ) );
#endif

	return true;
}

// ----------------------------------------------------------------------------

} // end namespace Loki

