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

// File: vogl_bigint128.h
#pragma once

#include "vogl_core.h"
#include "vogl_rand.h"

namespace vogl
{
    // Straightforward, portable 128-bit signed integer class.
    // Mostly intended to simplifying dealing with either signed or unsigned 64-bit values in a universal way.
    // Not particularly fast or flexible, it can't return 256 bits from 128*128 muls yet which is annoying.
    class bigint128 : public utils::relative_ops<bigint128>
    {
    public:
        enum
        {
            cMaxBYTEs = 16,
            cMaxQWORDs = 2,
            cMaxDWORDs = 4
        };

        inline bigint128()
        {
        }

        inline bigint128(int32_t x)
        {
            *this = x;
        }

        inline bigint128(int64_t x)
        {
            *this = x;
        }

        inline bigint128(uint32_t x)
        {
            *this = x;
        }

        inline bigint128(uint64_t x)
        {
            *this = x;
        }

        inline bigint128(uint64_t l, uint64_t h)
        {
            set(l, h);
        }

        inline bigint128 &set(uint64_t l, uint64_t h)
        {
            m_v[0] = l;
            m_v[1] = h;
            return *this;
        }

        bigint128(const void *p, uint32_t type_size_in_bytes, bool type_is_signed)
        {
            set_from_ptr(p, type_size_in_bytes, type_is_signed);
        }

        inline uint64_t get_qword(uint32_t index) const
        {
            return m_v[math::open_range_check<uint32_t>(index, cMaxQWORDs)];
        }
        inline bigint128 &set_qword(uint32_t index, uint64_t val)
        {
            m_v[math::open_range_check<uint32_t>(index, cMaxQWORDs)] = val;
            return *this;
        }

        inline uint32_t get_dword(uint32_t index) const
        {
            VOGL_ASSERT(index < cMaxDWORDs);
            uint64_t x = get_qword(index >> 1);
            if (index & 1)
                x >>= 32U;
            return static_cast<uint32_t>(x);
        }

        inline bigint128 &set_dword(uint32_t index, uint32_t val)
        {
            VOGL_ASSERT(index < cMaxDWORDs);
            uint32_t qword_index = index >> 1U;
            uint32_t shift = (index & 1) ? 32U : 0U;

            uint64_t mask = cUINT32_MAX;
            mask <<= shift;

            m_v[qword_index] &= ~mask;
            m_v[qword_index] |= (static_cast<uint64_t>(val) << shift);

            return *this;
        }

        inline void clear()
        {
            m_v[0] = 0;
            m_v[1] = 0;
        }

        inline bigint128 &operator=(int64_t x)
        {
            m_v[0] = x;
            m_v[1] = (x >> 63);
            return *this;
        }

        inline bigint128 &operator=(int32_t x)
        {
            *this = static_cast<int64_t>(x);
            return *this;
        }

        inline bigint128 &operator=(uint64_t x)
        {
            m_v[0] = x;
            m_v[1] = 0;
            return *this;
        }

        inline bigint128 &operator=(uint32_t x)
        {
            *this = static_cast<uint64_t>(x);
            return *this;
        }

        inline operator int8_t() const
        {
            return static_cast<int8_t>(m_v[0]);
        }

        inline operator int16_t() const
        {
            return static_cast<int16_t>(m_v[0]);
        }

        inline operator int32_t() const
        {
            return static_cast<int32_t>(m_v[0]);
        }

        inline operator int64_t() const
        {
            return static_cast<int64_t>(m_v[0]);
        }

        inline operator uint8_t() const
        {
            return static_cast<uint8_t>(m_v[0]);
        }

        inline operator uint16_t() const
        {
            return static_cast<uint16_t>(m_v[0]);
        }

        inline operator uint32_t() const
        {
            return static_cast<uint32_t>(m_v[0]);
        }

        inline operator uint64_t() const
        {
            return static_cast<uint64_t>(m_v[0]);
        }

        inline bool is_zero() const
        {
            return (m_v[0] == 0) && (m_v[1] == 0);
        }

        inline operator bool() const
        {
            return !is_zero();
        }

        // FIXME: not endian safe, assumes little endian
        bigint128 &set_from_ptr(const void *p, uint32_t type_size_in_bytes, bool type_is_signed)
        {
            if (type_size_in_bytes == 0)
            {
                clear();
                return *this;
            }
            else if (type_size_in_bytes > cMaxBYTEs)
            {
                VOGL_ASSERT_ALWAYS;
                clear();
                return *this;
            }

            const uint8_t *pSrc = reinterpret_cast<const uint8_t *>(p);
            uint8_t *pDst = reinterpret_cast<uint8_t *>(m_v);
            memcpy(pDst, pSrc, type_size_in_bytes);

            if (type_size_in_bytes < cMaxBYTEs)
            {
                uint32_t z = 0;
                if (type_is_signed)
                {
                    bool is_signed = (pSrc[type_size_in_bytes - 1] & 0x80) != 0;
                    z = is_signed ? 0xFF : 0;
                }
                memset(pDst + type_size_in_bytes, z, cMaxBYTEs - type_size_in_bytes);
            }

            return *this;
        }

