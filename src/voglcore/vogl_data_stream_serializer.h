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

// File: data_stream_serializer.h
#pragma once

#include "vogl_core.h"
#include "vogl_data_stream.h"

namespace vogl
{
    // Defaults to little endian mode.
    class data_stream_serializer
    {
    public:
        data_stream_serializer()
            : m_pStream(NULL), m_little_endian(true)
        {
        }
        data_stream_serializer(data_stream *pStream)
            : m_pStream(pStream), m_little_endian(true)
        {
        }
        data_stream_serializer(data_stream &stream)
            : m_pStream(&stream), m_little_endian(true)
        {
        }
        data_stream_serializer(const data_stream_serializer &other)
            : m_pStream(other.m_pStream), m_little_endian(other.m_little_endian)
        {
        }

        data_stream_serializer &operator=(const data_stream_serializer &rhs)
        {
            m_pStream = rhs.m_pStream;
            m_little_endian = rhs.m_little_endian;
            return *this;
        }

        data_stream *get_stream() const
        {
            return m_pStream;
        }
        void set_stream(data_stream *pStream)
        {
            m_pStream = pStream;
        }

        const dynamic_string &get_name() const
        {
            return m_pStream ? m_pStream->get_name() : get_empty_dynamic_string();
        }

        // true if stream has latched an error
        bool get_error()
        {
            return m_pStream ? m_pStream->get_error() : false;
        }

        bool get_little_endian() const
        {
            return m_little_endian;
        }
        void set_little_endian(bool little_endian)
        {
            m_little_endian = little_endian;
        }

        bool write(const void *pBuf, uint32_t len)
        {
            return m_pStream->write(pBuf, len) == len;
        }

        bool read(void *pBuf, uint32_t len)
        {
            return m_pStream->read(pBuf, len) == len;
        }

        // size = size of each element, count = number of elements, returns actual count of elements written
        uint32_t write(const void *pBuf, uint32_t size, uint32_t count)
        {
            uint32_t actual_size = size * count;
            if (!actual_size)
                return 0;
            uint32_t n = m_pStream->write(pBuf, actual_size);
            if (n == actual_size)
                return count;
            return n / size;
        }

        // size = size of each element, count = number of elements, returns actual count of elements read
        uint32_t read(void *pBuf, uint32_t size, uint32_t count)
        {
            uint32_t actual_size = size * count;
            if (!actual_size)
                return 0;
            uint32_t n = m_pStream->read(pBuf, actual_size);
            if (n == actual_size)
                return count;
            return n / size;
        }

        bool write_chars(const char *pBuf, uint32_t len)
        {
            return write(pBuf, len);
        }

        bool read_chars(char *pBuf, uint32_t len)
        {
            return read(pBuf, len);
        }

        bool skip(uint32_t len)
        {
            return m_pStream->skip(len) == len;
        }

        template <typename T>
        bool write_object(const T &obj)
        {
            if (m_little_endian == c_vogl_little_endian_platform)
                return write(&obj, sizeof(obj));
            else
            {
                uint8_t buf[sizeof(T)];
                uint32_t buf_size = sizeof(T);
                void *pBuf = buf;
                utils::write_obj(obj, pBuf, buf_size, m_little_endian);

                return write(buf, sizeof(T));
            }
        }

        template <typename T>
        bool read_object(T &obj)
        {
            if (m_little_endian == c_vogl_little_endian_platform)
                return read(&obj, sizeof(obj));
            else
            {
                uint8_t buf[sizeof(T)];
                if (!read(buf, sizeof(T)))
                    return false;

                uint32_t buf_size = sizeof(T);
                const void *pBuf = buf;
                utils::read_obj(obj, pBuf, buf_size, m_little_endian);

                return true;
            }
        }

        template <typename T>
        bool write_value(T value)
        {
            return write_object(value);
        }

        template <typename T>
        T read_value(const T &on_error_value = T())
        {
            T result;
            if (!read_object(result))
                result = on_error_value;
            return result;
        }

