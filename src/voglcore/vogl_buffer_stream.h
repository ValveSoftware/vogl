/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
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

// File: vogl_buffer_stream.h
#pragma once

#include "vogl_core.h"
#include "vogl_data_stream.h"

namespace vogl
{
    class buffer_stream : public data_stream
    {
    public:
        buffer_stream()
            : data_stream(),
              m_pBuf(NULL),
              m_size(0),
              m_ofs(0)
        {
        }

        buffer_stream(void *p, size_t size)
            : data_stream(),
              m_pBuf(NULL),
              m_size(0),
              m_ofs(0)
        {
            open(p, size);
        }

        buffer_stream(const void *p, size_t size)
            : data_stream(),
              m_pBuf(NULL),
              m_size(0),
              m_ofs(0)
        {
            open(p, size);
        }

        virtual ~buffer_stream()
        {
        }

        bool open(const void *p, size_t size)
        {
            VOGL_ASSERT(p);

            close();

            if ((!p) || (!size))
                return false;

            m_opened = true;
            m_pBuf = (uint8_t *)(p);
            m_size = size;
            m_ofs = 0;
            m_attribs = cDataStreamSeekable | cDataStreamReadable;
            return true;
        }

        bool open(void *p, size_t size)
        {
            VOGL_ASSERT(p);

            close();

            if ((!p) || (!size))
                return false;

            m_opened = true;
            m_pBuf = static_cast<uint8_t *>(p);
            m_size = size;
            m_ofs = 0;
            m_attribs = cDataStreamSeekable | cDataStreamWritable | cDataStreamReadable;
            return true;
        }

        virtual bool close()
        {
            if (m_opened)
            {
                m_opened = false;
                m_pBuf = NULL;
                m_size = 0;
                m_ofs = 0;
                return true;
            }

            return false;
        }

        const void *get_buf() const
        {
            return m_pBuf;
        }
        void *get_buf()
        {
            return m_pBuf;
        }

        virtual const void *get_ptr() const
        {
            return m_pBuf;
        }

        virtual uint32_t read(void *pBuf, uint32_t len)
        {
            VOGL_ASSERT(len <= 0x7FFFFFFF);

            if ((!m_opened) || (!is_readable()) || (!len))
                return 0;

            VOGL_ASSERT(m_ofs <= m_size);

            uint64_t bytes_left = m_size - m_ofs;

            len = static_cast<uint32_t>(math::minimum<uint64_t>(len, bytes_left));

            if (len)
                memcpy(pBuf, &m_pBuf[m_ofs], len);

            m_ofs += len;

            return len;
        }

        virtual uint32_t peek(char *pBuf)
        {
            if ((!m_opened) || (!is_readable()))
                return -1;

            uint64_t bytes_left = m_size - m_ofs;
            if (bytes_left == 0)
                return 0;

            (*pBuf) = m_pBuf[m_ofs];
            return 1;
        }

        virtual uint32_t write(const void *pBuf, uint32_t len)
        {
            VOGL_ASSERT(len <= 0x7FFFFFFF);

            if ((!m_opened) || (!is_writable()) || (!len))
                return 0;

            VOGL_ASSERT(m_ofs <= m_size);

            uint64_t bytes_left = m_size - m_ofs;

            len = static_cast<uint32_t>(math::minimum<uint64_t>(len, bytes_left));

            if (len)
                memcpy(&m_pBuf[m_ofs], pBuf, len);

            m_ofs += len;

            return len;
        }

        virtual bool flush()
        {
            if (!m_opened)
                return false;

            return true;
        }

        virtual uint64_t get_size() const
        {
            if (!m_opened)
                return 0;

            return m_size;
        }

        virtual uint64_t get_remaining() const
        {
            if (!m_opened)
                return 0;

            VOGL_ASSERT(m_ofs <= m_size);

            return m_size - m_ofs;
        }

        virtual uint64_t get_ofs() const
        {
            if (!m_opened)
                return 0;

            return m_ofs;
        }

        virtual bool seek(int64_t ofs, bool relative)
        {
            if ((!m_opened) || (!is_seekable()))
                return false;

            int64_t new_ofs = relative ? (m_ofs + ofs) : ofs;

            if (new_ofs < 0)
                return false;
            else if (new_ofs > static_cast<int64_t>(m_size))
                return false;

            m_ofs = static_cast<size_t>(new_ofs);

            return true;
        }

    private:
        uint8_t *m_pBuf;
        size_t m_size;
        size_t m_ofs;
    };

} // namespace vogl