        static inline void get_type_range(uint32_t type_size_in_bytes, bool type_is_signed, bigint128 &min_val, bigint128 &max_val)
        {
            if ((!type_size_in_bytes) || (type_size_in_bytes > 16))
            {
                VOGL_ASSERT_ALWAYS;
                min_val = 0U;
                max_val = 0U;
                return;
            }

            max_val = 1U;
            max_val <<= (type_size_in_bytes * 8U);
            --max_val;

            min_val = 0U;
            if (type_is_signed)
            {
                max_val = unsigned_shift_right(max_val, 1);
                min_val = -max_val - 1U;
            }
        }

        inline bool range_check(uint32_t type_size_in_bytes, bool type_is_signed) const
        {
            if (!type_size_in_bytes)
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }
            else if (type_size_in_bytes > 16)
                return true;

            bigint128 min_val, max_val;
            get_type_range(type_size_in_bytes, type_is_signed, min_val, max_val);
            if (type_is_signed)
                return (*this >= min_val) && (*this <= max_val);
            else
                return unsigned_less_equal(max_val);
        }

        inline bool is_negative() const
        {
            return static_cast<int64_t>(m_v[1]) < 0;
        }

        inline bool is_positive() const
        {
            return static_cast<int64_t>(m_v[1]) >= 0;
        }

        inline bigint128 operator+() const
        {
            return *this;
        }

        inline bool operator==(const bigint128 &other) const
        {
            return (m_v[0] == other.m_v[0]) && (m_v[1] == other.m_v[1]);
        }
        inline bool operator==(int32_t x) const
        {
            return *this == bigint128(x);
        }
        inline bool operator==(uint32_t x) const
        {
            return *this == bigint128(x);
        }
        inline bool operator==(int64_t x) const
        {
            return *this == bigint128(x);
        }
        inline bool operator==(uint64_t x) const
        {
            return *this == bigint128(x);
        }

        inline bool unsigned_less(const bigint128 &other) const
        {
            if (m_v[1] < other.m_v[1])
                return true;
            else if (m_v[1] == other.m_v[1])
                return m_v[0] < other.m_v[0];

            return false;
        }
        inline bool unsigned_greater(const bigint128 &other) const
        {
            return other.unsigned_less(*this);
        }
        inline bool unsigned_less_equal(const bigint128 &other) const
        {
            return !other.unsigned_less(*this);
        }
        inline bool unsigned_greater_equal(const bigint128 &other) const
        {
            return !unsigned_less(other);
        }

        inline bool operator<(const bigint128 &other) const
        {
            if (static_cast<int64_t>(m_v[1]) < static_cast<int64_t>(other.m_v[1]))
                return true;
            else if (m_v[1] == other.m_v[1])
                return m_v[0] < other.m_v[0];

            return false;
        }
        inline bool operator<(int32_t x) const
        {
            return *this < bigint128(x);
        }
        inline bool operator<(uint32_t x) const
        {
            return *this < bigint128(x);
        }
        inline bool operator<(int64_t x) const
        {
            return *this < bigint128(x);
        }
        inline bool operator<(uint64_t x) const
        {
            return *this < bigint128(x);
        }
        inline bigint128 &operator-=(const bigint128 &other)
        {
            *this = *this - other;
            return *this;
        }
        inline bool operator<=(int32_t x) const
        {
            return *this <= bigint128(x);
        }
        inline bool operator<=(uint32_t x) const
        {
            return *this <= bigint128(x);
        }
        inline bool operator<=(int64_t x) const
        {
            return *this <= bigint128(x);
        }
        inline bool operator<=(uint64_t x) const
        {
            return *this <= bigint128(x);
        }

        inline bool operator>(int32_t x) const
        {
            return *this > bigint128(x);
        }
        inline bool operator>(uint32_t x) const
        {
            return *this > bigint128(x);
        }
        inline bool operator>(int64_t x) const
        {
            return *this > bigint128(x);
        }
        inline bool operator>(uint64_t x) const
        {
            return *this > bigint128(x);
        }

        inline bool operator>=(int32_t x) const
        {
            return *this >= bigint128(x);
        }
        inline bool operator>=(uint32_t x) const
        {
            return *this >= bigint128(x);
        }
        inline bool operator>=(int64_t x) const
        {
            return *this >= bigint128(x);
        }
        inline bool operator>=(uint64_t x) const
        {
            return *this >= bigint128(x);
        }

        inline bigint128 operator~() const
        {
            bigint128 result;
            result.m_v[0] = ~m_v[0];
            result.m_v[1] = ~m_v[1];
            return result;
        }

        inline bigint128 &operator++()
        {
            bool carry = (m_v[0] == cUINT64_MAX);
            m_v[0]++;
            m_v[1] += carry;
            return *this;
        }

        inline bigint128 operator++(int)
        {
            bigint128 result(*this);
            bool carry = (m_v[0] == cUINT64_MAX);
            m_v[0]++;
            m_v[1] += carry;
            return result;
        }

