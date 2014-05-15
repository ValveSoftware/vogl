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

// File: vogl_data_stream.cpp
#include "vogl_core.h"
#include "vogl_data_stream.h"
#include "vogl_file_utils.h"

#define c_CR (0xD) 
#define c_LF (0xA) 

namespace vogl
{
    data_stream::data_stream()
        : m_pUser_data(NULL),
          m_attribs(0),
          m_opened(false), m_error(false)
    {
    }

    data_stream::data_stream(const char *pName, uint32_t attribs)
        : m_name(pName),
          m_pUser_data(NULL),
          m_attribs(static_cast<uint16_t>(attribs)),
          m_opened(false), m_error(false)
    {
    }

    uint64_t data_stream::read64(void *pBuf, uint64_t len)
    {
        uint64_t total_bytes_read = 0;

        while (total_bytes_read < len)
        {
            uint32_t bytes_to_read = static_cast<uint32_t>(math::minimum<uint64_t>(0x7FFFFFFFU, len - total_bytes_read));

            if (read(static_cast<uint8_t *>(pBuf) + total_bytes_read, bytes_to_read) != bytes_to_read)
                break;

            total_bytes_read += bytes_to_read;
        }

        return total_bytes_read;
    }

    uint64_t data_stream::skip(uint64_t len)
    {
        uint64_t total_bytes_read = 0;

        const uint32_t cBufSize = 1024;
        uint8_t buf[cBufSize];

        while (len)
        {
            const uint64_t bytes_to_read = math::minimum<uint64_t>(sizeof(buf), len);
            const uint64_t bytes_read = read(buf, static_cast<uint32_t>(bytes_to_read));
            total_bytes_read += bytes_read;

            if (bytes_read != bytes_to_read)
                break;

            len -= bytes_read;
        }

        return total_bytes_read;
    }

    bool data_stream::read_line(dynamic_string &str)
    {
        str.empty();

        for (;;)
        {
            // Can't be const--need to deal with Mac OS9 style line endings via substition
            const int c = read_byte();

            if (c < 0)
            {
                if (!str.is_empty())
                    break;

                return false;
            }
            // ignores DOS EOF, assumes it's at the very end of the file
            else if ((26 == c) || (!c))
                continue;
            else if (c_CR == c)
            {
                // This path handles OS9 and Windows style line endings.
                // Check to see if the next character is a LF, if so eat that one too.
                if (c_LF == peek_byte()) {
                    read_byte();
                }
                break;
            }
            else if (c_LF == c)
            {
                // UNIX style line endings.
                break;
            }

            str.append_char(static_cast<char>(c));
        }

        return true;
    }

    bool data_stream::printf(const char *p, ...)
    {
        va_list args;

        va_start(args, p);
        dynamic_string buf;
        buf.format_args(p, args);
        va_end(args);

        return write(buf.get_ptr(), buf.get_len() * sizeof(char)) == buf.get_len() * sizeof(char);
    }

    bool data_stream::puts(const char *p)
    {
        uint32_t len = vogl_strlen(p);
        return write(p, len * sizeof(char)) == len * sizeof(char);
    }

    bool data_stream::puts(const dynamic_string &str)
    {
        if (!str.is_empty())
            return write(str.get_ptr(), str.get_len()) == str.get_len();

        return true;
    }

    bool data_stream::read_array(vector<uint8_t> &buf)
    {
        if (buf.size() < get_remaining())
        {
            //if (get_remaining() > 1024U*1024U*1024U)
            if (get_remaining() > static_cast<uint64_t>(cINT32_MAX))
                return false;

            if (!buf.try_resize(static_cast<uint32_t>(get_remaining())))
                return false;
        }

        if (!get_remaining())
        {
            buf.resize(0);
            return true;
        }

        return read(&buf[0], buf.size()) == buf.size();
    }

    bool data_stream::write_array(const vector<uint8_t> &buf)
    {
        if (!buf.is_empty())
            return write(&buf[0], buf.size()) == buf.size();
        return true;
    }

    bool data_stream::write_file_data(const char *pFilename)
    {
        uint64_t file_size;
        if (!file_utils::get_file_size(pFilename, file_size))
            return false;

        if (!file_size)
            return true;

        FILE *pFile = vogl_fopen(pFilename, "rb");
        if (!pFile)
            return false;

        uint8_vec buf(64 * 1024);

        uint64_t bytes_remaining = file_size;
        while (bytes_remaining)
        {
            uint32_t n = static_cast<uint32_t>(math::minimum<uint64_t>(buf.size(), bytes_remaining));

            if (vogl_fread(buf.get_ptr(), 1, n, pFile) != n)
            {
                vogl_fclose(pFile);
                return false;
            }

            if (write(buf.get_ptr(), n) != n)
            {
                vogl_fclose(pFile);
                return false;
            }

            bytes_remaining -= n;
        }

        vogl_fclose(pFile);
        return true;
    }

} // namespace vogl
