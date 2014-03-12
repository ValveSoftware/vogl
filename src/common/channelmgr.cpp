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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>

#include "channel.h"
#include "mtqueue.h"

#include <pthread.h>

#if defined(_DEBUG) || defined(DEBUG)
#define DEBUG_PRINT(fmt, args...)    fprintf( stderr, fmt, ## args )
#else
#define DEBUG_PRINT(...) /* Don't do anything in release builds */
#endif

void *RecvThreadLoop(void *arg);
void *ReqRespRecvThreadLoop(void *arg);
void *SendThreadLoop(void *arg);
void *ReqRespSendThreadLoop(void *arg);

using namespace network;


channelmgr::channelmgr()
{

    m_threadRecv = (pthread_t) 0;
    m_threadReqRespRecv = (pthread_t) 0;
    m_threadSend = (pthread_t) 0;
    m_threadReqRespSend = (pthread_t) 0;
    m_basePort = 0;
    m_fTerminate = false;
    m_fServer = false;
    m_fLocal = false;
    memset(m_server, '\0', sizeof(m_server));
    m_errno = 0;
    m_recvCallbackfn = NULL;
    m_reqCallbackfn = NULL;
    m_recvCallbackParam = NULL;
    m_reqCallbackParam = NULL;
    m_pReqRespChannel = NULL;
    m_pSendChannel = NULL;
    m_pRecvChannel = NULL;

    m_pSendQueue = NULL;
    m_pReqRespSendQueue = NULL;

}
channelmgr::~channelmgr()
{
    Disconnect();
}

CHEC 
channelmgr::Connect(char *szServer, int port, char serviceMask, FnReadCallbackPtr recvCallbackfn, void *recvCallbackParam)
{
    CHEC ec = EC_NONE;

    //  int portReqResp = port;
    //  int portSend = port+1;  //  Needs to match portRecv in channelmgr::Accept
    //  int portRecv = port+2;  //  Needs to match portSend in channelmgr::Accept

    int err = 0;

    //
    //  Need to connect to the three different ports on the server, if requested

    if (0 == (serviceMask & (REQRESP|RECVASYNC|SENDASYNC)))
    {
        //  No service was requested?

        return EC_NOTALLOWED;
    }

    m_fServer = false;
    m_basePort = port;
    strncpy(m_server, szServer, sizeof(m_server)-1);

    m_fReadyReqResp = true;
    if (serviceMask & REQRESP)
    {
        m_fReadyReqResp = false;
        //  spin up a thread to handle the recv channel
        err = pthread_create(&m_threadReqRespSend, NULL, ReqRespSendThreadLoop, (void *)this);
        if (0 != err)
        {
            m_errno = errno;
            ec = EC_SYSTEM;
            goto out;
        }
    }

    m_fReadySendAsync = true;
    if (serviceMask & SENDASYNC)
    {
        m_fReadySendAsync = false;

        //  spin up a thread to handle the recv channel
        err = pthread_create(&m_threadSend, NULL, SendThreadLoop, (void *)this);
        if (0 != err)
        {
            m_errno = errno;
            ec = EC_SYSTEM;
            goto out;
        }

    }

    m_fReadyRecvAsync = true;
    if (serviceMask & RECVASYNC)
    {    
        m_fReadyRecvAsync = false;
        m_recvCallbackfn = recvCallbackfn;
        m_recvCallbackParam = recvCallbackParam;

        //  spin up a thread to handle the recv channel
        err = pthread_create(&m_threadRecv, NULL, RecvThreadLoop, (void *)this);
        if (0 != err)
        {
            m_errno = errno;
            ec = EC_SYSTEM;
            goto out;
        }
    }

    //  Wait for threads to spin up and signal that they're ready.
    //
    //  Just loop for it.
    while ((false == m_fReadyRecvAsync) || (false == m_fReadySendAsync) || (false == m_fReadyReqResp));


out:
    if (EC_NONE != ec)
    {
        // Cleanup
        Disconnect();
    }

    return ec;
}

