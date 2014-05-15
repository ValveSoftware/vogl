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

// File: vogl_data_stream.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    enum data_stream_attribs
    {
        cDataStreamReadable = 1,
        cDataStreamWritable = 2,
        cDataStreamSeekable = 4
    };

    const int64_t DATA_STREAM_SIZE_UNKNOWN = cINT64_MAX;
    const int64_t DATA_STREAM_SIZE_INFINITE = cUINT64_MAX;

    class data_stream
    {
        data_stream(const data_stream &);
        data_stream &operator=(const data_stream &);

    public:
        data_stream();
        data_stream(const char *pName, uint32_t attribs);

        virtual ~data_stream()
        {
        }

        virtual data_stream *get_parent()
        {
            return NULL;
        }

        virtual bool close()
        {
            m_opened = false;
            m_error = false;
            return true;
        }

        typedef uint16_t attribs_t;
        inline attribs_t get_attribs() const
        {
            return m_attribs;
        }

        inline bool is_opened() const
        {
            return m_opened;
        }

        inline bool is_readable() const
        {
            return utils::is_bit_set(m_attribs, cDataStreamReadable);
        }
        inline bool is_writable() const
        {
            return utils::is_bit_set(m_attribs, cDataStreamWritable);
        }
        inline bool is_seekable() const
        {
            return utils::is_bit_set(m_attribs, cDataStreamSeekable);
        }

        // true if stream has latched an error
        inline bool get_error() const
        {
            return m_error;
        }

        inline const dynamic_string &get_name() const
        {
            return m_name;
        }
        inline void set_name(const char *pName)
        {
            m_name.set(pName);
        }

        virtual uint32_t read(void *pBuf, uint32_t len) = 0;
        inline virtual uint32_t peek(char *out_char) { VOGL_VERIFY(!"peek not implemented for this class."); return 0; }
        uint64_t read64(void *pBuf, uint64_t len);

        virtual uint64_t skip(uint64_t len);

        virtual uint32_t write(const void *pBuf, uint32_t len) = 0;
        virtual bool flush() = 0;

        virtual bool is_size_known() const
        {
            return true;
        }

        // Returns DATA_STREAM_SIZE_UNKNOWN if size hasn't been determined yet, or DATA_STREAM_SIZE_INFINITE for infinite streams.
        virtual uint64_t get_size() const = 0;
        virtual uint64_t get_remaining() const = 0;

        virtual uint64_t get_ofs() const = 0;
        virtual bool seek(int64_t ofs, bool relative) = 0;

        virtual const void *get_ptr() const
        {
            return NULL;
        }

        inline int read_byte()
        {
            uint8_t c;
            if (read(&c, 1) != 1)
                return -1;
            return c;
        }
        inline bool write_byte(uint8_t c)
        {
            return write(&c, 1) == 1;
        }
        inline int peek_byte()
        {
            char c;
            if (peek(&c) != 1)
                return -1;
            return c;
        }

        bool read_line(dynamic_string &str);
        bool printf(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(2, 3);
        bool puts(const char *p);
        bool puts(const dynamic_string &str);
        bool write_bom()
        {
            uint16_t bom = 0xFEFF;
            return write(&bom, sizeof(bom)) == sizeof(bom);
        }

        bool read_array(vector<uint8_t> &buf);
        bool write_array(const vector<uint8_t> &buf);

        void set_user_data(void *p)
        {
            m_pUser_data = p;
        }
        void *get_user_data() const
        {
            return m_pUser_data;
        }

        // Writes the entire contents of a binary file to the stream, starting at the current file position.
        bool write_file_data(const char *pFilename);

    protected:
        dynamic_string m_name;
        void *m_pUser_data;

        attribs_t m_attribs;
        bool m_opened : 1;
        bool m_error : 1;

        inline void set_error()
        {
            m_error = true;
        }
        inline void clear_error()
        {
            m_error = false;
        }
    };

} // namespace vogl
