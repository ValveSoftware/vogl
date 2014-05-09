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

// File: vogl_dynamic_stream.h
#pragma once

#include "vogl_core.h"
#include "vogl_data_stream.h"

namespace vogl
{
    class dynamic_stream : public data_stream
    {
    public:
        dynamic_stream(uint32_t initial_size, const char *pName = "dynamic_stream", uint32_t attribs = cDataStreamSeekable | cDataStreamWritable | cDataStreamReadable)
            : data_stream(pName, attribs),
              m_ofs(0)
        {
            open(initial_size, pName, attribs);
        }

        dynamic_stream(const void *pBuf, uint32_t size, const char *pName = "dynamic_stream", uint32_t attribs = cDataStreamSeekable | cDataStreamWritable | cDataStreamReadable)
            : data_stream(pName, attribs),
              m_ofs(0)
        {
            open(pBuf, size, pName, attribs);
        }

        dynamic_stream()
            : data_stream(),
              m_ofs(0)
        {
            open();
        }

        virtual ~dynamic_stream()
        {
        }

        bool open(uint32_t initial_size = 0, const char *pName = "dynamic_stream", uint32_t attribs = cDataStreamSeekable | cDataStreamWritable | cDataStreamReadable)
        {
            close();

            m_opened = true;
            //m_buf.clear();
            m_buf.resize(initial_size);
            m_ofs = 0;
            m_name.set(pName ? pName : "dynamic_stream");
            m_attribs = static_cast<attribs_t>(attribs);
            return true;
        }

        bool reopen(const char *pName, uint32_t attribs)
        {
            if (!m_opened)
            {
                return open(0, pName, attribs);
            }

            m_name.set(pName ? pName : "dynamic_stream");
            m_attribs = static_cast<attribs_t>(attribs);
            return true;
        }

        bool open(const void *pBuf, uint32_t size, const char *pName = "dynamic_stream", uint32_t attribs = cDataStreamSeekable | cDataStreamWritable | cDataStreamReadable)
        {
            if (!m_opened)
            {
                m_opened = true;
                m_buf.resize(size);
                if (size)
                {
                    VOGL_ASSERT(pBuf);
                    memcpy(&m_buf[0], pBuf, size);
                }
                m_ofs = 0;
                m_name.set(pName ? pName : "dynamic_stream");
                m_attribs = static_cast<attribs_t>(attribs);
                return true;
            }

            return false;
        }

        virtual bool close()
        {
            if (m_opened)
            {
                m_opened = false;
                m_buf.clear();
                m_ofs = 0;
                return true;
            }

            return false;
        }

        bool reset()
        {
            if (m_opened)
            {
                m_opened = false;
                m_buf.resize(0);
                m_ofs = 0;
                return true;
            }

            return false;
        }

        const vogl::vector<uint8_t> &get_buf() const
        {
            return m_buf;
        }
        vogl::vector<uint8_t> &get_buf()
        {
            return m_buf;
        }

        void reserve(uint32_t size)
        {
            if (m_opened)
            {
                m_buf.reserve(size);
            }
        }

        virtual const void *get_ptr() const
        {
            return m_buf.is_empty() ? NULL : &m_buf[0];
        }

        virtual uint32_t read(void *pBuf, uint32_t len)
        {
            VOGL_ASSERT(len <= 0x7FFFFFFF);

            if ((!m_opened) || (!is_readable()) || (!len))
                return 0;

            VOGL_ASSERT(m_ofs <= m_buf.size());

            uint32_t bytes_left = m_buf.size() - m_ofs;

            len = math::minimum<uint32_t>(len, bytes_left);

            if (len)
                memcpy(pBuf, &m_buf[m_ofs], len);

            m_ofs += len;

            return len;
        }

        virtual uint32_t write(const void *pBuf, uint32_t len)
        {
            VOGL_ASSERT(len <= 0x7FFFFFFF);

            if ((!m_opened) || (!is_writable()) || (!len))
                return 0;

            VOGL_ASSERT(m_ofs <= m_buf.size());

            uint32_t new_ofs = m_ofs + len;
            if (new_ofs > m_buf.size())
                m_buf.resize(new_ofs);

            memcpy(&m_buf[m_ofs], pBuf, len);
            m_ofs = new_ofs;

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

            return m_buf.size();
        }

        virtual uint64_t get_remaining() const
        {
            if (!m_opened)
                return 0;

            VOGL_ASSERT(m_ofs <= m_buf.size());

            return m_buf.size() - m_ofs;
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
            else if (new_ofs > m_buf.size())
                return false;

            m_ofs = static_cast<uint32_t>(new_ofs);

            return true;
        }

    private:
        vogl::vector<uint8_t> m_buf;
        uint32_t m_ofs;
    };

} // namespace vogl