        inline bigint128 &operator--()
        {
            bool carry = (m_v[0] == 0);
            m_v[0]--;
            m_v[1] -= carry;
            return *this;
        }

        inline bigint128 operator--(int)
        {
            bigint128 result(*this);
            bool carry = (m_v[0] == 0);
            m_v[0]--;
            m_v[1] -= carry;
            return result;
        }

        inline bigint128 &operator|=(const bigint128 &other)
        {
            *this = *this | other;
            return *this;
        }

        inline bigint128 &operator^=(const bigint128 &other)
        {
            *this = *this ^ other;
            return *this;
        }

        inline bigint128 &operator&=(const bigint128 &other)
        {
            *this = *this & other;
            return *this;
        }

        inline bigint128 &operator>>=(uint32_t n)
        {
            *this = *this >> n;
            return *this;
        }

        inline bigint128 &operator<<=(uint32_t n)
        {
            *this = *this << n;
            return *this;
        }

        inline bigint128 get_abs() const
        {
            return is_negative() ? (-(*this)) : (*this);
        }

        friend inline bigint128 operator|(const bigint128 &lhs, const bigint128 &rhs)
        {
            return bigint128(lhs.m_v[0] | rhs.m_v[0], lhs.m_v[1] | rhs.m_v[1]);
        }

        friend inline bigint128 operator^(const bigint128 &lhs, const bigint128 &rhs)
        {
            return bigint128(lhs.m_v[0] ^ rhs.m_v[0], lhs.m_v[1] ^ rhs.m_v[1]);
        }

        friend inline bigint128 operator&(const bigint128 &lhs, const bigint128 &rhs)
        {
            return bigint128(lhs.m_v[0] & rhs.m_v[0], lhs.m_v[1] & rhs.m_v[1]);
        }

        friend inline bigint128 operator+(const bigint128 &lhs, const bigint128 &rhs)
        {
            bigint128 result;

            result.m_v[0] = lhs.m_v[0] + rhs.m_v[0];

            // Being careful here to avoid known gcc overflow bugs
            uint64_t max_to_add_before_overflow = cUINT64_MAX - lhs.m_v[0];
            bool carry = rhs.m_v[0] > max_to_add_before_overflow;

            result.m_v[1] = lhs.m_v[1] + rhs.m_v[1] + carry;
            return result;
        }

        inline bigint128 &operator+=(const bigint128 &other)
        {
            *this = *this + other;
            return *this;
        }

        friend inline bigint128 operator-(const bigint128 &lhs, const bigint128 &rhs)
        {
            bigint128 result;

            result.m_v[0] = lhs.m_v[0] - rhs.m_v[0];

            bool carry = rhs.m_v[0] > lhs.m_v[0];

            result.m_v[1] = lhs.m_v[1] - rhs.m_v[1] - carry;
            return result;
        }

        inline bigint128 operator-() const
        {
            bigint128 result;
            result.m_v[0] = ~m_v[0];
            result.m_v[1] = ~m_v[1];

            if (result.m_v[0] != cUINT64_MAX)
                result.m_v[0]++;
            else
            {
                result.m_v[0] = 0;
                result.m_v[1]++;
            }
            return result;
        }

        friend inline bigint128 operator+(const bigint128 &lhs, int32_t x)
        {
            return lhs + bigint128(x);
        }
        friend inline bigint128 operator+(const bigint128 &lhs, int64_t x)
        {
            return lhs + bigint128(x);
        }
        friend inline bigint128 operator+(const bigint128 &lhs, uint32_t x)
        {
            return lhs + bigint128(x);
        }
        friend inline bigint128 operator+(const bigint128 &lhs, uint64_t x)
        {
            return lhs + bigint128(x);
        }

        friend inline bigint128 operator-(const bigint128 &lhs, int32_t x)
        {
            return lhs - bigint128(x);
        }
        friend inline bigint128 operator-(const bigint128 &lhs, int64_t x)
        {
            return lhs - bigint128(x);
        }
        friend inline bigint128 operator-(const bigint128 &lhs, uint32_t x)
        {
            return lhs - bigint128(x);
        }
        friend inline bigint128 operator-(const bigint128 &lhs, uint64_t x)
        {
            return lhs - bigint128(x);
        }

        // signed shift right
        friend inline bigint128 operator>>(const bigint128 &lhs, uint32_t n)
        {
            if (!n)
                return lhs;
            else if (n >= 128)
                return lhs.is_negative() ? bigint128(-1) : bigint128(UINT64_C(0));

            uint64_t l = lhs.m_v[0];
            uint64_t h = lhs.m_v[1];

            if (n >= 64)
            {
                l = h;
                h = lhs.is_negative() ? cUINT64_MAX : 0;
                n -= 64;
            }

            // n can be 0-63 here
            if (n)
            {
                l >>= n;
                l |= ((h & ((UINT64_C(1) << n) - UINT64_C(1))) << (64 - static_cast<int>(n)));
                h = static_cast<uint64_t>(static_cast<int64_t>(h) >> static_cast<int>(n));
            }

            return bigint128(l, h);
        }

