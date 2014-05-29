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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>

#include "vogl_core.h"
#include "channel.h"

#if defined(PLATFORM_POSIX)
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
    #include <unistd.h>
#else
    #include <Winsock2.h>

    #define SHUT_RDWR SD_BOTH
    #define close closesocket

    void usleep(DWORD usec) { Sleep(usec / 1000); }
#endif


int hostname_to_ip(char *hostname, char *ip);
void print_time(const char *szDesc, struct timespec *ptspec);

#if defined(_DEBUG) || defined(DEBUG)
    #if defined(COMPILER_GCCLIKE)
        #define DEBUG_PRINT(fmt, args...)    fprintf( stderr, fmt, ## args )
    #else
        #define DEBUG_PRINT(fmt, ...)       fprintf( stderr, fmt, __VA_ARGS__ )
    #endif
#else
    #define DEBUG_PRINT(...) /* Don't do anything in release builds */
#endif

using namespace network;


/**********************************************************/
//
//  Class Channel implementation
//
//
channel::channel(void *pbFixedBuffer, size_t cbFixedBuffer)
{
    Initialize(pbFixedBuffer, cbFixedBuffer);
}

channel::channel()
{
    Initialize(NULL, 0);
}

void
channel::Initialize(void *pbFixedBuffer, size_t cbFixedBuffer)
{
    //  Common
    m_socket = 0;
    m_fServer = false;
    m_port = 0;
    //  fixed buffer to use for packets
    m_cbFixedBuffer = cbFixedBuffer;
    m_pbFixedBuffer = pbFixedBuffer;

    //  Client specific
    memset(m_szServer, 0, sizeof(m_szServer));

    //  Server specific
    m_fLocal = false;
    m_listenSocket = 0;
    m_backlog = 0;
}

channel::~channel()
{
    if (m_listenSocket)
    {
        shutdown(m_listenSocket, SHUT_RDWR);
        close(m_listenSocket);
    }

    if (m_socket)
    {
        shutdown(m_socket, SHUT_RDWR);
        close(m_socket);
    }
}

CHEC
channel::Connect(char *szServer, int port, int nRetries, unsigned int waitMS)
{
    CHEC ec = EC_NONE;

    m_fServer = false;
    m_port = port;
    //  Copy szServer
    if (szServer)
    {
        strncpy(m_szServer, szServer, sizeof(m_szServer));
    }

    for (int iTry = 0; iTry < nRetries; iTry++)
    {
        ec = channelConnect();
        if (EC_TIMEOUT != ec)
        {
            break;
        }

        usleep(waitMS * 1000); // 1000 microseconds = 1 milisecond
    }

    return ec;
}

CHEC
channel::Connect(int port, int backlog, bool fLocal)
{
    CHEC ec = EC_NONE;

    m_fServer = true;
    m_port = port;
    m_backlog = backlog;
    m_fLocal = fLocal;

    ec = channelConnect();

    return ec;
}

CHEC
channel::channelConnect()
{
    CHEC ec = EC_NONE;

    if (false == m_fServer)
    {
        ec = connectConnection();
    }
    else
    {
        ec = acceptConnection();
    }

    return ec;
}

