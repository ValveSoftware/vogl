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
#include "mtqueue.h"

namespace network
{

    //  Error code constants
    typedef enum
    {
        EC_NONE = 0,
        EC_NETWORK,
        EC_TIMEOUT,
        EC_MEMORY,
        EC_SYSTEM,
        EC_NOTALLOWED
    } CHEC;

    class channel
    {
    public:
        channel(void *pbFixedBuffer, size_t cbFixedBuffer);
        channel();

        ~channel();

        CHEC Connect(char *szServer, int port, int nRetries, unsigned int waitMS); // Connecting side
        CHEC Connect(int port, int backlog, bool fLocal);                          // Accepting side

        CHEC Disconnect();

        CHEC ReadMsg(unsigned int *pcbBufOut, char **ppBufOut, int nRetries, int timoutMS);
        CHEC WriteMsg(unsigned int cbBufIn, const char *pbBufIn, int nRetries, int timeoutMS);

    private:
        volatile int m_socket;
        bool m_fServer;
        int m_port;

        char m_szServer[128];
        void *m_pbFixedBuffer;
        size_t m_cbFixedBuffer;

        bool m_fLocal;
        int m_listenSocket;
        int m_backlog;

        void Initialize(void *pbFixedBuffer, size_t cbFixedBuffer);

        CHEC acceptConnection();
        CHEC connectConnection();
        CHEC channelConnect();

        CHEC readMsgLoop(unsigned int *pcbBufOut, char **ppBufOut, int timoutMS);
        CHEC writeMsgLoop(unsigned int cbBufIn, const char *pbBufIn, int timeoutMS);

        CHEC readMsgTimeout(unsigned int cbSize, char *pBuff, int timeoutMS);
        CHEC writeMsgTimeout(unsigned int cbSize, const char *pBuff, int timeoutMS);

        void *my_malloc(size_t cbSize);
        void my_free(void *pBuff);
    };

    //  These function callbacks are called on their own threads.  Implementations of these functions should ensure that any data they
    //  need to access is managed appropriately.
    typedef void (*FnReadCallbackPtr)(void *callbackParam, unsigned int cbData, char *pgData);
    typedef void (*FnRRCallbackPtr)(void *callbackParam, unsigned int cbReq, char *pbReq, unsigned int *pcbResp, char **ppbResp);

    //  Service Masks
    //  OR'ing these masks together say how what kind of connections the channel manager will be managing.
    #define REQRESP   0x01  //  Request Response service
    #define RECVASYNC 0x02  //  Receive Async messages
    #define SENDASYNC 0x04  //  Send Async messages

    class channelmgr
    {
    public:
        channelmgr();
        ~channelmgr();

        CHEC Connect(char *szServer, int port, char serviceMask, FnReadCallbackPtr recvCallbackfn, void *recvCallbackParam);
        CHEC Accept(int port, char serviceMask, bool fLocal, FnReadCallbackPtr readCallbackfn, void *readCallbackParam, FnRRCallbackPtr reqCallbackfn, void *reqCallbackParam);

        //  SendData is thread safe and can be called from any thread.  Messages will queue if the connection is slow or interrupted.
        CHEC SendData(unsigned int cbData, char *pbData, FnReadCallbackPtr respCallbackfn, void *respCallbackParam);
        CHEC SendData(unsigned int cbData, char *pbData);

        bool HasError(int *perrno);

        void Disconnect();

        // Listens for receiving data and calls the recvCallbackfn() with any data it gets
        void DriveRecvLoop(); 
        // Reads from the SendQueue and sends across the socket anything coming its way.
        void DriveSendLoop();

        // Listens for receiving data and calls the reqrespCallbackfn() with any data it gets, and sends
        // back the response syncronously.
        void DriveReqRespRecvLoop();
        // Reads from the ReqRespSendQueue and sends across the socket anything coming its way
        void DriveReqRespSendLoop();

    private:

        int m_basePort;
        int m_errno;
        bool m_fTerminate;
        bool m_fServer;
        bool m_fLocal;
        char m_server[128];

        volatile bool m_fReadyRecvAsync;
        volatile bool m_fReadySendAsync;
        volatile bool m_fReadyReqResp;

        channel *m_pReqRespChannel;
        channel *m_pSendChannel;
        channel *m_pRecvChannel;

        pthread_t m_threadRecv;
        FnReadCallbackPtr m_recvCallbackfn;
        void * m_recvCallbackParam;

        pthread_t m_threadReqRespRecv;
        FnRRCallbackPtr m_reqCallbackfn;
        void * m_reqCallbackParam;


        CHEC SendDataInt(unsigned int cbData, char *pbData);
        pthread_t m_threadSend;
        queues::MtQueue *m_pSendQueue;

        CHEC SendDataRRInt(unsigned int cbData, char *pbData);
        pthread_t m_threadReqRespSend;
        queues::MtQueue *m_pReqRespSendQueue;

    };

} // namespace network