CHEC 
channelmgr::Accept(int port, char serviceMask, bool fLocal, FnReadCallbackPtr readcallbackfn, void *readCallbackParam, FnRRCallbackPtr reqCallbackfn, void *reqCallbackParam)
{
    CHEC ec = EC_NONE;

    //  int portReqResp = port;
    //  int portSend = port+2;  //  Needs to match portRecv in channelmgr::Connect
    //  int portRecv = port+1;  //  Needs to match portSend in channelmgr::Connect

    int err = 0;

    if (0 == (serviceMask & (REQRESP|RECVASYNC|SENDASYNC)))
    {
        //  No service was requested?

        return EC_NOTALLOWED;
    }

    m_fServer = true;
    m_basePort = port;
    m_fLocal = fLocal;

    m_fReadyReqResp = true;
    if (serviceMask & REQRESP)
    {
        //  spin up a thread to handle the reqresp channel
        m_fReadyReqResp = false;

        m_reqCallbackfn = reqCallbackfn;
        m_reqCallbackParam = reqCallbackParam;

        err = pthread_create(&m_threadReqRespRecv, NULL, ReqRespRecvThreadLoop, (void *)this);
        if (0 != err)
        {
            ec = EC_SYSTEM;
            m_errno = errno;
            goto out;
        }
    }

    m_fReadyRecvAsync = true;
    if (serviceMask & RECVASYNC)
    {
        m_fReadyRecvAsync = false;
        //  spin up a thread to handle the recv channel
        m_recvCallbackfn = readcallbackfn;
        m_recvCallbackParam = readCallbackParam;

        err = pthread_create(&m_threadRecv, NULL, RecvThreadLoop, (void *)this);
        if (0 != err)
        {
            ec = EC_SYSTEM;
            m_errno = errno;
            goto out;
        }
    }

    m_fReadySendAsync = true;
    if (serviceMask & SENDASYNC)
    {
        m_fReadySendAsync = false;

        //  spin up a thread to handle the recv channel
        err = pthread_create(&m_threadSend, NULL, SendThreadLoop, (void *)this);
        if (0 != err)
        {
            m_errno = errno;
            ec = EC_SYSTEM;
            goto out;
        }
    }

    //  Wait for threads to spin up and signal that they're ready.
    //
    //  Just loop for it.
    while ((false == m_fReadyRecvAsync) || (false == m_fReadySendAsync) || (false == m_fReadyReqResp));

out:
    if (EC_NONE != ec)
    {
        // Cleanup
        Disconnect();
    }

    return ec;
}

CHEC 
channelmgr::SendData(unsigned int cbData, char *pbData, FnReadCallbackPtr respCallbackfn, void *respCallbackParam)
{
    CHEC ec = EC_NONE;
    queues::MTQ_CODE mtqec = MTQ_NONE;

    //  Don't allow this call if we accepted connections (i.e. this is the "server" side of things)
    if (true == m_fServer)
        return EC_NOTALLOWED;

    mtqec = m_pReqRespSendQueue->Enqueue(cbData, pbData, 500, 2);
    if (MTQ_NONE != mtqec)
    {
        if (MTQ_FULL == mtqec)
        {
            DEBUG_PRINT("%s:%d %s Unable to send Request (QFull) (error = %x, %d)\n", __FILE__, __LINE__, __func__, mtqec, errno);
            ec = EC_NETWORK;
        }
        else
        {
            DEBUG_PRINT("%s:%d %s Unable to send Request (error = %x, %d)\n", __FILE__, __LINE__, __func__, mtqec, errno);
        }
        goto out;
    }

    ec = m_pReqRespChannel->ReadMsg(&cbData, &pbData, 2, 0);
    if (EC_NONE != ec)
    {
        DEBUG_PRINT("%s:%d %s Unable to read response (error = %x)\n", __FILE__, __LINE__, __func__, ec);
        goto out;
    }   

    (*respCallbackfn)(respCallbackParam, cbData, pbData);

    if (NULL != pbData)
        free(pbData);

out:
    return ec;
}

