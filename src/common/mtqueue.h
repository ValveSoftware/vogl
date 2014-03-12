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

#pragma once

#include <pthread.h>

namespace queues
{

    typedef int MTQ_CODE;

#define MTQ_NONE 0
#define MTQ_FULL 1
#define MTQ_EMPTY 2
#define MTQ_MEMERROR -1
#define MTQ_SYSERROR -2

#define DEFAULT_Q_SIZE 5 // Default size of nElementCount

    //  Internal Structure for holding an element in the queue
    typedef struct _qElem
    {
        unsigned int cb;
        char *pb;
    } QELEM;

    //
    //  MtQueue
    //		Multi-thread safe queue system.  Used to move data across threads asyncronously (though with some
    //		locking as neccessary).
    //
    class MtQueue
    {

    public:
        MtQueue();
        ~MtQueue(); // Consumer needs to ensure this is called safely

        //  Initializes this queue
        //	  nElementCount specifies the maximum number of elements that may be enqueued at any one
        //	  time.  Attempting to Enqueue more when the queue is full may result in an MTQ_FULL error
        //	  returned.
        //	Note:  This is pulled out separately from the constructor as in C++ has no good way of
        //		   failing its constructor in any way other than the allocation of the object returning
        //		   NULL.  In this instance we can fail to malloc, or create the mutex surrounding state
        //		   data.
        //		   Also, this method may only be called once, and must be called before any other methods
        //		   get called (except its destructor).
        MTQ_CODE Initialize(unsigned int nElementCount);

        //
        //  Enqueues a buffer onto the queue
        //    cb - size of the data pointed to by...
        //	  pb - the pointer to the data to be enqueued...  Data is copied if necessary using malloc().
        //	  timeoutMilSec - Amount of time, for each retry, that the data is attempted to enqueue.  This only
        //					  really matters if the queue is full.
        //	  nRetries - the number of times it will attempt to enqueue the data.  The data will only ever be enqueued
        //	  			 once, but if the queue is full, we'll retry enqueing until we succeed or the queue remains
        //				 full the whole time.
        //
        MTQ_CODE Enqueue(unsigned int cb, char *pb, unsigned int timeoutMilSec, unsigned int nRetries);

        //
        //  Dequeues data from the queue
        //	  pcb - contains the size of the data pointed to by...
        //	  ppb - the pointer to a pointer pointing to the actual data.  The caller here is responsible for
        //			cleaning up this memory by using free().
        //	  timeoutMilSec - Amount of time, for each retry, that the data is attempted to dequeue.  This only
        //					  really matters if the queue is empty.
        //	  nRetries - the number of times it will attempt to dequeue the data.  The data will only ever be dequeued
        //	  			 once, but if the queue is empty, we'll retry dequeing until we succeed or the queue remains
        //				 empty the whole time.
        //
        MTQ_CODE Dequeue(unsigned int *pcb, char **ppb, unsigned int timeoutMilSec, unsigned int nRetries);

        MTQ_CODE Purge(); // Empties the remaining elements in the Queue

    private:
        QELEM *m_pbDataList;
        unsigned int m_iHead;
        unsigned int m_iTail;
        unsigned int m_cElements;

        pthread_mutex_t m_mutex;

        bool IsFull();
        bool IsEmpty();
    };

} // namespace queues