CHEC
channel::acceptConnection()
{
    CHEC ec = EC_NONE;
    int rv = 0;
    int noNagle = 1; //  Turn off nagle
    int reuseaddress = 1;  // Do not reuse addresses
    struct sockaddr_in serv_addr;
#ifdef NOTYET
    bool fFirstTime = false;
#endif

    if (0 == m_listenSocket)
    {

        m_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == m_listenSocket)
        {
            m_listenSocket = 0;
            DEBUG_PRINT("%s %d: %s socket() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            goto out;
        }

        // setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &noNagle, sizeof(noNagle));
        rv = setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (void *)&reuseaddress, sizeof(reuseaddress));
        if (-1 == rv)
        {
            close(m_listenSocket);
            m_listenSocket = 0;
            DEBUG_PRINT("%s %d: %s setsockopt() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            goto out;
        }

        rv = setsockopt(m_listenSocket, IPPROTO_TCP, TCP_NODELAY, (void *)&noNagle, sizeof(noNagle));
        if (-1 == rv)
        {
            close(m_listenSocket);
            m_listenSocket = 0;
            DEBUG_PRINT("%s %d: %s setsockopt() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            goto out;
        }

        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        if (true == m_fLocal)
        {
            serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        }
        else
        {
            serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        serv_addr.sin_port = htons(m_port);

        rv = bind(m_listenSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (-1 == rv)
        {
            close(m_listenSocket);
            m_listenSocket = 0;
            DEBUG_PRINT("%s %d: %s bind() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            goto out;
        }

        rv = listen(m_listenSocket, m_backlog);
        if (-1 == rv)
        {
            close(m_listenSocket);
            m_listenSocket = 0;
            DEBUG_PRINT("%s %d: %s listen() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            goto out;
        }

#ifdef NOTYET
        fFirstTime = true;
#endif
    }

    if (0 == m_socket)
    {
#ifdef NOTYET
        int flags = 0;
        int ret = 0;

        if (true != fFirstTime)
        {
            //  Non-blocking see if there's any connections awaiting us
            flags = fcntl(m_listenSocket, F_GETFL, 0);
            if (-1 == flags) flags = 0;
            flags |= O_NONBLOCK;
            ret = fcntl(m_listenSocket, F_SETFL, flags);
        }
#endif // NOTYET
        DEBUG_PRINT("%s %d: %s accept() attempt on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, m_port);
        m_socket = accept(m_listenSocket, (struct sockaddr *)NULL, NULL);

        if (-1 == m_socket)
        {
            m_socket = 0;

            DEBUG_PRINT("%s %d: %s accept() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            //if (EAGAIN == errno || EWOULDBLOCK == errno)
            //    ec = EC_TIMEOUT;

            goto out;
        }

        // setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &noNagle, sizeof(noNagle));
        rv = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&reuseaddress, sizeof(reuseaddress));
        if (-1 == rv)
        {
            close(m_socket);
            m_socket = 0;
            DEBUG_PRINT("%s %d: %s setsockopt() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            goto out;
        }

        noNagle = 1;
        rv = setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (void *)&noNagle, sizeof(noNagle));
        if (-1 == rv)
        {
            close(m_socket);
            m_socket = 0;
            DEBUG_PRINT("%s %d: %s setsockopt() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
            ec = EC_NETWORK;
            goto out;
        }

        DEBUG_PRINT("%s %d: %s accept() succeeded to socket %d on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, m_socket, m_port);
    }

out:
    return ec;
}

CHEC
channel::connectConnection()
{
    CHEC ec = EC_NONE;
    int rv = 0;
    int noNagle = 1; //  Turn off nagle
    int reuseaddress = 1;  // Do not reuse addresses
    struct sockaddr_in serv_addr;
    char ip[1024] = { 0 };

    m_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (m_socket < 0)
    {
        DEBUG_PRINT("\n %s %d: %s  Error : Could not create socket \n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR);
        m_socket = 0;

        ec = EC_NETWORK;
        goto out;
    }

    // setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &noNagle, sizeof(noNagle));
    rv = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&reuseaddress, sizeof(reuseaddress));
    if (-1 == rv)
    {
        close(m_socket);
        m_socket = 0;
        DEBUG_PRINT("%s %d: %s setsockopt() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
        ec = EC_NETWORK;
        goto out;
    }

    rv = setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (void *)&noNagle, sizeof(noNagle));
    if (-1 == rv)
    {
        close(m_socket);
        m_socket = 0;
        DEBUG_PRINT("%s %d: %s setsockopt() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
        ec = EC_NETWORK;
        goto out;
    }


    hostname_to_ip(m_szServer, ip);

    DEBUG_PRINT("%s has the ip of: %s\n", m_szServer, ip);

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(m_port);

    rv = inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    if (-1 == rv)
    {
        DEBUG_PRINT("%s %d: %s inet_pton() failed with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno);
        ec = EC_NETWORK;
        Disconnect();
        goto out;
    }
    if (0 == rv)
    {
        DEBUG_PRINT("%s %d: %s inet_pton() \"%s\" does not contain a character string representing a valid network address in the AF_INET address family.", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, ip);
        ec = EC_NETWORK;
        Disconnect();
        goto out;
    }

    rv = connect(m_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (-1 == rv)
    {
        int tErrno = errno;
        ec = EC_NETWORK;
        DEBUG_PRINT("%s %d: %s connect() failed on port %d with %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, m_port, tErrno);
        if (ECONNREFUSED == tErrno)
        {
            ec = EC_TIMEOUT;
        }
        Disconnect();
        goto out;
    }

    DEBUG_PRINT("%s %d: %s connect() to socket %d on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, m_socket, m_port);

out:
    return ec;
}

CHEC
channel::Disconnect()
{
    if (0 != m_socket)
    {
        DEBUG_PRINT("%s %d: %s closing down socket %d on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, m_socket, m_port);
        shutdown(m_socket, SHUT_RDWR);
        close(m_socket);
        m_socket = 0;
    }

    return EC_NONE;
}



//  Public
//  ReadMsg
//
//  Returns the size and data of a message read from the network socket.  Lifetime of the data returned is owned by the
//  caller and should be cleaned up by using free().
//
//  This method is the one that loops through the retries.
//
CHEC
channel::ReadMsg(unsigned int *pcbBufOut, char **ppBufOut, int nRetries, int timeoutMS)
{
    CHEC ec = EC_NONE;

    for (int iTry = 0; iTry < nRetries; iTry++)
    {
        ec = readMsgLoop(pcbBufOut, ppBufOut, timeoutMS);
        if (EC_TIMEOUT == ec)
        {
            continue;
        }
        break;
    }

    //if (EC_TIMEOUT == ec)
        //Disconnect();

    return ec;
}

//  Private
//  readMsgLoop
//
//  The inner loop of the ReadMsg() that deals with reconnection and message size.
//
CHEC
channel::readMsgLoop(unsigned int *pcbBufOut, char **ppBufOut, int timeoutMS)
{
    CHEC ec = EC_NONE;
    char *pRecBuff = NULL;
    //int bToRead = 1024;
    //int bRead = 0;
    int64_t cbSize64 = 0;

    if (0 == m_socket)
    {
        ec = channelConnect();
        if (EC_NONE != ec)
            goto out;
    }

    //  A buffer is defined as having a size (int64_t) followed by the data of that size.
    //
    //  First, read the size of the buffer:

    ec = readMsgTimeout(sizeof(int64_t), (char *)&cbSize64, timeoutMS);
    if (EC_NONE != ec)
    {
        goto out;
    }

    //  malloc up some space
    pRecBuff = (char *)my_malloc((size_t)cbSize64);
    if (NULL == pRecBuff)
    {
        ec = EC_MEMORY;
        goto out;
    }

    //  Now read the data buffer
    ec = readMsgTimeout(cbSize64, pRecBuff, timeoutMS);
    if (EC_NONE != ec)
    {
        my_free(pRecBuff);
        DEBUG_PRINT("%s %d: %s  Error from read(data) = %d (%d).", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, ec, errno);

        goto out;
    }

    DEBUG_PRINT("Debug: client buffer read %" PRIdPTR " bytes\n", cbSize64);

    *ppBufOut = pRecBuff;
    *pcbBufOut = cbSize64; //  MIN(bToRead, bRead)?

out:

    if (EC_NONE != ec && EC_TIMEOUT != ec)
    {
        DEBUG_PRINT("  Disconnecting.\n");
        Disconnect();
        //  Try to reconnect right away.
        ec = channelConnect();
        if (EC_NONE == ec)
            ec = EC_TIMEOUT;
    }

    return ec;
}

//  Private
//  readMsgTimeout
//
//  The inner read of the readMsgLoop().  This one deals with timeouts explicitly.
//
CHEC
channel::readMsgTimeout(unsigned int cbSize, char *pBuff, int timeoutMS)
{
    CHEC ec = EC_NONE;
    double timeElapsedMS = 0;
    unsigned int bRead = 0; // How many bytes read so far
    char *pRecBuff = pBuff;
    struct timespec tspecBefore, tspecNow;

    // baseline time
    clock_gettime(CLOCK_MONOTONIC, &tspecBefore);

    //DEBUG_PRINT("channel::InternalRead:  Timeout specified as %dms.\n", timeoutMS);
    while (cbSize > bRead)
    {
        int bReadT = 0;

        //bReadT = read(m_socket, pRecBuff+bRead, cbSize - bRead );
        bReadT = recv(m_socket, pRecBuff + bRead, cbSize - bRead, MSG_NOSIGNAL | (m_fServer ? 0: MSG_DONTWAIT));

        if (bReadT <= 0)
        {
            //  Check to see if the error is actually just a timeout
            //
            if (EWOULDBLOCK != errno && EAGAIN != errno)
            {
                ec = EC_NETWORK;
                DEBUG_PRINT("%s %d: %s Error from read() on port %d = %d. \n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, m_port, errno);
                goto out;
            }

            //  See if we've gone past the timeout requested
            clock_gettime(CLOCK_MONOTONIC, &tspecNow);

            timeElapsedMS = ((double)(tspecNow.tv_sec - tspecBefore.tv_sec) * 1000) + ((double)(tspecNow.tv_nsec - tspecBefore.tv_nsec) / 1.0e6);

            if (timeoutMS && (timeoutMS <= timeElapsedMS))
            {
                //ec = EC_NETWORK;
                ec = EC_TIMEOUT;
                //DEBUG_PRINT("channel::InternalRead: Timing out %f\n", timeElapsedMS);
                break;
            }

            //  Still time left
            //DEBUG_PRINT("channel::InternalRead: Still time left %f\n", timeElapsedMS);
            continue;
        }

        bRead += (unsigned int) bReadT; // Add the bytes we've read up to this point.
    }

out:
    if (EC_NONE == ec)
        DEBUG_PRINT("%s %d: %s recv'd(%d bytes) on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, bRead, m_port);

    return ec;
}

//
//  writeMsg -
//
//    Writes a buffer out to a socket.  In order to do so, it needs to prepend the buffer with the
//	size in order for the readMsg to know how much data to get.
//

CHEC
channel::WriteMsg(unsigned int cbBufIn, const char *pbBufIn, int nRetries, int timeoutMS)
{
    CHEC ec = EC_NONE;

    for (int iTry = 0; iTry < nRetries; iTry++)
    {
        ec = writeMsgLoop(cbBufIn, pbBufIn, timeoutMS);
        if (EC_TIMEOUT == ec)
        {
            continue;
        }
        break;
    }

    //if (EC_TIMEOUT == ec)
        //Disconnect();

    return ec;
}

CHEC
channel::writeMsgLoop(unsigned int cbBufIn, const char *pbBufIn, int timeoutMS)
{
    CHEC ec = EC_NONE;

    uint64_t cbToWrite = cbBufIn; // Number of bytes to write if this write is to succeed

    if (0 == m_socket)
    {
        ec = channelConnect();
        if (EC_NONE != ec)
            goto out;
    }

    ec = writeMsgTimeout(sizeof(uint64_t), (char *)&cbToWrite, timeoutMS);
    if (EC_NONE != ec)
    {
        DEBUG_PRINT("%s %d: %s  Error from write(buffersize) = %d (%d) on port %d.", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, ec, errno, m_port);
        goto out;
    }

    ec = writeMsgTimeout(cbBufIn, pbBufIn, timeoutMS);
    if (EC_NONE != ec)
    {
        DEBUG_PRINT("%s %d: %s  Error from write(data) = %d (%d) on port %d.", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, ec, errno, m_port);
        goto out;
    }

    DEBUG_PRINT("Debug: client buffer wrote %" PRIuPTR " bytes on port %d\n", cbToWrite, m_port);

out:
    if (EC_NONE != ec && EC_TIMEOUT != ec)
    {
        DEBUG_PRINT("  Disconnecting.\n");
        Disconnect();
        //  Try to reconnect right away.
        ec = channelConnect();
        if (EC_NONE == ec)
            ec = EC_TIMEOUT;
    }

    return ec;
}

CHEC
channel::writeMsgTimeout(unsigned int cbSize, const char *pBuff, int timeoutMS)
{
    CHEC ec = EC_NONE;
    double timeMS = 0;
    unsigned int bWritten = 0; // How many bytes sent so far
    struct timespec tspecBefore, tspecNow;

    // baseline time
    clock_gettime(CLOCK_MONOTONIC, &tspecBefore);

    DEBUG_PRINT("%s %d: %s attempting to send (%d bytes) to socket %d on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, cbSize, m_socket, m_port);
    while (bWritten < cbSize)
    {
        int bWrote = 0;

        bWrote = send(m_socket, pBuff + bWritten, cbSize - bWritten, MSG_NOSIGNAL | (timeoutMS ? MSG_DONTWAIT : 0));
        if (bWrote <= 0)
        {
            //  Check to see if the error is actually just a timeout
            //
            if (EWOULDBLOCK != errno && EAGAIN != errno)
            {
                ec = EC_NETWORK;
                DEBUG_PRINT("%s %d: %s  Error %d from write() to socket %d on port %d.\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno, m_socket, m_port);
                goto out;
            }

            //  See if we've gone past the timeout requested
            clock_gettime(CLOCK_MONOTONIC, &tspecNow);

            timeMS = ((double)(tspecNow.tv_sec - tspecBefore.tv_sec) * 1.0e9 + (double)(tspecNow.tv_nsec - tspecBefore.tv_nsec)) / 1000;

            if (timeoutMS && (timeoutMS <= timeMS))
            {
                ec = EC_TIMEOUT;
                break;
            }

            //  Still time left

            continue;
        }

        bWritten += (unsigned int) bWrote;
    }

out:
    if (EC_NONE == ec)
        DEBUG_PRINT("%s %d: %s sent (%d bytes) to socket %d on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, bWritten, m_socket, m_port);
    else
        DEBUG_PRINT("%s %d: %s error (%d) sending (%d bytes) to socket %d on port %d\n", __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR, errno, cbSize, m_socket, m_port);

    return ec;
}

void *
channel::my_malloc(size_t cbSize)
{
    if (NULL == m_pbFixedBuffer)
        return malloc(cbSize);

    if (cbSize > m_cbFixedBuffer)
        return NULL;

    return m_pbFixedBuffer;
}

void
channel::my_free(void *pBuff)
{

    if (pBuff == m_pbFixedBuffer)
        return;

    free(pBuff);

    return;
}

//
//  Internal helper functions
//

int
hostname_to_ip(char *hostname, char *ip)
{
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, "http", &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        h = (struct sockaddr_in *)p->ai_addr;
        strcpy(ip, inet_ntoa(h->sin_addr));
    }

    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}

void print_time(const char *szDesc, struct timespec *ptspec)
{
    printf("\t%s time:\n", szDesc);
    printf("\t\t%ld seconds\n", ptspec->tv_sec);
    printf("\t\t%ld nanoseconds\n", ptspec->tv_nsec);

    return;
}