CHEC 
channelmgr::SendData(unsigned int cbData, char *pbData)
{
    CHEC ec = EC_NONE;
    queues::MTQ_CODE mtqec = MTQ_NONE;

    mtqec = m_pSendQueue->Enqueue(cbData, pbData, 500, 2);
    if (MTQ_NONE != mtqec)
    {
        if (MTQ_FULL == mtqec)
        {
            DEBUG_PRINT("%s:%d %s Unable to send (QFull) (error = %x, %d)\n", __FILE__, __LINE__, __func__, mtqec, errno);
            ec = EC_NETWORK;
        }
        else
        {
            DEBUG_PRINT("%s:%d %s Unable to send (error = %x, %d)\n", __FILE__, __LINE__, __func__, mtqec, errno);
        }
        goto out;
    }

out:
    return ec;
}

bool 
channelmgr::HasError(int *perrno)
{
    if (NULL != perrno)
    {
        *perrno = m_errno;
    }
    
    return (0 != m_errno);
}

void 
channelmgr::Disconnect()
{

    m_fTerminate = true;

    //  Wait for the recvthread to notice and clean itself up
    if (m_threadRecv)
        pthread_join(m_threadRecv, NULL);

    if (m_threadSend)
        pthread_join(m_threadSend, NULL);

    if (m_threadReqRespRecv)
        pthread_join(m_threadReqRespRecv, NULL);

    if (m_threadReqRespSend)
        pthread_join(m_threadReqRespSend, NULL);

    //  Clean up all remaining channels
    if (NULL != m_pReqRespChannel)
    {
        m_pReqRespChannel->Disconnect();
        delete m_pReqRespChannel;
        m_pReqRespChannel = NULL;
    }

    if (NULL != m_pSendChannel)
    {
        m_pSendChannel->Disconnect();
        delete m_pSendChannel;
        m_pSendChannel = NULL;
    }

    if (NULL != m_pRecvChannel)
    {
        m_pRecvChannel->Disconnect();
        delete m_pRecvChannel;
        m_pRecvChannel = NULL;
    }

    if (m_pSendQueue)
    {
        delete m_pSendQueue;
        m_pSendQueue = NULL;
    }

    if (m_pReqRespSendQueue)
    {
        delete m_pReqRespSendQueue;
        m_pReqRespSendQueue = NULL;
    }

    m_threadRecv = (pthread_t) 0;
    m_threadReqRespRecv= (pthread_t) 0;
    m_basePort = 0;
    m_fTerminate = false;
    m_fServer = false;
    m_fLocal = false;
    memset(m_server, '\0', sizeof(m_server));
    m_errno = 0;
    m_recvCallbackfn = NULL;
    m_reqCallbackfn = NULL;
    m_recvCallbackParam = NULL;
    m_reqCallbackParam = NULL;

    return;
}


//
//  Handle the recv thread
//

void *RecvThreadLoop(void *arg)
{
    channelmgr *pChannelMgr = (channelmgr *) arg;

    pChannelMgr->DriveRecvLoop();

    return NULL;
}