        static inline bigint128 unsigned_shift_right(const bigint128 &lhs, uint32_t n)
        {
            if (!n)
                return lhs;
            else if (n >= 128)
                return bigint128(UINT64_C(0));

            uint64_t l = lhs.m_v[0];
            uint64_t h = lhs.m_v[1];

            if (n >= 64)
            {
                l = h;
                h = 0;
                n -= 64;
            }

            // n can be 0-63 here
            if (n)
            {
                l >>= n;
                l |= ((h & ((UINT64_C(1) << n) - UINT64_C(1))) << (64 - static_cast<int>(n)));
                h >>= n;
            }

            return bigint128(l, h);
        }

        // shift left
        friend inline bigint128 operator<<(const bigint128 &lhs, uint32_t n)
        {
            if (!n)
                return lhs;
            else if (n >= 128)
                return bigint128(UINT64_C(0));

            uint64_t l = lhs.m_v[0];
            uint64_t h = lhs.m_v[1];

            if (n >= 64)
            {
                h = l;
                l = 0;
                n -= 64;
            }

            // n can be 0-63 here
            if (n)
            {
                h <<= n;
                h |= (l >> (64 - static_cast<int>(n)));
                l <<= n;
            }

            return bigint128(l, h);
        }

        // unsigned op
        uint32_t get_num_significant_bits() const
        {
            bigint128 d(*this);
            uint32_t num = 0;
            while (d)
            {
                num++;
                d = d.unsigned_shift_right(d, 1);
            }
            return num;
        }

        // true if overflowed (returns lower 128 bits of result)
        static bool unsigned_multiply(bigint128 x, bigint128 y, bigint128 &result)
        {
            // Shortcut if both are 32-bit.
            if ((x.m_v[1] | y.m_v[1]) == 0)
            {
                if (((x.m_v[0] | y.m_v[0]) >> 32U) == 0)
                {
                    result = x.m_v[0] * y.m_v[0];
                    return false;
                }
            }

            // Minor optimization: ensure x <= y.
            if (x > y)
            {
                std::swap(x, y);
            }

            bool overflow_flag = false;

            bigint128 value(0U);

            // Seems most portable to use unsigned mul of 32x32=64 bits as the underlying primitive.
            for (uint32_t i = 0; i < cMaxDWORDs; ++i)
            {
                const uint64_t m = static_cast<uint64_t>(x.get_dword(i));
                if (!m)
                    continue;
                for (uint32_t j = 0; j < cMaxDWORDs; ++j)
                {
                    uint32_t k = i + j;

                    uint64_t product = m * y.get_dword(j);
                    while (product)
                    {
                        if (k >= cMaxDWORDs)
                        {
                            overflow_flag = true;
                            break;
                        }

                        product += value.get_dword(k);
                        value.set_dword(k, static_cast<uint32_t>(product));
                        product >>= 32U;
                        k++;
                    }
                }
            }
            result = value;

            return overflow_flag;
        }

        // true if overflowed
        static bool signed_multiply(bigint128 a, bigint128 b, bigint128 &result)
        {
            bool a_is_negative = a.is_negative();
            bool b_is_negative = b.is_negative();

            bool overflow_flag = unsigned_multiply(a.get_abs(), b.get_abs(), result);

            if (a_is_negative != b_is_negative)
            {
                if (result.unsigned_greater(bigint128(0, UINT64_C(0x8000000000000000))))
                    overflow_flag = true;

                result = -result;
            }
            else if (result.is_negative())
                overflow_flag = true;

            return overflow_flag;
        }

        // true on success
        static bool unsigned_divide(const bigint128 &a, const bigint128 &b, bigint128 &q, bigint128 &r)
        {
            // Shortcut if both are 64-bit.
            if ((!a.m_v[1]) && (!b.m_v[1]))
            {
                q = a.m_v[0] / b.m_v[0];
                r = a.m_v[0] % b.m_v[0];
                return true;
            }

            int num_a = a.get_num_significant_bits();
            int num_b = b.get_num_significant_bits();
            if (!num_b)
            {
                VOGL_ASSERT_ALWAYS;
                q = 0;
                r = 0;
                return false;
            }

            int cur_pos = num_a - num_b;

            // Good old fashioned non-restoring division 1 bit at a time. Hope you're not busy!
            bigint128 c(a);
            bigint128 f(0);
            bigint128 k;
            if (cur_pos >= 0)
            {
                k = b << static_cast<uint32_t>(cur_pos);

                do
                {
                    f <<= 1U;

                    if (c.unsigned_greater_equal(k))
                    {
                        f |= UINT64_C(1);
                        c -= k;
                    }

                    k = unsigned_shift_right(k, 1U);
                    cur_pos--;
                } while (cur_pos >= 0);
            }

            q = f;
            r = c;

            VOGL_ASSERT(r.unsigned_less(b));
            return true;
        }

