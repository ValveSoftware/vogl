// Stopwatch.c
// Implements a stopwatch

#include "gltools.h"
#ifdef __linux__
#include <time.h>
#endif


// Reset the stopwatch
// You must call this at least once to set the stopwatch
void gltStopwatchReset(GLTStopwatch *pWatch)
    {
    #ifdef WIN32
    QueryPerformanceCounter(&pWatch->m_LastCount);
    #elif defined __APPLE__
    gettimeofday(&pWatch->last, 0);
    #else
    clock_gettime(CLOCK_MONOTONIC, &pWatch->last);
    #endif
    }


/////////////////////////////////////////////////////////////////////
// Get the number of seconds since the last reset
float gltStopwatchRead(GLTStopwatch *pWatch)
    {
    #ifdef WIN32
    LARGE_INTEGER m_CounterFrequency;
	LARGE_INTEGER lCurrent;

    QueryPerformanceFrequency(&m_CounterFrequency);
	QueryPerformanceCounter(&lCurrent);

	return (float)((lCurrent.QuadPart - pWatch->m_LastCount.QuadPart) /
										(float)(m_CounterFrequency.QuadPart));    
    #elif defined __APPLE__
    struct timeval lcurrent;    // Current timer
    float  fSeconds, fFraction; // Number of seconds & fraction (microseconds)

    // Get current time
    gettimeofday(&lcurrent, 0);
        
    // Subtract the last time from the current time. This is tricky because
    // we have seconds and microseconds. Both must be subtracted and added 
    // together to get the final difference.
    fSeconds = (float)(lcurrent.tv_sec - pWatch->last.tv_sec);
    fFraction = (float)(lcurrent.tv_usec - pWatch->last.tv_usec) * 0.000001f;
    return fSeconds + fFraction;
    #else

    struct timespec lcurrent; 
    clock_gettime( CLOCK_MONOTONIC, &lcurrent );

    return ( lcurrent.tv_sec - pWatch->last.tv_sec ) + lcurrent.tv_nsec * 1e-9f;

    #endif
    }