void 
channelmgr::DriveRecvLoop()
{
    CHEC ec = EC_NONE;
    int err = 0;
    unsigned int cbData = 0;
    char *pbData = NULL;
    pthread_t thisThread = pthread_self();
    int portRecv;

    //  Set this thread's name:
    err = pthread_setname_np(thisThread, "VOGLRecvThd");
    if (0 != err)
    {
        DEBUG_PRINT("%s:%d %s Unable to set the name of the thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
    }

    m_pRecvChannel = new channel();
    if (NULL == m_pRecvChannel)
    {
        //  Unable to allocate a new channel
        ec = EC_MEMORY;
        DEBUG_PRINT("%s:%d %s Unable to allocate new channel for Recv - OOM?\n", __FILE__, __LINE__, __func__);
        goto out;
    }

    if (m_fServer)
    {
        portRecv = m_basePort+1;  //  Needs to match portSend in channelmgr::Connect
        ec = m_pRecvChannel->Connect(portRecv, 0, m_fLocal);
        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Failed to accept on port %d for Recv. returned %d, errno %d\n", __FILE__, __LINE__, __func__, portRecv, ec, errno);
            goto out;
        }
    }
    else
    {
        portRecv = m_basePort+2;  //  Needs to match portSend in channelmgr::Accept 
        ec = m_pRecvChannel->Connect(m_server, portRecv, 100, 500); // loop 100 times, waiting 500MS between attempts
        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Failed to connect to %s on port %d for Recv. returned %d, errno %d\n", __FILE__, __LINE__, __func__, m_server, portRecv, ec, errno);
            goto out;
        }
    }

    m_fReadyRecvAsync = true;

    while (!m_fTerminate)
    {

        ec = m_pRecvChannel->ReadMsg(&cbData, &pbData, 5, 100); // 500
        //if (EC_TIMEOUT == ec)
        //{
            //  Timeouts are not really errors
        //    continue;
        //}

        if (EC_NONE != ec)
        {
            //  Keep going on the server side.  We'll accept new connections as they come along.
            if (m_fServer)
            {
                m_pRecvChannel->Disconnect();
                continue;
            }

            //  On the client side, we need to just keep going.
            //DEBUG_PRINT("%s:%d %s Error reading message (error = %x, %d)\n", __FILE__, __LINE__, __func__, ec, errno);
            continue;
        }

        (*m_recvCallbackfn)(m_recvCallbackParam, cbData, pbData);

        if (NULL != pbData)
            free(pbData);

        cbData = 0;
        pbData = NULL;
    }
out:
    return;
}


//
//  Handle the Async Send thread
//

void *SendThreadLoop(void *arg)
{
    channelmgr *pChannelMgr = (channelmgr *) arg;

    pChannelMgr->DriveSendLoop();

    return NULL;
}

void
channelmgr::DriveSendLoop()
{
    CHEC ec = EC_NONE;
    queues::MTQ_CODE mtqec = MTQ_NONE;
    int err = 0;
    bool fmessageSent;
    unsigned int message_size = 0;
    char *message = NULL;
    pthread_t thisThread = pthread_self();
    int portSend;

    //  Set this thread's name:
    err = pthread_setname_np(thisThread, "VOGLSendThd");
    if (0 != err)
    {
        DEBUG_PRINT("%s:%d %s Unable to set the name of the thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
    }

    //
    //  Generate the sendQ to hold the messages.
    m_pSendQueue = new queues::MtQueue();
    if (NULL == m_pSendQueue)
    {
        //  Unable to allocate a new send queue
        ec = EC_MEMORY;
        goto out;
    }

    mtqec = m_pSendQueue->Initialize(1000); // Not sure how large this should be.  This is a guess.
    if (MTQ_NONE != mtqec)
    {
        ec = EC_MEMORY;
        goto out;
    }

    m_pSendChannel = new channel();
    if (NULL == m_pSendChannel)
    {
        //  Unable to allocate a new channel
        ec = EC_MEMORY;
        goto out;
    }

    if (m_fServer)
    {
        portSend = m_basePort+2;  //  Needs to match portRecv in channelmgr::Connect
        ec = m_pSendChannel->Connect(portSend, 0, m_fLocal);
        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Failed to accept on port %d for Send. returned %d, errno %d\n", __FILE__, __LINE__, __func__, portSend, ec, errno);
            goto out;
        }
    }
    else
    {
        portSend = m_basePort+1;  //  Needs to match portRecv in channelmgr::Accept
        ec = m_pSendChannel->Connect(m_server, portSend, 100, 500); // loop 100 times, waiting 500MS between attempts
        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Failed to connect to %s on port %d for Send. returned %d, errno %d\n", __FILE__, __LINE__, __func__, m_server, portSend, ec, errno);
            goto out;
        }
    }

    m_fReadySendAsync = true;
    fmessageSent = true;
    while (!m_fTerminate)
    {
        if (fmessageSent && message)
        {
            free(message);
            message = NULL;
            message_size = 0;
        }

        if (fmessageSent)
        {
            //  if we sent the last message dequeued, get a new one to send.
            mtqec = m_pSendQueue->Dequeue(&message_size, &message, 20, 5);
            if (MTQ_NONE != mtqec)
            {
                //  Just keep going round until you get a message to send.
                continue;
            }
            DEBUG_PRINT("%s:%d %s DQ'd message to send (size = %d)\n", __FILE__, __LINE__, __func__, message_size);
            fmessageSent = false; // we have a new message to send
        }
        ec = this->SendDataInt(message_size, message);
        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Unable to send message (error = %x)\n", __FILE__, __LINE__, __func__, ec);
            continue;
        }
        DEBUG_PRINT("%s:%d %s sent message (size = %d)\n", __FILE__, __LINE__, __func__, message_size);
        fmessageSent = true; // the message was sent successfully
    }

    if (message)
        free(message);


