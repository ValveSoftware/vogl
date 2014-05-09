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

// File: vogl_cfile_stream.h
#pragma once

#include "vogl_core.h"
#include "vogl_data_stream.h"

namespace vogl
{
    class cfile_stream : public data_stream
    {
    public:
        cfile_stream()
            : data_stream(), m_pFile(NULL), m_size(0), m_ofs(0), m_has_ownership(false)
        {
        }

        cfile_stream(FILE *pFile, const char *pFilename, uint32_t attribs, bool has_ownership)
            : data_stream(), m_pFile(NULL), m_size(0), m_ofs(0), m_has_ownership(false)
        {
            open(pFile, pFilename, attribs, has_ownership);
        }

        cfile_stream(const char *pFilename, uint32_t attribs = cDataStreamReadable | cDataStreamSeekable, bool open_existing = false)
            : data_stream(), m_pFile(NULL), m_size(0), m_ofs(0), m_has_ownership(false)
        {
            open(pFilename, attribs, open_existing);
        }

        virtual ~cfile_stream()
        {
            close();
        }

        virtual bool close()
        {
            clear_error();

            if (m_opened)
            {
                bool status = true;
                if (m_has_ownership)
                {
                    if (EOF == vogl_fclose(m_pFile))
                        status = false;
                }

                m_pFile = NULL;
                m_opened = false;
                m_size = 0;
                m_ofs = 0;
                m_has_ownership = false;

                return status;
            }

            return false;
        }

        bool open(FILE *pFile, const char *pFilename, uint32_t attribs, bool has_ownership)
        {
            VOGL_ASSERT(pFile);
            VOGL_ASSERT(pFilename);

            close();

            set_name(pFilename);
            m_pFile = pFile;
            m_has_ownership = has_ownership;
            m_attribs = static_cast<uint16_t>(attribs);

            m_ofs = vogl_ftell(m_pFile);
            vogl_fseek(m_pFile, 0, SEEK_END);
            m_size = vogl_ftell(m_pFile);
            vogl_fseek(m_pFile, m_ofs, SEEK_SET);

            m_opened = true;

            return true;
        }

        bool open(const char *pFilename, uint32_t attribs = cDataStreamReadable | cDataStreamSeekable, bool open_existing = false)
        {
            VOGL_ASSERT(pFilename);

            close();

            m_attribs = static_cast<uint16_t>(attribs);

            const char *pMode;
            if ((is_readable()) && (is_writable()))
                pMode = open_existing ? "r+b" : "w+b";
            else if (is_writable())
                pMode = open_existing ? "ab" : "wb";
            else if (is_readable())
                pMode = "rb";
            else
            {
                set_error();
                return false;
            }

            FILE *pFile = vogl_fopen(pFilename, pMode);
            m_has_ownership = true;

            if (!pFile)
            {
                set_error();
                return false;
            }

            // TODO: Change stream class to support UCS2 filenames.

            return open(pFile, pFilename, attribs, true);
        }

        FILE *get_file() const
        {
            return m_pFile;
        }

        virtual uint32_t read(void *pBuf, uint32_t len)
        {
            VOGL_ASSERT(len <= 0x7FFFFFFF);

            if (!m_opened || (!is_readable()) || (!len))
                return 0;

            len = static_cast<uint32_t>(math::minimum<uint64_t>(len, get_remaining()));

            if (vogl_fread(pBuf, 1, len, m_pFile) != len)
            {
                set_error();
                return 0;
            }

            m_ofs += len;
            return len;
        }

        virtual uint32_t peek(char *out_char)
        {
            VOGL_ASSERT(out_char);

            if (!m_opened || (!is_readable()))
                return -1;

            int c = vogl_fgetc(m_pFile);
            vogl_ungetc(c, m_pFile);

            if (c == EOF)
                return 0;

            VOGL_ASSERT((c & 0xFF) == c);
            (*out_char) = (c & 0xFF);
             return 1;
        }

        virtual uint32_t write(const void *pBuf, uint32_t len)
        {
            VOGL_ASSERT(len <= 0x7FFFFFFF);

            if (!m_opened || (!is_writable()) || (!len))
                return 0;

            if (vogl_fwrite(pBuf, 1, len, m_pFile) != len)
            {
                set_error();
                return 0;
            }

            m_ofs += len;
            m_size = math::maximum(m_size, m_ofs);

            return len;
        }

        virtual bool flush()
        {
            if ((!m_opened) || (!is_writable()))
                return false;

            if (EOF == vogl_fflush(m_pFile))
            {
                set_error();
                return false;
            }

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
            else if (static_cast<uint64_t>(new_ofs) > m_size)
                return false;

            if (static_cast<uint64_t>(new_ofs) != m_ofs)
            {
                if (vogl_fseek(m_pFile, new_ofs, SEEK_SET) != 0)
                {
                    set_error();
                    return false;
                }

                m_ofs = new_ofs;
            }

            return true;
        }

        static bool read_file_into_array(const char *pFilename, vector<uint8_t> &buf)
        {
            cfile_stream in_stream(pFilename);
            if (!in_stream.is_opened())
                return false;
            return in_stream.read_array(buf);
        }

        static bool write_array_to_file(const char *pFilename, const vector<uint8_t> &buf)
        {
            cfile_stream out_stream(pFilename, cDataStreamWritable | cDataStreamSeekable);
            if (!out_stream.is_opened())
                return false;
            return out_stream.write_array(buf);
        }

    private:
        FILE *m_pFile;
        uint64_t m_size, m_ofs;
        bool m_has_ownership;
    };

} // namespace vogl
