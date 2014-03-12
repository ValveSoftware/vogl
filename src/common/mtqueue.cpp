/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
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


#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include "mtqueue.h"

using namespace queues;

MtQueue::MtQueue()
{
    m_pbDataList = NULL;
    m_iHead = 0;
    m_iTail = 0;
    m_cElements = 0;
}

MtQueue::~MtQueue()
{
    this->Purge(); // Should delete whatever is remaining in the queue

    if (m_pbDataList)
    {
        free(m_pbDataList);

        (void)pthread_mutex_destroy(&m_mutex); //  Nothing to look at
    }
}

//
//  Creates up the empty queue, sets the location for empty (m_iHead = m_iTail), creates the
//  locking mutex and returns.
//

MTQ_CODE MtQueue::Initialize(unsigned int nElementCount)
{
    MTQ_CODE mtqCode = MTQ_NONE;
    pthread_mutexattr_t mAttr;
    int ec = 0; // error code for mutex calls

    m_cElements = nElementCount + 1;

    //  Malloc up the list to hold the queue
    m_pbDataList = (QELEM *)malloc(m_cElements * sizeof(QELEM));
    m_iTail = m_iHead = 0;

    if (NULL == m_pbDataList)
    {
        mtqCode = MTQ_MEMERROR;

        goto out;
    }

    //  Create our protective mutex
    // setup recursive mutex for mutex attribute - this is most like, in behavior, to Windows critical sections
    //
    //  PTHREAD_MUTEX_RECURSIVE_NP means that the mutex can be used recursively.
    /*
		The pthread_mutexattr_settype() function shall fail if:
		EINVAL	The value type is invalid.
	*/
    ec = pthread_mutexattr_init(&mAttr);
    if (0 != ec)
    {
        printf("Error initializing mutex attribute:  ec = %d\n", ec);
        mtqCode = MTQ_SYSERROR;
        goto out;
    }

    ec = pthread_mutexattr_settype(&mAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    if (0 != ec)
    {
        printf("Error setting mutex attribute:  ec = %d\n", ec);
        mtqCode = MTQ_SYSERROR;
        goto out;
    }
    // Use the mutex attribute to create the mutex
    /* 
		The pthread_mutex_init() function shall fail if:
		EAGAIN	The system lacked the necessary resources (other than memory) to initialize another mutex.
		ENOMEM	Insufficient memory exists to initialize the mutex.
		EPERM	The caller does not have the privilege to perform the operation.

		The pthread_mutex_init() function may fail if:
		EBUSY	The implementation has detected an attempt to reinitialize the object referenced by mutex, a previously initialized, but not yet destroyed, mutex.
		EINVAL	The value specified by attr is invalid.
	*/
    ec = pthread_mutex_init(&m_mutex, &mAttr);
    if (0 != ec)
    {
        printf("Error creating mutex:  ec = %d\n", ec);
        mtqCode = MTQ_SYSERROR;
        goto out;
    }

out:
    if (MTQ_NONE != mtqCode)
    {
        //  Clean up
        if (m_pbDataList)
        {
            free(m_pbDataList);
            m_pbDataList = NULL;
        }
    }

    return mtqCode;
}

//
//  Insert at m_iHead.
//
MTQ_CODE MtQueue::Enqueue(unsigned int cb, char *pb, unsigned int timeoutMilSec, unsigned int nRetries)
{
    MTQ_CODE mtqCode = MTQ_NONE;
    int ec = 0;
    (void)ec;
    unsigned int cTries = 0;

    char *pbT = NULL;

    if (0 == nRetries)
        nRetries = 1;

    while (cTries < nRetries)
    {
        // acquire mutex
        ec = pthread_mutex_lock(&m_mutex);

        if (!this->IsFull())
        {
            // We have the lock, now to add the element to the list
            break;
        }

        pthread_mutex_unlock(&m_mutex);

        usleep(timeoutMilSec);
        cTries++;
    }

    //  Note:  The only way we have the mutex is if the queue has room for us to enqueue.
    //  Otherwise, we cannot have the mutex.

    if (cTries == nRetries)
    {
        //
        //  The queue never became unfull...
        mtqCode = MTQ_FULL;
        goto out;
    }

    //  We should have the mutex locked at this point
    pbT = (char *)malloc(cb);
    if (NULL == pbT)
    {
        printf("MTQUEUE: unable to enqueue data - unable to alloc memory\n");
        mtqCode = MTQ_MEMERROR;

        goto release;
    }
    memcpy(pbT, pb, cb);

    m_pbDataList[m_iHead].cb = cb;
    m_pbDataList[m_iHead].pb = pbT;

    m_iHead = ((m_iHead + 1) % m_cElements);

release:
    pthread_mutex_unlock(&m_mutex);
out:
    return mtqCode;
}

//
//  Remove at m_iTail
//
MTQ_CODE MtQueue::Dequeue(unsigned int *pcb, char **ppb, unsigned int timeoutMilSec, unsigned int nRetries)
{
    MTQ_CODE mtqCode = MTQ_NONE;
    int ec = 0;
    (void)ec;
    unsigned int cTries = 0;

    if (0 == nRetries)
        nRetries = 1;

    while (cTries < nRetries)
    {
        // acquire mutex
        ec = pthread_mutex_lock(&m_mutex);
        if (!IsEmpty())
        {
            // We have the lock, now to add the element to the list

            break;
        }

        pthread_mutex_unlock(&m_mutex);

        usleep(timeoutMilSec);
        cTries++;
    }

    //  Note:  The only way we have the mutex is if the queue has room for us to enqueue.
    //  Otherwise, we cannot have the mutex.

    if (cTries == nRetries)
    {
        //
        //  The queue never had anything in it...
        mtqCode = MTQ_EMPTY;
        goto out;
    }

    //  We should have the mutex locked at this point
    *pcb = m_pbDataList[m_iTail].cb;
    *ppb = m_pbDataList[m_iTail].pb;

    m_iTail = ((m_iTail + 1) % m_cElements);

    // release:
    pthread_mutex_unlock(&m_mutex);

out:
    return mtqCode;
}

//  go from iHead->iTail and simply release everything.
//  set it back to empty (iHead = iTail)
MTQ_CODE MtQueue::Purge()
{
    MTQ_CODE mtqCode = MTQ_NONE;
    int ec = 0;
    (void)ec;

    // acquire mutex
    ec = pthread_mutex_lock(&m_mutex);
    if (IsEmpty())
    {
        // optimization...the queue is already empty
        goto release;
    }

    do
    {
        free(m_pbDataList[m_iHead].pb);
        m_pbDataList[m_iHead].cb = 0;
        m_pbDataList[m_iHead].pb = NULL;

        m_iHead = ((m_iHead + 1) % m_cElements);

    } while (m_iHead != m_iTail);

    //  Just checking
    if (!IsEmpty())
    {
        printf("MtQueue::Purge:  oops...internal state mucked up...should be an empty queue.\n");
    }

release:
    pthread_mutex_unlock(&m_mutex);

    // out:
    return mtqCode;
}

bool MtQueue::IsFull()
{
    if (((m_iHead + 1) % m_cElements) == m_iTail)
        return true;

    return false;
}

bool MtQueue::IsEmpty()
{
    return (m_iHead == m_iTail);
}