out:
    return;
}


CHEC
channelmgr::SendDataInt(unsigned int cbData, char *pbData)
{
    CHEC ec = EC_NONE;

    ec = m_pSendChannel->WriteMsg(cbData, pbData, 2, 500);
    if (EC_NONE != ec)
    {
        DEBUG_PRINT("%s:%d %s Unable to send (error = %x, %d)\n", __FILE__, __LINE__, __func__, ec, errno);
        goto out;
    }

    DEBUG_PRINT("%s:%d %s sent(internal) message (size = %d)\n", __FILE__, __LINE__, __func__, cbData);

out:
    return ec;
}

//
//  Handle the reqresp thread
//

void *ReqRespRecvThreadLoop(void *arg)
{
    channelmgr *pChannelMgr = (channelmgr *) arg;

    pChannelMgr->DriveReqRespRecvLoop();

    return NULL;
}

void 
channelmgr::DriveReqRespRecvLoop()
{
    CHEC ec = EC_NONE;
    int err = 0;
    unsigned int cbReq = 0, cbResp = 0;
    char *pbReq = NULL, *pbResp = NULL;
    pthread_t thisThread = pthread_self();
    int portReqResp = m_basePort;


    //  Set this thread's name:
    err = pthread_setname_np(thisThread, "VOGLRRThd");
    if (0 != err)
    {
        DEBUG_PRINT("%s:%d %s Unable to set the name of the thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
    }

    m_pReqRespChannel = new channel();
    if (NULL == m_pReqRespChannel)
    {
        //  Unable to allocate a new channel
        ec = EC_MEMORY;
        goto out;
    }

    //  This thread should only be running when m_fServer is true.
    ec = m_pReqRespChannel->Connect(portReqResp, 5, m_fLocal);
    if (EC_NONE != ec)
    {
        DEBUG_PRINT("%s:%d %s Failed to accept on port %d for ReqResp. returned %d, errno %d\n", __FILE__, __LINE__, __func__, portReqResp, ec, errno);
        goto out;
    }

    m_fReadyReqResp = true;

    while (!m_fTerminate)
    {
        if (NULL != pbReq)
            free(pbReq);
        cbReq = 0;
        pbReq = NULL;

        if (NULL != pbResp)
            free(pbResp);
        cbResp = 0;
        pbResp = NULL;

        ec = m_pReqRespChannel->ReadMsg(&cbReq, &pbReq, 5, 100); //100
        if (EC_TIMEOUT == ec)
        {
            //  Timeouts are not really errors
            continue;
        }

        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Error reading request (error = %x)\n", __FILE__, __LINE__, __func__, ec);
            continue;
        }

        (*m_reqCallbackfn)(m_reqCallbackParam, cbReq, pbReq, &cbResp, &pbResp);

        ec = m_pReqRespChannel->WriteMsg(cbResp, pbResp, 5, 0); //100
        if (EC_TIMEOUT == ec)
        {
            //  Timeouts are not really errors
            continue;
        }

        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Unable to write response (error = %x)\n", __FILE__, __LINE__, __func__, ec);
            continue;
        }        

    }
    if (NULL != pbReq)
        free(pbReq);
    if (NULL != pbResp)
        free(pbResp);

