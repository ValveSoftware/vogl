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
#include <pthread.h>
#include "../common/radlogging.h"

#include "portmgr.h"

using namespace ports;

PortManager::PortManager(int portStart, int portIncrement, int portEnd)
{
    m_fInitialized = false;

    m_portStart = portStart;
    m_portIncrement = portIncrement;
    m_portEnd = portEnd;

    m_numPorts = (m_portEnd - m_portStart) / m_portIncrement;

    m_rgPortUsage = NULL;
}

PortManager::~PortManager()
{
    if (false == m_fInitialized)
        return;

    if (m_rgPortUsage)
        free(m_rgPortUsage);

    (void)pthread_mutex_destroy(&m_portList_mutex); //  Nothing to look at
}

bool
PortManager::FInitialize(int portReserved)
{
    bool fReturn = false;

    if ((portReserved < m_portStart) || (portReserved > m_portEnd))
    {
        syslog(RAD_CRITICAL, "PortMgr::Finitialize(%d) - portReserved out of range (%d - %d).\n", portReserved, m_portStart, m_portEnd);
        goto out;
    }

    fReturn = FInitialize();
    if (false == fReturn)
        goto out;

    //
    //  Now reserve this particular port in the list
    //
    m_rgPortUsage[portReserved - m_portStart] = true;

out:
    return fReturn;
}

bool
PortManager::FInitialize()
{

    pthread_mutexattr_t mAttr;
    int rc = 0;

    if (m_fInitialized)
        goto out; // already initialized

    //
    //  Need to allocate the array of ports
    //
    if (m_rgPortUsage)
        free(m_rgPortUsage);

    m_rgPortUsage = (bool *)malloc(1 + m_numPorts);
    if (NULL == m_rgPortUsage)
    {
        syslog(RAD_CRITICAL, "Unable to allocate memory for PortMgr array.\n");
        goto out;
    }

    //  Initialize to unused (false)
    for (int iPort = 0; iPort < m_numPorts; iPort++)
        m_rgPortUsage[iPort] = false;

    //  Initialize mutex for assigning new ports
    // setup recursive mutex for mutex attribute - this is most like, in behavior, to Windows critical sections
    //
    //  PTHREAD_MUTEX_RECURSIVE_NP means that the mutex can be used recursively.
    /*
			The pthread_mutexattr_settype() function shall fail if:
			EINVAL  The value type is invalid.
	*/
    rc = pthread_mutexattr_init(&mAttr);
    if (0 != rc)
    {
        syslog(RAD_CRITICAL, "Error on pthread_mutexattr_init(return code = %x)\nTerminating...\n", rc);
        goto out;
    }

    rc = pthread_mutexattr_settype(&mAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    if (0 != rc)
    {
        syslog(RAD_CRITICAL, "Error on pthread_mutexattr_settype(return code = %x)\nTerminating...\n", rc);
        goto out;
    }
    // Use the mutex attribute to create the mutex
    /*
		The pthread_mutex_init() function shall fail if:
		EAGAIN  The system lacked the necessary resources (other than memory) to initialize another mutex.
		ENOMEM  Insufficient memory exists to initialize the mutex.
		EPERM   The caller does not have the privilege to perform the operation.

		The pthread_mutex_init() function may fail if:
		EBUSY   The implementation has detected an attempt to reinitialize the object referenced by mutex, a previously initialized, but not yet destroyed, mutex.
		EINVAL  The value specified by attr is invalid.
	*/
    rc = pthread_mutex_init(&m_portList_mutex, &mAttr);
    if (0 != rc)
    {
        syslog(RAD_CRITICAL, "Error on port manager pthread_mutex_init(return code = %x)\nTerminating...\n", rc);
        goto out;
    }
    m_fInitialized = true;

out:

    return m_fInitialized;
}

int PortManager::GetNextAvailablePort()
{
    int freePort = -1;
    int ec = 0;
    (void)ec;

    if (false == m_fInitialized)
        return -1;

    ec = pthread_mutex_lock(&m_portList_mutex);
    for (int i = 0; i < m_numPorts; i++)
    {
        if (false == m_rgPortUsage[i])
        {
            //  This port is free
            freePort = (i * m_portIncrement) + m_portStart;
            m_rgPortUsage[i] = true;
            break;
        }
    }
    pthread_mutex_unlock(&m_portList_mutex);

    return freePort;
}

int PortManager::ReleasePort(int port)
{
    int portIndex = (port - m_portStart) / m_portIncrement;
    int ec = 0;
    (void)ec;

    if (false == m_fInitialized)
        return -1;

    if (portIndex < 0 || portIndex >= m_numPorts)
    {
        syslog(RAD_WARN, "Attempting to release port number out of range(%d)\n", port);
        return -1;
    }

    ec = pthread_mutex_lock(&m_portList_mutex);

    m_rgPortUsage[portIndex] = false;

    pthread_mutex_unlock(&m_portList_mutex);

    return port;
}
