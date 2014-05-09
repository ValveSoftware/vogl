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

/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

#if !defined(_MD5_H)
#define _MD5_H

#include "vogl_core.h"
#include "vogl_json.h"
#include "vogl_data_stream_serializer.h"

namespace vogl
{

    /* Any 32-bit or wider unsigned integer data type will do */
    typedef unsigned int MD5_u32plus;

    struct MD5_CTX
    {
        MD5_u32plus lo, hi;
        MD5_u32plus a, b, c, d;
        unsigned char buffer[64];
        MD5_u32plus block[16];
    };

    void MD5_Init(MD5_CTX *ctx);
    void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size);
    void MD5_Final(unsigned char *result, MD5_CTX *ctx);
    void MD5_Final(MD5_CTX *ctx);

    // rg: Basic C++ helpers
    class md5_hash : public utils::relative_ops<md5_hash>
    {
    public:
        inline md5_hash()
        {
        }
        inline md5_hash(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
        {
            init(a, b, c, d);
        }

		// This is a convenience function for creating MD5 hashes directly from Windows UUID structures.
		inline md5_hash(uint32_t a32, uint16_t b16, uint16_t c16, uint8_t d8_8[8])
		{
			uint32_t a = a32;
			uint32_t b = uint32_t(b16) << 16 | c16;
			uint32_t c = (*(uint32_t*)(&d8_8[0]));
			uint32_t d = (*(uint32_t*)(&d8_8[4]));

			init(a, b, c, d);
		}

        // pStr must be zero terminated, 32 hex chars, big endian order
        inline md5_hash(const char *pStr)
        {
            bool success = init(pStr);
            VOGL_ASSERT(success);
            VOGL_NOTE_UNUSED(success);
        }

        // pStr must be zero terminated, 32 hex chars, big endian order
        inline bool init(const char *pStr)
        {
            if (vogl_strlen(pStr) != 32)
                return false;

            for (uint32_t i = 0; i < 16; ++i)
            {
                const int v1 = utils::from_hex(pStr[i * 2]);
                const int v0 = utils::from_hex(pStr[i * 2 + 1]);
                if ((v0 < 0) || (v1 < 0))
                    return false;

                uint32_t v = v0 | (v1 << 4);

                reinterpret_cast<uint8_t *>(m_state)[i] = static_cast<uint8_t>(v);
            }

            return true;
        }

        inline dynamic_string &get_string(dynamic_string &str) const
        {
            const uint8_t *pHash = reinterpret_cast<const uint8_t *>(m_state);
            str.set_len(32);
            for (uint32_t i = 0; i < 16; i++)
            {
                str.set_char(i * 2, utils::to_hex(pHash[i] >> 4));
                str.set_char(i * 2 + 1, utils::to_hex(pHash[i] & 0xF));
            }
            return str;
        }

        inline void clear()
        {
            utils::zero_object(*this);
        }

        inline void init(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
        {
            m_state[0] = a;
            m_state[1] = b;
            m_state[2] = c;
            m_state[3] = d;
        }

        // size() is in uint32_t's, not bytes!
        inline uint32_t size() const
        {
            return cStateSize;
        }
        inline uint32_t operator[](uint32_t index) const
        {
            return m_state[vogl_assert_range<uint32_t>(index, cStateSize)];
        }
        inline uint32_t &operator[](uint32_t index)
        {
            return m_state[vogl_assert_range<uint32_t>(index, cStateSize)];
        }

        inline uint32_t size_in_bytes() const
        {
            return cStateSize * sizeof(uint32_t);
        }
        inline const void *get_ptr() const
        {
            return m_state;
        }
        inline void set(const void *p)
        {
            memcpy(m_state, p, size_in_bytes());
        }

        inline bool operator==(const md5_hash &rhs) const
        {
            for (uint32_t i = 0; i < cStateSize; i++)
                if (m_state[i] != rhs.m_state[i])
                    return false;
            return true;
        }

        inline bool operator<(const md5_hash &rhs) const
        {
            for (uint32_t i = 0; i < cStateSize; i++)
                if (m_state[i] < rhs.m_state[i])
                    return true;
                else if (m_state[i] != rhs.m_state[i])
                    return false;
            return false;
        }

        inline bool json_serialize(json_node &dst_node, const char *pKey) const
        {
            return json_serialize(dst_node.add(pKey));
        }

        inline bool json_serialize(json_value &dst_val) const
        {
            dynamic_string str;
            get_string(str);
            dst_val.set_value(str);
            return true;
        }

        inline bool json_deserialize(const json_node &src_node, const char *pKey)
        {
            return json_deserialize(src_node.find_value(pKey));
        }

        inline bool json_deserialize(const json_value &src_val)
        {
            const char *pStr = src_val.as_string_ptr();
            return pStr ? init(pStr) : false;
        }

        friend inline data_stream_serializer &operator<<(data_stream_serializer &serializer, const md5_hash &h)
        {
            serializer.write(&h.m_state, sizeof(h.m_state));
            return serializer;
        }

        friend inline data_stream_serializer &operator>>(data_stream_serializer &serializer, md5_hash &h)
        {
            serializer.read(&h.m_state, sizeof(h.m_state));
            return serializer;
        }

        inline operator size_t() const
        {
            return (size_t)fast_hash(this, sizeof(*this));
        }

    private:
        enum
        {
            cStateSize = 4
        };
        uint32_t m_state[cStateSize];
    };

    class md5_hash_gen
    {
    public:
        inline md5_hash_gen()
        {
            clear();
        }

        inline md5_hash_gen(const void *p, uint32_t n)
        {
            clear();
            update(p, n);
        }

        inline md5_hash_gen(const char *pStr)
        {
            clear();
            update(pStr);
        }

        inline md5_hash_gen(const dynamic_string &str)
        {
            clear();
            update(str);
        }

        inline md5_hash_gen(const md5_hash_gen &other)
        {
            set(other);
        }

        inline md5_hash_gen &set(const md5_hash_gen &rhs)
        {
            memcpy(&m_ctx, &rhs.m_ctx, sizeof(m_ctx));
            return *this;
        }

        inline void clear()
        {
            MD5_Init(&m_ctx);
        }

        // finalize() doesn't invalidate the current hash, so it can be continued,
        // and there's never any question that this object is in a valid state of not.
        inline md5_hash finalize() const
        {
            MD5_CTX tmp_ctx;
            memcpy(&tmp_ctx, &m_ctx, sizeof(m_ctx));
            MD5_Final(&tmp_ctx);
            return md5_hash(tmp_ctx.a, tmp_ctx.b, tmp_ctx.c, tmp_ctx.d);
        }

        inline md5_hash_gen &update(const char *pStr)
        {
            uint32_t len = vogl_strlen(pStr);
            if (len)
                update(pStr, len);
            return *this;
        }

        inline md5_hash_gen &update(const dynamic_string &str)
        {
            if (str.get_len())
                update(str.get_ptr(), str.get_len());
            return *this;
        }

        // Hashes v as big endian
        inline md5_hash_gen &update(uint64_t v)
        {
            uint8_t octets[8];
            for (uint32_t i = 0; i < 8; i++)
            {
                octets[i] = static_cast<uint8_t>((v >> 56U) & 0xFF);
                v <<= 8U;
            }
            return update(octets, sizeof(octets));
        }

        // Hashes v as big endian
        inline md5_hash_gen &update(uint32_t v)
        {
            uint8_t octets[4];
            octets[0] = (v >> 24U) & 0xFF;
            octets[1] = (v >> 16U) & 0xFF;
            octets[2] = (v >> 8U) & 0xFF;
            octets[3] = v & 0xFF;
            return update(octets, sizeof(octets));
        }

        // Hashes v as big endian
        inline md5_hash_gen &update(uint16_t v)
        {
            uint8_t octets[2];
            octets[0] = (v >> 8U) & 0xFF;
            octets[1] = v & 0xFF;
            return update(octets, sizeof(octets));
        }

        inline md5_hash_gen &update(const void *p, uint32_t n)
        {
            MD5_Update(&m_ctx, p, n);
            return *this;
        }

        inline md5_hash_gen &update(const uint8_vec &buf)
        {
            return update(buf.get_ptr(), buf.size());
        }

        template <typename T>
        inline md5_hash_gen &update_obj_bits(const T &obj)
        {
            return update(&obj, sizeof(obj));
        }

        inline md5_hash_gen &update_huge_buf(const void *p, uint64_t n)
        {
            while (n)
            {
                unsigned long bytes_to_process = static_cast<unsigned long>(math::minimum<uint64_t>(cUINT32_MAX, n));
                MD5_Update(&m_ctx, p, bytes_to_process);

                n -= bytes_to_process;
                p = static_cast<const uint8_t *>(p) + bytes_to_process;
            }
            return *this;
        }

    private:
        MD5_CTX m_ctx;
    };

    bool md5_test();

} // namespace vogl

#endif