        int8_t read_int8(int8_t def = 0)
        {
            int8_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        uint8_t read_uint8(uint8_t def = 0)
        {
            uint8_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        int16_t read_int16(int16_t def = 0)
        {
            int16_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        uint16_t read_uint16(uint16_t def = 0)
        {
            uint16_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        int32_t read_int32(int32_t def = 0)
        {
            int32_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        uint32_t read_uint32(uint32_t def = 0)
        {
            uint32_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        float read_float(float def = 0)
        {
            float result;
            if (!read_object(result))
                result = def;
            return result;
        }

        double read_double(double def = 0)
        {
            double result;
            if (!read_object(result))
                result = def;
            return result;
        }

        int64_t read_int64(int64_t def = 0)
        {
            int64_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        uint64_t read_uint64(uint64_t def = 0)
        {
            uint64_t result;
            if (!read_object(result))
                result = def;
            return result;
        }

        template <typename T>
        bool write_enum(T e)
        {
            int val = static_cast<int>(e);
            return write_object(val);
        }

        template <typename T>
        T read_enum()
        {
            return static_cast<T>(read_value<int>());
        }

        // Writes uint32_t using a simple variable length code (VLC).
        bool write_uint_vlc(uint32_t val)
        {
            do
            {
                uint8_t c = static_cast<uint8_t>(val) & 0x7F;
                if (val <= 0x7F)
                    c |= 0x80;

                if (!write_value(c))
                    return false;

                val >>= 7;
            } while (val);

            return true;
        }

        // Reads uint32_t using a simple variable length code (VLC).
        bool read_uint_vlc(uint32_t &val)
        {
            val = 0;
            uint32_t shift = 0;

            for (;;)
            {
                if (shift >= 32)
                    return false;

                uint8_t c;
                if (!read_object(c))
                    return false;

                val |= ((c & 0x7F) << shift);
                shift += 7;

                if (c & 0x80)
                    break;
            }

            return true;
        }

        bool write_c_str(const char *p)
        {
            uint32_t len = static_cast<uint32_t>(strlen(p));
            if (!write_uint_vlc(len))
                return false;

            return write_chars(p, len);
        }

        bool read_c_str(char *pBuf, uint32_t buf_size)
        {
            uint32_t len;
            if (!read_uint_vlc(len))
                return false;
            if ((len + 1) > buf_size)
                return false;

            pBuf[len] = '\0';

            return read_chars(pBuf, len);
        }

        bool write_string(const dynamic_string &str)
        {
            if (!write_uint_vlc(str.get_len()))
                return false;

            return write_chars(str.get_ptr(), str.get_len());
        }

        bool read_string(dynamic_string &str)
        {
            uint32_t len;
            if (!read_uint_vlc(len))
                return false;

            if (!str.set_len(len))
                return false;

            if (len)
            {
                if (!read_chars(str.get_ptr_raw(), len))
                    return false;

                if (memchr(str.get_ptr(), 0, len) != NULL)
                {
                    str.truncate(0);
                    return false;
                }
            }

            return true;
        }

        template <typename T>
        bool write_vector(const T &vec)
        {
            if (!write_uint_vlc(vec.size()))
                return false;

            for (uint32_t i = 0; i < vec.size(); i++)
            {
                *this << vec[i];
                if (get_error())
                    return false;
            }

            return true;
        };

        template <typename T>
        bool read_vector(T &vec, uint32_t num_expected = UINT_MAX)
        {
            uint32_t size;
            if (!read_uint_vlc(size))
                return false;

            if ((size * sizeof(T::value_type)) >= 2U * 1024U * 1024U * 1024U)
                return false;

            if ((num_expected != UINT_MAX) && (size != num_expected))
                return false;

            vec.resize(size);
            for (uint32_t i = 0; i < vec.size(); i++)
            {
                *this >> vec[i];

                if (get_error())
                    return false;
            }

            return true;
        }

        bool read_entire_file(vogl::vector<uint8_t> &buf)
        {
            return m_pStream->read_array(buf);
        }

        bool write_entire_file(const vogl::vector<uint8_t> &buf)
        {
            return m_pStream->write_array(buf);
        }

        // Got this idea from the Molly Rocket forums.
        // fmt may contain the characters "1", "2", or "4".
        bool writef(char *fmt, ...)
        {
            va_list v;
            va_start(v, fmt);

            while (*fmt)
            {
                switch (*fmt++)
                {
                    case '1':
                    {
                        const uint8_t x = static_cast<uint8_t>(va_arg(v, uint32_t));
                        if (!write_value(x))
                            return false;
                    }
                    case '2':
                    {
                        const uint16_t x = static_cast<uint16_t>(va_arg(v, uint32_t));
                        if (!write_value(x))
                            return false;
                    }
                    case '4':
                    {
                        const uint32_t x = static_cast<uint32_t>(va_arg(v, uint32_t));
                        if (!write_value(x))
                            return false;
                    }
                    case ' ':
                    case ',':
                    {
                        break;
                    }
                    default:
                    {
                        VOGL_ASSERT_ALWAYS;
                        return false;
                    }
                }
            }

            va_end(v);
            return true;
        }

        // Got this idea from the Molly Rocket forums.
        // fmt may contain the characters "1", "2", or "4".
        bool readf(char *fmt, ...)
        {
            va_list v;
            va_start(v, fmt);

            while (*fmt)
            {
                switch (*fmt++)
                {
                    case '1':
                    {
                        uint8_t *x = va_arg(v, uint8_t *);
                        VOGL_ASSERT(x);
                        if (!read_object(*x))
                            return false;
                    }
                    case '2':
                    {
                        uint16_t *x = va_arg(v, uint16_t *);
                        VOGL_ASSERT(x);
                        if (!read_object(*x))
                            return false;
                    }
                    case '4':
                    {
                        uint32_t *x = va_arg(v, uint32_t *);
                        VOGL_ASSERT(x);
                        if (!read_object(*x))
                            return false;
                    }
                    case ' ':
                    case ',':
                    {
                        break;
                    }
                    default:
                    {
                        VOGL_ASSERT_ALWAYS;
                        return false;
                    }
                }
            }

            va_end(v);
            return true;
        }

    private:
        data_stream *m_pStream;

        bool m_little_endian;
    };

    // Write operators
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, bool val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, int8_t val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, uint8_t val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, int16_t val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, uint16_t val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, int32_t val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, uint32_t val)
    {
        serializer.write_uint_vlc(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, int64_t val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, uint64_t val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, float val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, double val)
    {
        serializer.write_value(val);
        return serializer;
    }
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, const char *p)
    {
        serializer.write_c_str(p);
        return serializer;
    }

    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, const dynamic_string &str)
    {
        serializer.write_string(str);
        return serializer;
    }

    template <typename T>
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, const vogl::vector<T> &vec)
    {
        serializer.write_vector(vec);
        return serializer;
    }

    template <typename T>
    inline data_stream_serializer &operator<<(data_stream_serializer &serializer, const T *p)
    {
        serializer.write_object(*p);
        return serializer;
    }

    // Read operators
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, bool &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, int8_t &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, uint8_t &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, int16_t &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, uint16_t &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, int32_t &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, uint32_t &val)
    {
        serializer.read_uint_vlc(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, int64_t &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, uint64_t &val)
    {
        serializer.read_object(val);
        return serializer;
    }
#ifdef NEVER // __amd64__
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, long &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, unsigned long &val)
    {
        serializer.read_object(val);
        return serializer;
    }
#endif
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, float &val)
    {
        serializer.read_object(val);
        return serializer;
    }
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, double &val)
    {
        serializer.read_object(val);
        return serializer;
    }

    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, dynamic_string &str)
    {
        serializer.read_string(str);
        return serializer;
    }

    template <typename T>
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, vogl::vector<T> &vec)
    {
        serializer.read_vector(vec);
        return serializer;
    }

    template <typename T>
    inline data_stream_serializer &operator>>(data_stream_serializer &serializer, T *p)
    {
        serializer.read_object(*p);
        return serializer;
    }

} // namespace vogl