out:
    return;
}

//
//  Handle the Async Send thread
//

void *ReqRespSendThreadLoop(void *arg)
{
    channelmgr *pChannelMgr = (channelmgr *) arg;

    pChannelMgr->DriveReqRespSendLoop();

    return NULL;
}

void
channelmgr::DriveReqRespSendLoop()
{
    CHEC ec = EC_NONE;
    queues::MTQ_CODE mtqec = MTQ_NONE;
    int err = 0;
    unsigned int message_size = 0;
    char *message = NULL;
    pthread_t thisThread = pthread_self();
    int portReqResp = m_basePort;

    //  Set this thread's name:
    err = pthread_setname_np(thisThread, "VOGLRRSndThd");
    if (0 != err)
    {
        DEBUG_PRINT("%s:%d %s Unable to set the name of the thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
    }

    //
    //  Generate the sendQ to hold the messages.
    m_pReqRespSendQueue = new queues::MtQueue();
    if (NULL == m_pReqRespSendQueue)
    {
        //  Unable to allocate a new send queue
        ec = EC_MEMORY;
        goto out;
    }

    mtqec = m_pReqRespSendQueue->Initialize(1000); // Not sure how large this should be.  This is a guess.
    if (MTQ_NONE != mtqec)
    {
        ec = EC_MEMORY;
        goto out;
    }

    m_pReqRespChannel = new channel();
    if (NULL == m_pReqRespChannel)
    {
        //  Unable to allocate a new channel
        ec = EC_MEMORY;
        goto out;
    }

    ec = m_pReqRespChannel->Connect(m_server, portReqResp, 100, 500); // loop 100 times, waiting 500MS between attempts
    if (EC_NONE != ec)
    {
        goto out;
    }
    m_fReadyReqResp = true;

    while (!m_fTerminate)
    {
        if (message)
            free(message);
        message = NULL;
        message_size = 0;

        mtqec = m_pReqRespSendQueue->Dequeue(&message_size, &message, 20, 5);
        if (MTQ_NONE != mtqec)
        {
            //  Just keep going round until you get a message to send.
            continue;
        }

        ec = this->SendDataRRInt(message_size, message);
        if (EC_NONE != ec)
        {
            DEBUG_PRINT("%s:%d %s Unable to send message (error = %x)\n", __FILE__, __LINE__, __func__, ec);
            continue;
        }
    }

    if (message)
        free(message);

out:
    return;
}


CHEC
channelmgr::SendDataRRInt(unsigned int cbData, char *pbData)
{
    CHEC ec = EC_NONE;

    //  Don't allow this call if we accepted connections (i.e. this is the "server" side of things)
    if (true == m_fServer)
        return EC_NOTALLOWED;

    ec = m_pReqRespChannel->WriteMsg(cbData, pbData, 2, 0);
    if (EC_NONE != ec)
    {
        DEBUG_PRINT("%s:%d %s Unable to send request (error = %x, %d)\n", __FILE__, __LINE__, __func__, ec, errno);
        goto out;
    }

out:
    return ec;
}

