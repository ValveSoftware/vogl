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

//
//  Class used to manage ports used.  Allows you to keep track of ports used.
//

namespace ports
{

    class PortManager
    {

    public:
        PortManager(int portStart, int portIncrement, int portEnd);
        ~PortManager();

        //
        //  returns true if this works...false if it fails (see syslog for details)
        bool FInitialize();
        bool FInitialize(int portReserved); // Initially reserve this particular port

        //
        //  Either returns the port to use or -1 if no ports are available.
        int GetNextAvailablePort();

        //
        //  Makes available the port for future use, or -1 if the port passed in is out
        //  of range.
        int ReleasePort(int port);

    private:
        bool *m_rgPortUsage;
        int m_portStart;
        int m_portIncrement;
        int m_portEnd;

        int m_numPorts;

        bool m_fInitialized;
        pthread_mutex_t m_portList_mutex;
    };
} // namespace ports
