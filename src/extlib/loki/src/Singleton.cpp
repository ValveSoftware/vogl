////////////////////////////////////////////////////////////////////////////////
// The Loki Library
// Copyright (c) 2001 by Andrei Alexandrescu
// This code accompanies the book:
// Alexandrescu, Andrei. "Modern C++ Design: Generic Programming and Design
//     Patterns Applied". Copyright (c) 2001. Addison-Wesley.
// Permission to use, copy, modify, distribute and sell this software for any
//     purpose is hereby granted without fee, provided that the above copyright
//     notice appear in all copies and that both that copyright notice and this
//     permission notice appear in supporting documentation.
// The author or Addison-Wesley Longman make no representations about the
//     suitability of this software for any purpose. It is provided "as is"
//     without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

// $Id: Singleton.cpp 756 2006-10-17 20:05:42Z syntheticpp $


#include <loki/Singleton.h>


#ifdef LOKI_ENABLE_NEW_SETLONGLIVITY_HELPER_DATA_IMPL
Loki::Private::TrackerArray *Loki::Private::pTrackerArray = 0;
#else
Loki::Private::TrackerArray Loki::Private::pTrackerArray = 0;
unsigned int Loki::Private::elements = 0;
#endif

////////////////////////////////////////////////////////////////////////////////
// function AtExitFn
// Ensures proper destruction of objects with longevity
////////////////////////////////////////////////////////////////////////////////

#ifdef LOKI_ENABLE_NEW_SETLONGLIVITY_HELPER_DATA_IMPL

void LOKI_C_CALLING_CONVENTION_QUALIFIER Loki::Private::AtExitFn()
{
	assert(pTrackerArray!=0 && !pTrackerArray->empty());

	// Pick the element at the top of the stack
	LifetimeTracker *pTop = pTrackerArray->back();

	// Remove that object off the stack _before_ deleting pTop
	pTrackerArray->pop_back();

	// Destroy the element
	delete pTop;

	// Destroy stack when it's empty _after_ deleting pTop
	if(pTrackerArray->empty())
	{
		delete pTrackerArray;
		pTrackerArray = 0;
	}
}

#else

void LOKI_C_CALLING_CONVENTION_QUALIFIER Loki::Private::AtExitFn()
{
	assert(elements > 0 && pTrackerArray != 0);
	// Pick the element at the top of the stack
	LifetimeTracker *pTop = pTrackerArray[elements - 1];
	// Remove that object off the stack
	// Don't check errors - realloc with less memory
	//     can't fail
	pTrackerArray = static_cast<TrackerArray>(std::realloc(
	                    pTrackerArray, sizeof(*pTrackerArray) * --elements));
	// Destroy the element
	delete pTop;
}

#endif