        static bool signed_divide(const bigint128 &a, const bigint128 &b, bigint128 &q, bigint128 &r)
        {
            if (!b)
            {
                VOGL_ASSERT_ALWAYS;
                q = 0;
                r = 0;
                return false;
            }

            bool a_is_negative = a.is_negative();
            bool b_is_negative = b.is_negative();

            // abs()'s are safe here, the min 128-bit signed value can be represented as an unsigned 128-bit value
            unsigned_divide(a.get_abs(), b.get_abs(), q, r);

            if (a_is_negative != b_is_negative)
                q = -q;

            if (a_is_negative)
                r = -r;

            return true;
        }

        friend inline bigint128 operator*(const bigint128 &lhs, const bigint128 &rhs)
        {
            bigint128 result;
            signed_multiply(lhs, rhs, result);
            return result;
        }
        friend inline bigint128 operator*(const bigint128 &lhs, int32_t x)
        {
            return lhs * bigint128(x);
        }
        friend inline bigint128 operator*(const bigint128 &lhs, int64_t x)
        {
            return lhs * bigint128(x);
        }
        friend inline bigint128 operator*(const bigint128 &lhs, uint32_t x)
        {
            return lhs * bigint128(x);
        }
        friend inline bigint128 operator*(const bigint128 &lhs, uint64_t x)
        {
            return lhs * bigint128(x);
        }

        bigint128 &operator*=(const bigint128 &other)
        {
            *this = *this * other;
            return *this;
        }
        bigint128 &operator*=(int32_t other)
        {
            *this = *this * other;
            return *this;
        }
        bigint128 &operator*=(uint32_t other)
        {
            *this = *this * other;
            return *this;
        }
        bigint128 &operator*=(int64_t other)
        {
            *this = *this * other;
            return *this;
        }
        bigint128 &operator*=(uint64_t other)
        {
            *this = *this * other;
            return *this;
        }

        friend inline bigint128 operator/(const bigint128 &lhs, const bigint128 &rhs)
        {
            bigint128 q, r;
            signed_divide(lhs, rhs, q, r);
            return q;
        }
        friend inline bigint128 operator/(const bigint128 &lhs, int32_t x)
        {
            return lhs / bigint128(x);
        }
        friend inline bigint128 operator/(const bigint128 &lhs, int64_t x)
        {
            return lhs / bigint128(x);
        }
        friend inline bigint128 operator/(const bigint128 &lhs, uint32_t x)
        {
            return lhs / bigint128(x);
        }
        friend inline bigint128 operator/(const bigint128 &lhs, uint64_t x)
        {
            return lhs / bigint128(x);
        }

        bigint128 &operator/=(const bigint128 &other)
        {
            *this = *this / other;
            return *this;
        }
        bigint128 &operator/=(int32_t other)
        {
            *this = *this / other;
            return *this;
        }
        bigint128 &operator/=(uint32_t other)
        {
            *this = *this / other;
            return *this;
        }
        bigint128 &operator/=(int64_t other)
        {
            *this = *this / other;
            return *this;
        }
        bigint128 &operator/=(uint64_t other)
        {
            *this = *this / other;
            return *this;
        }

        friend inline bigint128 operator%(const bigint128 &lhs, const bigint128 &rhs)
        {
            bigint128 q, r;
            signed_divide(lhs, rhs, q, r);
            return r;
        }
        friend inline bigint128 operator%(const bigint128 &lhs, int32_t x)
        {
            return lhs % bigint128(x);
        }
        friend inline bigint128 operator%(const bigint128 &lhs, int64_t x)
        {
            return lhs % bigint128(x);
        }
        friend inline bigint128 operator%(const bigint128 &lhs, uint32_t x)
        {
            return lhs % bigint128(x);
        }
        friend inline bigint128 operator%(const bigint128 &lhs, uint64_t x)
        {
            return lhs % bigint128(x);
        }

        bigint128 &operator%=(const bigint128 &other)
        {
            *this = *this % other;
            return *this;
        }
        bigint128 &operator%=(int32_t other)
        {
            *this = *this % other;
            return *this;
        }
        bigint128 &operator%=(uint32_t other)
        {
            *this = *this % other;
            return *this;
        }
        bigint128 &operator%=(int64_t other)
        {
            *this = *this % other;
            return *this;
        }
        bigint128 &operator%=(uint64_t other)
        {
            *this = *this % other;
            return *this;
        }

        static bigint128 get_random(random &r)
        {
            return bigint128(r.urand64(), r.urand64());
        }

        static bigint128 get_smallest()
        {
            return bigint128(0, cINT64_MIN);
        }

        static bigint128 get_largest()
        {
            return bigint128(cUINT64_MAX, cINT64_MAX);
        }

        static bigint128 unsigned_get_largest()
        {
            return bigint128(cUINT64_MAX, cUINT64_MAX);
        }

        static bigint128 get_all_zeros()
        {
            return bigint128(0U, 0U);
        }

        static bigint128 get_all_ones()
        {
            return bigint128(cUINT64_MAX, cUINT64_MAX);
        }

    private:
        uint64_t m_v[2];
    };

    VOGL_DEFINE_BITWISE_COPYABLE(bigint128);

    inline void bigint128_check(bool success, uint32_t &num_failures)
    {
        if (!success)
        {
            num_failures++;
            VOGL_VERIFY(0);
        }
    }

    inline bool bigint128_test(uint32_t seed = 5)
    {
        VOGL_NOTE_UNUSED(seed);
        VOGL_ASSUME(sizeof(bigint128) == sizeof(uint64_t) * 2U);

        uint32_t num_failures = 0;

        bigint128 zero(0U);
        bigint128 one(1);
        bigint128 neg_one(-1);

#define BIGINT128_CHECK(x) bigint128_check(x, num_failures)
        BIGINT128_CHECK(neg_one.get_qword(0) == cUINT64_MAX);
        BIGINT128_CHECK(neg_one.get_qword(1) == cUINT64_MAX);
        BIGINT128_CHECK(neg_one + 2 == 1);
        BIGINT128_CHECK(zero.is_zero());
        BIGINT128_CHECK(one.is_positive() && !one.is_zero() && !one.is_negative());
        BIGINT128_CHECK(neg_one.is_negative() && !neg_one.is_zero() && !neg_one.is_positive());
        BIGINT128_CHECK(one > zero);
        BIGINT128_CHECK(!(zero > one));
        BIGINT128_CHECK(zero < one);
        BIGINT128_CHECK(one > neg_one);
        BIGINT128_CHECK(neg_one < zero);
        BIGINT128_CHECK(zero > neg_one);
        BIGINT128_CHECK(neg_one < zero);
        BIGINT128_CHECK(bigint128(zero)++ == zero);
        BIGINT128_CHECK(++bigint128(zero) == one);
        BIGINT128_CHECK(bigint128(one)-- == one);
        BIGINT128_CHECK(--bigint128(one) == zero);
        BIGINT128_CHECK(--bigint128(zero) < zero);
        BIGINT128_CHECK(--bigint128(zero) == -1);
        BIGINT128_CHECK(++bigint128(zero) == 1);
        BIGINT128_CHECK(++bigint128(cUINT64_MAX) == bigint128(0, 1));
        BIGINT128_CHECK(--bigint128(0, 1) == cUINT64_MAX);
        BIGINT128_CHECK(++bigint128(cUINT64_MAX, cUINT64_MAX) == zero);
        BIGINT128_CHECK(bigint128(0, 0) - bigint128(1, 0) == bigint128(-1));
        BIGINT128_CHECK(bigint128(1, 0) + bigint128(cUINT64_MAX, cUINT64_MAX) == bigint128(0));

        BIGINT128_CHECK(bigint128(0, 1) < bigint128(0, 2));
        BIGINT128_CHECK(bigint128(0, 2) > bigint128(0, 1));
        BIGINT128_CHECK(bigint128(0, cUINT64_MAX) < -1);
        BIGINT128_CHECK(bigint128(0, cUINT64_MAX) > bigint128::get_smallest());

        BIGINT128_CHECK(bigint128::get_smallest() < bigint128::get_largest());
        BIGINT128_CHECK(bigint128::get_largest() > bigint128::get_smallest());
        BIGINT128_CHECK(bigint128::get_smallest() < 0);
        BIGINT128_CHECK(bigint128::get_largest() > 0);
        BIGINT128_CHECK(bigint128::get_smallest() < (bigint128::get_smallest() + 1));
        BIGINT128_CHECK(bigint128::get_largest() > (bigint128::get_largest() - 1));

        BIGINT128_CHECK(bigint128::get_smallest() < bigint128(0, cINT64_MAX));
        BIGINT128_CHECK(bigint128::get_largest() > bigint128(0, cUINT64_MAX));

        BIGINT128_CHECK((bigint128::get_smallest() - 1) == bigint128::get_largest());
        BIGINT128_CHECK((bigint128::get_largest() + 1) == bigint128::get_smallest());

        BIGINT128_CHECK((bigint128::get_smallest() - 2) == (bigint128::get_largest() - 1));
        BIGINT128_CHECK((bigint128::get_largest() + 2) == (bigint128::get_smallest() + 1));

        BIGINT128_CHECK(~bigint128(0) == bigint128(-1));
        BIGINT128_CHECK(~~bigint128(0) == bigint128(0));
        BIGINT128_CHECK((bigint128(0xDEADBEEF, 0xDEADBEEF) ^ bigint128(0xDEADBEEF, 0xDEADBEEF) ^ bigint128(0xDEADBEEF, 0xDEADBEEF) ^ bigint128(0xDEADBEEF, 0xDEADBEEF)) == 0);

        bigint128 zz(bigint128(0xDEADBEEF, 0xDEADBEEF));
        BIGINT128_CHECK((zz ^= zz) == 0);
        BIGINT128_CHECK((bigint128(0) | bigint128(1)) == 1);

        BIGINT128_CHECK(bigint128(0, 1) - bigint128(1) == cUINT64_MAX);
        BIGINT128_CHECK(bigint128(0, 50) - bigint128(0, 50) == 0);
        BIGINT128_CHECK(bigint128(0, 50) - bigint128(1, 50) == -1);
        BIGINT128_CHECK(bigint128(-1) - bigint128(-1) == 0);
        BIGINT128_CHECK(bigint128(-1) + bigint128(1) == 0);

        BIGINT128_CHECK(bigint128(0).range_check(1, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFF)).range_check(1, false));

        BIGINT128_CHECK(bigint128(0).range_check(2, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFF)).range_check(2, false));

        BIGINT128_CHECK(bigint128(0).range_check(3, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFF)).range_check(3, false));

        BIGINT128_CHECK(bigint128(0).range_check(4, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFF)).range_check(4, false));

        BIGINT128_CHECK(bigint128(0).range_check(5, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFFFF)).range_check(5, false));

        BIGINT128_CHECK(bigint128(0).range_check(6, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFFFFFF)).range_check(6, false));

        BIGINT128_CHECK(bigint128(0).range_check(7, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFFFFFFFF)).range_check(7, false));

        BIGINT128_CHECK(bigint128(0).range_check(8, false));
        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFFFFFFFFFF)).range_check(8, false));

        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFFFFFFFF80), cUINT64_MAX).range_check(1, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0xFFFFFFFFFFFFFF7F), cUINT64_MAX).range_check(1, true));
        BIGINT128_CHECK(bigint128(UINT64_C(0x000000000000007F), 0).range_check(1, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0x0000000000000080), 0).range_check(1, true));

        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFFFFFF8000), cUINT64_MAX).range_check(2, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0xFFFFFFFFFFFF7FFF), cUINT64_MAX).range_check(2, true));
        BIGINT128_CHECK(bigint128(UINT64_C(0x0000000000007FFF), 0).range_check(2, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0x0000000000008000), 0).range_check(2, true));

        BIGINT128_CHECK(bigint128(UINT64_C(0xFFFFFFFF80000000), cUINT64_MAX).range_check(4, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0xFFFFFFFF7FFFFFFF), cUINT64_MAX).range_check(4, true));
        BIGINT128_CHECK(bigint128(UINT64_C(0x000000007FFFFFFF), 0).range_check(4, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0x0000000080000000), 0).range_check(4, true));

        BIGINT128_CHECK(bigint128(UINT64_C(0x8000000000000000), cUINT64_MAX).range_check(8, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0x7FFFFFFFFFFFFFFF), cUINT64_MAX).range_check(8, true));
        BIGINT128_CHECK(!(bigint128(UINT64_C(0x7FFFFFFFFFFFFFFF), cUINT64_MAX) - 1000).range_check(8, true));
        BIGINT128_CHECK(bigint128(UINT64_C(0x7FFFFFFFFFFFFFFF), 0).range_check(8, true));
        BIGINT128_CHECK(!bigint128(UINT64_C(0x8000000000000000), 0).range_check(8, true));
        BIGINT128_CHECK(!(bigint128(UINT64_C(0x8000000000000000), 0) + 1000).range_check(8, true));

        BIGINT128_CHECK(bigint128(cUINT64_MAX, UINT64_C(0x8000000000000000)).range_check(16, true));
        BIGINT128_CHECK(bigint128(cUINT64_MAX, UINT64_C(0x7FFFFFFFFFFFFFFF)).range_check(16, true));

        BIGINT128_CHECK(bigint128(0, 0).range_check(16, false));
        BIGINT128_CHECK(bigint128(cUINT64_MAX, 0).range_check(16, false));
        BIGINT128_CHECK(bigint128(cUINT64_MAX, cUINT64_MAX).range_check(16, false));

        {
            bigint128 x(0U);
            BIGINT128_CHECK(x == 0);
            BIGINT128_CHECK(x.is_zero());
            BIGINT128_CHECK(!(x < 0));
            BIGINT128_CHECK(!(x > 0));
        }

#if defined(VOGL_PLATFORM_PC_X64) && defined(COMPILER_GCCLIKE)
        vogl::random r;
        r.seed(seed);
        for (uint32_t i = 0; i < 1000000000; i++)
        {
            __int128 x(0), y(0), z(0);

            uint32_t n0 = r.irand_inclusive(1, 16);
            for (uint32_t j = 0; j < n0; j++)
                ((uint8_t *)&x)[j] = r.urand32() >> 8;
            if (n0 < 16)
            {
                if (r.urand32() & 1)
                    x = -x;
            }

            uint32_t n1 = r.irand_inclusive(1, 16);
            for (uint32_t j = 0; j < n1; j++)
                ((uint8_t *)&y)[j] = r.urand32() >> 8;
            if (n1 < 16)
            {
                if (r.urand32() & 1)
                    y = -y;
            }

            if (!r.irand(0, 100))
                x = 0;
            if (!r.irand(0, 100))
                y = 0;

            uint32_t op = r.irand_inclusive(0, 24);

            bigint128 bx((uint64_t)x, (uint64_t)(x >> 64U));
            bigint128 by((uint64_t)y, (uint64_t)(y >> 64U));
            bigint128 bz(0);

            if ((op >= 21) && (op <= 24))
            {
                if (!r.irand(0, 30))
                {
                    bx = bigint128::get_smallest();
                    memcpy(&x, &bx, sizeof(x));
                }
                if (!r.irand(0, 30))
                {
                    bx = bigint128::get_largest();
                    memcpy(&x, &bx, sizeof(x));
                }
                if (!r.irand(0, 30))
                {
                    by = bigint128::get_smallest();
                    memcpy(&y, &by, sizeof(y));
                }
                if (!r.irand(0, 30))
                {
                    by = bigint128::get_largest();
                    memcpy(&y, &by, sizeof(y));
                }
            }

            uint32_t shift = r.irand_inclusive(0, 127);

            switch (op)
            {
                case 0:
                {
                    z = x + y;
                    bz = bx + by;
                    break;
                }
                case 1:
                {
                    z = x - y;
                    bz = bx - by;
                    break;
                }
                case 2:
                {
                    z = x < y;
                    bz = bx < by;
                    break;
                }
                case 3:
                {
                    z = x <= y;
                    bz = bx <= by;
                    break;
                }
                case 4:
                {
                    z = x > y;
                    bz = bx > by;
                    break;
                }
                case 5:
                {
                    z = x >= y;
                    bz = bx >= by;
                    break;
                }
                case 6:
                {
                    z = y ^ x;
                    bz = bx ^ by;
                    break;
                }
                case 7:
                {
                    z = y | x;
                    bz = bx | by;
                    break;
                }
                case 8:
                {
                    z = x >> shift;
                    bz = bx >> shift;
                    break;
                }
                case 9:
                {
                    z = x << shift;
                    bz = bx << shift;
                    break;
                }
                case 10:
                {
                    z = x & y;
                    bz = bx & by;
                    break;
                }
                case 11:
                {
                    x += y;
                    z = x;
                    bx += by;
                    bz = bx;
                    break;
                }
                case 12:
                {
                    x -= y;
                    z = x;
                    bx -= by;
                    bz = bx;
                    break;
                }
                case 13:
                {
                    z = x--;
                    bz = bx--;
                    break;
                }
                case 14:
                {
                    z = --x;
                    bz = --bx;
                    break;
                }
                case 15:
                {
                    z = ++x;
                    bz = ++bx;
                    break;
                }
                case 16:
                {
                    z = x++;
                    bz = bx++;
                    break;
                }

                case 17:
                {
                    z = (__uint128_t)x < (__uint128_t)y;
                    bz = bx.unsigned_less(by);
                    break;
                }
                case 18:
                {
                    z = (__uint128_t)x <= (__uint128_t)y;
                    bz = bx.unsigned_less_equal(by);
                    break;
                }
                case 19:
                {
                    z = (__uint128_t)x > (__uint128_t)y;
                    bz = bx.unsigned_greater(by);
                    break;
                }
                case 20:
                {
                    z = (__uint128_t)x >= (__uint128_t)y;
                    bz = bx.unsigned_greater_equal(by);
                    break;
                }
                case 21:
                {
                    if (y)
                    {
                        z = (__uint128_t)x / (__uint128_t)y;
                        __int128 m = (__uint128_t)x % (__uint128_t)y;

                        bigint128 r2;
                        VOGL_VERIFY(bigint128::unsigned_divide(bx, by, bz, r2));

                        BIGINT128_CHECK((uint64_t)m == r2.get_qword(0));
                        BIGINT128_CHECK((uint64_t)(m >> 64) == r2.get_qword(1));
                    }
                    break;
                }
                case 22:
                {
                    if (y)
                    {
                        z = x / y;
                        __int128 m = x % y;

                        bigint128 r2;
                        VOGL_VERIFY(bigint128::signed_divide(bx, by, bz, r2));
                        bigint128 nr(-r2);
                        VOGL_NOTE_UNUSED(nr);

                        BIGINT128_CHECK((uint64_t)m == r2.get_qword(0));
                        BIGINT128_CHECK((uint64_t)(m >> 64) == r2.get_qword(1));
                    }
                    break;
                }
                case 23:
                {
                    z = (__uint128_t)x * (__uint128_t)y;
                    bigint128::unsigned_multiply(bx, by, bz);
                    break;
                }
                case 24:
                {
                    z = x * y;
                    bigint128::signed_multiply(bx, by, bz);
                    break;
                }

                // TODO: unsigned comps, signed/unsigned division
                default:
                    VOGL_VERIFY(0);
                    break;
            }

            BIGINT128_CHECK((uint64_t)z == bz.get_qword(0));
            BIGINT128_CHECK((uint64_t)(z >> 64) == bz.get_qword(1));
        }
#endif

#undef BIGINT128_CHECK

        return !num_failures;
    }

} // namespace vogl

