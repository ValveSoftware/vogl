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

// File: vogl_matrix.h
#pragma once

#include "vogl_core.h"
#include "vogl_vec.h"

namespace vogl
{
    // Matrix/vector cheat sheet, because confusingly, depending on how matrices are stored in memory people can use opposite definitions of "rows", "cols", etc.
    // See http://www.mindcontrol.org/~hplus/graphics/matrix-layout.html
    //
    // So in this simple general matrix class:
    // matrix=[NumRows][NumCols] or [R][C], i.e. a 3x3 matrix stored in memory will appear as: R0C0, R0C1, R0C2,  R1C0, R1C1, R1C2,  etc.
    // Matrix multiplication: [R0,C0]*[R1,C1]=[R0,C1], C0 must equal R1
    //
    // In this class:
    // A "row vector" type is a vector of size # of matrix cols, 1xC. It's the vector type that is used to store the matrix rows.
    // A "col vector" type is a vector of size # of matrix rows, Rx1. It's a vector type large enough to hold each matrix column.
    //
    // Subrow/col vectors: last component is assumed to be either 0 (a "vector") or 1 (a "point")
    // "subrow vector": vector/point of size # cols-1, 1x(C-1)
    // "subcol vector": vector/point of size # rows-1, (R-1)x1
    //
    // D3D style:
    // vec*matrix, row vector on left (vec dotted against columns)
    // [1,4]*[4,4]=[1,4]
    // abcd * A B C D
    //        A B C D
    //        A B C D
    //        A B C D
    // =      e f g h
    //
    // Now confusingly, in the matrix transform method for vec*matrix below the vector's type is "col_vec", because col_vec will have the proper size for non-square matrices. But the vector on the left is written as row vector, argh.
    //
    //
    // OGL style:
    // matrix*vec, col vector on right (vec dotted against rows):
    // [4,4]*[4,1]=[4,1]
    //
    // A B C D * e = e
    // A B C D   f   f
    // A B C D   g   g
    // A B C D   h   h

    template <class X, class Y, class Z>
    Z &matrix_mul_helper(Z &result, const X &lhs, const Y &rhs)
    {
        VOGL_ASSUME(Z::num_rows == X::num_rows);
        VOGL_ASSUME(Z::num_cols == Y::num_cols);
        VOGL_ASSUME(X::num_cols == Y::num_rows);
        VOGL_ASSERT(((void *)&result != (void *)&lhs) && ((void *)&result != (void *)&rhs));
        for (int r = 0; r < X::num_rows; r++)
            for (int c = 0; c < Y::num_cols; c++)
            {
                typename Z::scalar_type s = lhs(r, 0) * rhs(0, c);
                for (uint32_t i = 1; i < X::num_cols; i++)
                    s += lhs(r, i) * rhs(i, c);
                result(r, c) = s;
            }
        return result;
    }

    template <class X, class Y, class Z>
    Z &matrix_mul_helper_transpose_lhs(Z &result, const X &lhs, const Y &rhs)
    {
        VOGL_ASSUME(Z::num_rows == X::num_cols);
        VOGL_ASSUME(Z::num_cols == Y::num_cols);
        VOGL_ASSUME(X::num_rows == Y::num_rows);
        VOGL_ASSERT(((void *)&result != (void *)&lhs) && ((void *)&result != (void *)&rhs));
        for (int r = 0; r < X::num_cols; r++)
            for (int c = 0; c < Y::num_cols; c++)
            {
                typename Z::scalar_type s = lhs(0, r) * rhs(0, c);
                for (uint32_t i = 1; i < X::num_rows; i++)
                    s += lhs(i, r) * rhs(i, c);
                result(r, c) = s;
            }
        return result;
    }

    template <class X, class Y, class Z>
    Z &matrix_mul_helper_transpose_rhs(Z &result, const X &lhs, const Y &rhs)
    {
        VOGL_ASSUME(Z::num_rows == X::num_rows);
        VOGL_ASSUME(Z::num_cols == Y::num_rows);
        VOGL_ASSUME(X::num_cols == Y::num_cols);
        VOGL_ASSERT(((void *)&result != (void *)&lhs) && ((void *)&result != (void *)&rhs));
        for (int r = 0; r < X::num_rows; r++)
            for (int c = 0; c < Y::num_rows; c++)
            {
                typename Z::scalar_type s = lhs(r, 0) * rhs(c, 0);
                for (uint32_t i = 1; i < X::num_cols; i++)
                    s += lhs(r, i) * rhs(c, i);
                result(r, c) = s;
            }
        return result;
    }

    enum eIdentity
    {
        cIdentity
    };

    template <uint32_t R, uint32_t C, typename T>
    class matrix
    {
    public:
        typedef T scalar_type;
        enum
        {
            num_rows = R,
            num_cols = C
        };

        typedef vec<R, T> col_vec;
        typedef vec < (R > 1) ? (R - 1) : 0, T > subcol_vec;

        typedef vec<C, T> row_vec;
        typedef vec < (C > 1) ? (C - 1) : 0, T > subrow_vec;

        inline matrix()
        {
        }

        inline matrix(eClear)
        {
            clear();
        }

        inline matrix(eIdentity)
        {
            set_identity_matrix();
        }

        inline matrix(const T *p)
        {
            set(p);
        }

        inline matrix(const matrix &other)
        {
            for (uint32_t i = 0; i < R; i++)
                m_rows[i] = other.m_rows[i];
        }

        inline matrix &operator=(const matrix &rhs)
        {
            if (this != &rhs)
                for (uint32_t i = 0; i < R; i++)
                    m_rows[i] = rhs.m_rows[i];
            return *this;
        }

        inline matrix(T val00, T val01,
                      T val10, T val11)
        {
            set(val00, val01, val10, val11);
        }

        inline matrix(T val00, T val01,
                      T val10, T val11,
                      T val20, T val21)
        {
            set(val00, val01, val10, val11, val20, val21);
        }

        inline matrix(T val00, T val01, T val02,
                      T val10, T val11, T val12,
                      T val20, T val21, T val22)
        {
            set(val00, val01, val02, val10, val11, val12, val20, val21, val22);
        }

        inline matrix(T val00, T val01, T val02, T val03,
                      T val10, T val11, T val12, T val13,
                      T val20, T val21, T val22, T val23,
                      T val30, T val31, T val32, T val33)
        {
            set(val00, val01, val02, val03, val10, val11, val12, val13, val20, val21, val22, val23, val30, val31, val32, val33);
        }

        inline matrix(T val00, T val01, T val02, T val03,
                      T val10, T val11, T val12, T val13,
                      T val20, T val21, T val22, T val23)
        {
            set(val00, val01, val02, val03, val10, val11, val12, val13, val20, val21, val22, val23);
        }

        inline void set(const float *p)
        {
            for (uint32_t i = 0; i < R; i++)
            {
                m_rows[i].set(p);
                p += C;
            }
        }

        inline void set(T val00, T val01,
                        T val10, T val11)
        {
            m_rows[0].set(val00, val01);
            if (R >= 2)
            {
                m_rows[1].set(val10, val11);

                for (uint32_t i = 2; i < R; i++)
                    m_rows[i].clear();
            }
        }

        inline void set(T val00, T val01,
                        T val10, T val11,
                        T val20, T val21)
        {
            m_rows[0].set(val00, val01);
            if (R >= 2)
            {
                m_rows[1].set(val10, val11);

                if (R >= 3)
                {
                    m_rows[2].set(val20, val21);

                    for (uint32_t i = 3; i < R; i++)
                        m_rows[i].clear();
                }
            }
        }

        inline void set(T val00, T val01, T val02,
                        T val10, T val11, T val12,
                        T val20, T val21, T val22)
        {
            m_rows[0].set(val00, val01, val02);
            if (R >= 2)
            {
                m_rows[1].set(val10, val11, val12);
                if (R >= 3)
                {
                    m_rows[2].set(val20, val21, val22);

                    for (uint32_t i = 3; i < R; i++)
                        m_rows[i].clear();
                }
            }
        }

        inline void set(T val00, T val01, T val02, T val03,
                        T val10, T val11, T val12, T val13,
                        T val20, T val21, T val22, T val23,
                        T val30, T val31, T val32, T val33)
        {
            m_rows[0].set(val00, val01, val02, val03);
            if (R >= 2)
            {
                m_rows[1].set(val10, val11, val12, val13);
                if (R >= 3)
                {
                    m_rows[2].set(val20, val21, val22, val23);

                    if (R >= 4)
                    {
                        m_rows[3].set(val30, val31, val32, val33);

                        for (uint32_t i = 4; i < R; i++)
                            m_rows[i].clear();
                    }
                }
            }
        }

        inline void set(T val00, T val01, T val02, T val03,
                        T val10, T val11, T val12, T val13,
                        T val20, T val21, T val22, T val23)
        {
            m_rows[0].set(val00, val01, val02, val03);
            if (R >= 2)
            {
                m_rows[1].set(val10, val11, val12, val13);
                if (R >= 3)
                {
                    m_rows[2].set(val20, val21, val22, val23);

                    for (uint32_t i = 3; i < R; i++)
                        m_rows[i].clear();
                }
            }
        }

        inline T operator()(uint32_t r, uint32_t c) const
        {
            VOGL_ASSERT((r < R) && (c < C));
            return m_rows[r][c];
        }

        inline T &operator()(uint32_t r, uint32_t c)
        {
            VOGL_ASSERT((r < R) && (c < C));
            return m_rows[r][c];
        }

        inline const row_vec &operator[](uint32_t r) const
        {
            VOGL_ASSERT(r < R);
            return m_rows[r];
        }

        inline row_vec &operator[](uint32_t r)
        {
            VOGL_ASSERT(r < R);
            return m_rows[r];
        }

        inline const row_vec &get_row(uint32_t r) const
        {
            return (*this)[r];
        }
        inline row_vec &get_row(uint32_t r)
        {
            return (*this)[r];
        }

        inline col_vec get_col(uint32_t c) const
        {
            VOGL_ASSERT(c < C);
            col_vec result;
            for (uint32_t i = 0; i < R; i++)
                result[i] = m_rows[i][c];
            return result;
        }

        inline void set_col(uint32_t c, const col_vec &col)
        {
            VOGL_ASSERT(c < C);
            for (uint32_t i = 0; i < R; i++)
                m_rows[i][c] = col[i];
        }

        inline void set_col(uint32_t c, const subcol_vec &col)
        {
            VOGL_ASSERT(c < C);
            for (uint32_t i = 0; i < (R - 1); i++)
                m_rows[i][c] = col[i];

            m_rows[R - 1][c] = 0.0f;
        }

        inline const row_vec &get_translate() const
        {
            return m_rows[R - 1];
        }

        inline matrix &set_translate(const row_vec &r)
        {
            m_rows[R - 1] = r;
            return *this;
        }

        inline matrix &set_translate(const subrow_vec &r)
        {
            m_rows[R - 1] = row_vec(r).as_point();
            return *this;
        }

        inline const T *get_ptr() const
        {
            return reinterpret_cast<const T *>(&m_rows[0]);
        }
        inline T *get_ptr()
        {
            return reinterpret_cast<T *>(&m_rows[0]);
        }

        inline matrix &operator+=(const matrix &other)
        {
            for (uint32_t i = 0; i < R; i++)
                m_rows[i] += other.m_rows[i];
            return *this;
        }

        inline matrix &operator-=(const matrix &other)
        {
            for (uint32_t i = 0; i < R; i++)
                m_rows[i] -= other.m_rows[i];
            return *this;
        }

        inline matrix &operator*=(T val)
        {
            for (uint32_t i = 0; i < R; i++)
                m_rows[i] *= val;
            return *this;
        }

        inline matrix &operator/=(T val)
        {
            for (uint32_t i = 0; i < R; i++)
                m_rows[i] /= val;
            return *this;
        }

        inline matrix &operator*=(const matrix &other)
        {
            matrix result;
            matrix_mul_helper(result, *this, other);
            *this = result;
            return *this;
        }

        friend inline matrix operator+(const matrix &lhs, const matrix &rhs)
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                result[i] = lhs.m_rows[i] + rhs.m_rows[i];
            return result;
        }

        friend inline matrix operator-(const matrix &lhs, const matrix &rhs)
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                result[i] = lhs.m_rows[i] - rhs.m_rows[i];
            return result;
        }

        friend inline matrix operator*(const matrix &lhs, T val)
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                result[i] = lhs.m_rows[i] * val;
            return result;
        }

        friend inline matrix operator/(const matrix &lhs, T val)
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                result[i] = lhs.m_rows[i] / val;
            return result;
        }

        friend inline matrix operator*(T val, const matrix &rhs)
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                result[i] = val * rhs.m_rows[i];
            return result;
        }

        friend inline matrix operator*(const matrix &lhs, const matrix &rhs)
        {
            matrix result;
            return matrix_mul_helper(result, lhs, rhs);
        }

        friend inline row_vec operator*(const col_vec &a, const matrix &b)
        {
            return transform(a, b);
        }

        inline matrix operator+() const
        {
            return *this;
        }

        inline matrix operator-() const
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                result[i] = -m_rows[i];
            return result;
        }

        inline matrix &clear()
        {
            for (uint32_t i = 0; i < R; i++)
                m_rows[i].clear();
            return *this;
        }

        inline matrix &set_zero_matrix()
        {
            clear();
            return *this;
        }

        inline matrix &set_identity_matrix()
        {
            for (uint32_t i = 0; i < R; i++)
            {
                m_rows[i].clear();
                m_rows[i][i] = 1.0f;
            }
            return *this;
        }

        inline matrix &set_scale_matrix(float s)
        {
            clear();
            for (int i = 0; i < (R - 1); i++)
                m_rows[i][i] = s;
            m_rows[R - 1][C - 1] = 1.0f;
            return *this;
        }

        inline matrix &set_scale_matrix(const row_vec &s)
        {
            clear();
            for (uint32_t i = 0; i < R; i++)
                m_rows[i][i] = s[i];
            return *this;
        }

        inline matrix &set_scale_matrix(float x, float y)
        {
            set_identity_matrix();
            m_rows[0].set_x(x);
            m_rows[1].set_y(y);
            return *this;
        }

        inline matrix &set_scale_matrix(float x, float y, float z)
        {
            set_identity_matrix();
            m_rows[0].set_x(x);
            m_rows[1].set_y(y);
            m_rows[2].set_z(z);
            return *this;
        }

        inline matrix &set_translate_matrix(const row_vec &s)
        {
            set_identity_matrix();
            set_translate(s);
            return *this;
        }

        inline matrix &set_translate_matrix(float x, float y)
        {
            set_identity_matrix();
            set_translate(row_vec(x, y).as_point());
            return *this;
        }

        inline matrix &set_translate_matrix(float x, float y, float z)
        {
            set_identity_matrix();
            set_translate(row_vec(x, y, z).as_point());
            return *this;
        }

        inline matrix get_transposed() const
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                for (uint32_t j = 0; j < C; j++)
                    result.m_rows[i][j] = m_rows[j][i];
            return result;
        }

        inline matrix &transpose_in_place()
        {
            matrix result;
            for (uint32_t i = 0; i < R; i++)
                for (uint32_t j = 0; j < C; j++)
                    result.m_rows[i][j] = m_rows[j][i];
            *this = result;
            return *this;
        }

        bool invert(matrix &result) const
        {
            VOGL_ASSUME(R == C);

            result.set_identity_matrix();

            matrix mat(*this);

            for (uint32_t c = 0; c < C; c++)
            {
                uint32_t max_r = c;
                for (uint32_t r = c + 1; r < R; r++)
                    if (fabs(mat[r][c]) > fabs(mat[max_r][c]))
                        max_r = r;

                if (mat[max_r][c] == 0.0f)
                {
                    result.set_identity_matrix();
                    return false;
                }

                utils::swap(mat[c], mat[max_r]);
                utils::swap(result[c], result[max_r]);

                result[c] /= mat[c][c];
                mat[c] /= mat[c][c];

                for (uint32_t row = 0; row < R; row++)
                {
                    if (row != c)
                    {
                        const row_vec temp(mat[row][c]);
                        mat[row] -= row_vec::mul_components(mat[c], temp);
                        result[row] -= row_vec::mul_components(result[c], temp);
                    }
                }
            }

            return true;
        }

        matrix &invert_in_place()
        {
            matrix result;
            invert(result);
            *this = result;
            return *this;
        }

        matrix get_inverse() const
        {
            matrix result;
            invert(result);
            return result;
        }

        T get_det() const
        {
            VOGL_ASSERT(R == C);
            return det_helper(*this, R);
        }

        bool equal_tol(const matrix &b, float tol) const
        {
            for (uint32_t r = 0; r < R; r++)
                if (!row_vec::equal_tol(m_rows[r], b.m_rows[r], tol))
                    return false;
            return true;
        }

        // This method transforms a vec by a matrix (D3D-style: row vector on left).
        // Confusingly, note that the data type is named "col_vec", but mathematically it's actually written as a row vector (of size equal to the # matrix rows, which is why it's called a "col_vec" in this class).
        // 1xR * RxC = 1xC
        // This dots against the matrix columns.
        static inline row_vec transform(const col_vec &a, const matrix &b)
        {
            row_vec result(b[0] * a[0]);
            for (uint32_t r = 1; r < R; r++)
                result += b[r] * a[r];
            return result;
        }

        // This method transforms a vec by a matrix (D3D-style: row vector on left).
        // Last component of vec is assumed to be 1.
        static inline row_vec transform_point(const col_vec &a, const matrix &b)
        {
            row_vec result(0);
            for (int r = 0; r < (R - 1); r++)
                result += b[r] * a[r];
            result += b[R - 1];
            return result;
        }

        // This method transforms a vec by a matrix (D3D-style: row vector on left).
        // Last component of vec is assumed to be 0.
        static inline row_vec transform_vector(const col_vec &a, const matrix &b)
        {
            row_vec result(0);
            for (int r = 0; r < (R - 1); r++)
                result += b[r] * a[r];
            return result;
        }

        // This method transforms a vec by a matrix (D3D-style: row vector on left).
        // Last component of vec is assumed to be 1.
        static inline subcol_vec transform_point(const subcol_vec &a, const matrix &b)
        {
            subcol_vec result(0);
            for (int r = 0; r < static_cast<int>(R); r++)
            {
                const T s = (r < subcol_vec::num_elements) ? a[r] : 1.0f;
                for (int c = 0; c < static_cast<int>(C - 1); c++)
                    result[c] += b[r][c] * s;
            }
            return result;
        }

        // This method transforms a vec by a matrix (D3D-style: row vector on left).
        // Last component of vec is assumed to be 0.
        static inline subcol_vec transform_vector(const subcol_vec &a, const matrix &b)
        {
            subcol_vec result(0);
            for (int r = 0; r < static_cast<int>(R - 1); r++)
            {
                const T s = a[r];
                for (int c = 0; c < static_cast<int>(C - 1); c++)
                    result[c] += b[r][c] * s;
            }
            return result;
        }

        // Like transform() above, but the matrix is effectively transposed before the multiply.
        static inline col_vec transform_transposed(const col_vec &a, const matrix &b)
        {
            VOGL_ASSUME(R == C);
            col_vec result;
            for (uint32_t r = 0; r < R; r++)
                result[r] = b[r].dot(a);
            return result;
        }

        // Like transform() above, but the matrix is effectively transposed before the multiply.
        // Last component of vec is assumed to be 0.
        static inline col_vec transform_vector_transposed(const col_vec &a, const matrix &b)
        {
            VOGL_ASSUME(R == C);
            col_vec result;
            for (uint32_t r = 0; r < R; r++)
            {
                T s = 0;
                for (uint32_t c = 0; c < (C - 1); c++)
                    s += b[r][c] * a[c];

                result[r] = s;
            }
            return result;
        }

        // This method transforms a vec by a matrix (D3D-style: row vector on left), but the matrix is effectively transposed before the multiply.
        // Last component of vec is assumed to be 1.
        static inline subcol_vec transform_point_transposed(const subcol_vec &a, const matrix &b)
        {
            VOGL_ASSUME(R == C);
            subcol_vec result(0);
            for (int r = 0; r < R; r++)
            {
                const T s = (r < subcol_vec::num_elements) ? a[r] : 1.0f;
                for (int c = 0; c < (C - 1); c++)
                    result[c] += b[c][r] * s;
            }
            return result;
        }

        // This method transforms a vec by a matrix (D3D-style: row vector on left), but the matrix is effectively transposed before the multiply.
        // Last component of vec is assumed to be 0.
        static inline subcol_vec transform_vector_transposed(const subcol_vec &a, const matrix &b)
        {
            VOGL_ASSUME(R == C);
            subcol_vec result(0);
            for (int r = 0; r < static_cast<int>(R - 1); r++)
            {
                const T s = a[r];
                for (int c = 0; c < static_cast<int>(C - 1); c++)
                    result[c] += b[c][r] * s;
            }
            return result;
        }

        // This method transforms a matrix by a vector (OGL style, col vector on the right).
        // Note that the data type is named "row_vec", but mathematically it's actually written as a column vector (of size equal to the # matrix cols).
        // RxC * Cx1 = Rx1
        // This dots against the matrix rows.
        static inline col_vec transform(const matrix &b, const row_vec &a)
        {
            col_vec result;
            for (int r = 0; r < static_cast<int>(R); r++)
                result[r] = b[r].dot(a);
            return result;
        }

        // This method transforms a matrix by a vector (OGL style, col vector on the right), except the matrix is effectively transposed before the multiply.
        // Note that the data type is named "row_vec", but mathematically it's actually written as a column vector (of size equal to the # matrix cols).
        // RxC * Cx1 = Rx1
        // This dots against the matrix cols.
        static inline col_vec transform_transposed(const matrix &b, const row_vec &a)
        {
            VOGL_ASSUME(R == C);
            row_vec result(b[0] * a[0]);
            for (int r = 1; r < static_cast<int>(R); r++)
                result += b[r] * a[r];
            return col_vec(result);
        }

        static inline matrix &mul_components(matrix &result, const matrix &lhs, const matrix &rhs)
        {
            for (uint32_t r = 0; r < R; r++)
                result[r] = row_vec::mul_components(lhs[r], rhs[r]);
            return result;
        }

        static inline matrix &concat(matrix &lhs, const matrix &rhs)
        {
            return matrix_mul_helper(lhs, matrix(lhs), rhs);
        }

        inline matrix &concat_in_place(const matrix &rhs)
        {
            return concat(*this, rhs);
        }

        static inline matrix &multiply(matrix &result, const matrix &lhs, const matrix &rhs)
        {
            matrix temp;
            matrix *pResult = ((&result == &lhs) || (&result == &rhs)) ? &temp : &result;

            matrix_mul_helper(*pResult, lhs, rhs);
            if (pResult != &result)
                result = *pResult;

            return result;
        }

        static matrix make_zero_matrix()
        {
            matrix result;
            result.clear();
            return result;
        }

        static matrix make_identity_matrix()
        {
            matrix result;
            result.set_identity_matrix();
            return result;
        }

        static matrix make_translate_matrix(const row_vec &t)
        {
            return matrix(cIdentity).set_translate(t);
        }

        static matrix make_translate_matrix(float x, float y)
        {
            return matrix(cIdentity).set_translate_matrix(x, y);
        }

        static matrix make_translate_matrix(float x, float y, float z)
        {
            return matrix(cIdentity).set_translate_matrix(x, y, z);
        }

        static inline matrix make_scale_matrix(float s)
        {
            return matrix().set_scale_matrix(s);
        }

        static inline matrix make_scale_matrix(const row_vec &s)
        {
            return matrix().set_scale_matrix(s);
        }

        static inline matrix make_scale_matrix(float x, float y)
        {
            VOGL_ASSUME(R >= 3 && C >= 3);
            matrix result;
            result.set_identity_matrix();
            result.m_rows[0][0] = x;
            result.m_rows[1][1] = y;
            return result;
        }

        static inline matrix make_scale_matrix(float x, float y, float z)
        {
            VOGL_ASSUME(R >= 4 && C >= 4);
            matrix result;
            result.set_identity_matrix();
            result.m_rows[0][0] = x;
            result.m_rows[1][1] = y;
            result.m_rows[2][2] = z;
            return result;
        }

        // Helpers derived from Graphics Gems 1 and 2 (Matrices and Transformations, Ronald N. Goldman)
        static matrix make_rotate_matrix(const vec<3, T> &axis, T ang)
        {
            VOGL_ASSUME(R >= 3 && C >= 3);

            vec<3, T> norm_axis(axis.get_normalized());

            double cos_a = cos(ang);
            double inv_cos_a = 1.0f - cos_a;

            double sin_a = sin(ang);

            const T x = norm_axis[0];
            const T y = norm_axis[1];
            const T z = norm_axis[2];

            const double x2 = norm_axis[0] * norm_axis[0];
            const double y2 = norm_axis[1] * norm_axis[1];
            const double z2 = norm_axis[2] * norm_axis[2];

            matrix result;
            result.set_identity_matrix();

            result[0][0] = (T)((inv_cos_a * x2) + cos_a);
            result[1][0] = (T)((inv_cos_a * x * y) + (sin_a * z));
            result[2][0] = (T)((inv_cos_a * x * z) - (sin_a * y));

            result[0][1] = (T)((inv_cos_a * x * y) - (sin_a * z));
            result[1][1] = (T)((inv_cos_a * y2) + cos_a);
            result[2][1] = (T)((inv_cos_a * y * z) + (sin_a * x));

            result[0][2] = (T)((inv_cos_a * x * z) + (sin_a * y));
            result[1][2] = (T)((inv_cos_a * y * z) - (sin_a * x));
            result[2][2] = (T)((inv_cos_a * z2) + cos_a);

            return result;
        }

        static inline matrix make_rotate_matrix(T ang)
        {
            VOGL_ASSUME(R >= 2 && C >= 2);

            matrix ret(cIdentity);

            const T sin_a = static_cast<T>(sin(ang));
            const T cos_a = static_cast<T>(cos(ang));

            ret[0][0] = +cos_a;
            ret[0][1] = -sin_a;
            ret[1][0] = +sin_a;
            ret[1][1] = +cos_a;

            return ret;
        }

        static inline matrix make_rotate_matrix(uint32_t axis, T ang)
        {
            vec<3, T> axis_vec;
            axis_vec.clear();
            axis_vec[axis] = 1.0f;
            return make_rotate_matrix(axis_vec, ang);
        }

        static inline matrix make_cross_product_matrix(const vec<3, scalar_type> &c)
        {
            VOGL_ASSUME((num_rows >= 3) && (num_cols >= 3));
            matrix ret(cClear);
            ret[0][1] = c[2];
            ret[0][2] = -c[1];
            ret[1][0] = -c[2];
            ret[1][2] = c[0];
            ret[2][0] = c[1];
            ret[2][1] = -c[0];
            return ret;
        }

        static inline matrix make_reflection_matrix(const vec<4, scalar_type> &n, const vec<4, scalar_type> &q)
        {
            VOGL_ASSUME((num_rows == 4) && (num_cols == 4));
            matrix ret;
            VOGL_ASSERT(n.is_vector() && q.is_vector());
            ret = make_identity_matrix() - 2.0f * make_tensor_product_matrix(n, n);
            ret.set_translate((2.0f * q.dot(n) * n).as_point());
            return ret;
        }

        static inline matrix make_tensor_product_matrix(const row_vec &v, const row_vec &w)
        {
            matrix ret;
            for (int r = 0; r < num_rows; r++)
                ret[r] = row_vec::mul_components(v.broadcast(r), w);
            return ret;
        }

        static inline matrix make_uniform_scaling_matrix(const vec<4, scalar_type> &q, scalar_type c)
        {
            VOGL_ASSUME((num_rows == 4) && (num_cols == 4));
            VOGL_ASSERT(q.is_vector());
            matrix ret;
            ret = c * make_identity_matrix();
            ret.set_translate(((1.0f - c) * q).as_point());
            return ret;
        }

        static inline matrix make_nonuniform_scaling_matrix(const vec<4, scalar_type> &q, scalar_type c, const vec<4, scalar_type> &w)
        {
            VOGL_ASSUME((num_rows == 4) && (num_cols == 4));
            VOGL_ASSERT(q.is_vector() && w.is_vector());
            matrix ret;
            ret = make_identity_matrix() - (1.0f - c) * make_tensor_product_matrix(w, w);
            ret.set_translate(((1.0f - c) * q.dot(w) * w).as_point());
            return ret;
        }

        // n = normal of plane, q = point on plane
        static inline matrix make_ortho_projection_matrix(const vec<4, scalar_type> &n, const vec<4, scalar_type> &q)
        {
            VOGL_ASSERT(n.is_vector() && q.is_vector());
            matrix ret;
            ret = make_identity_matrix() - make_tensor_product_matrix(n, n);
            ret.set_translate((q.dot(n) * n).as_point());
            return ret;
        }

        static inline matrix make_parallel_projection(const vec<4, scalar_type> &n, const vec<4, scalar_type> &q, const vec<4, scalar_type> &w)
        {
            VOGL_ASSERT(n.is_vector() && q.is_vector() && w.is_vector());
            matrix ret;
            ret = make_identity_matrix() - (make_tensor_product_matrix(n, w) / (w.dot(n)));
            ret.set_translate(((q.dot(n) / w.dot(n)) * w).as_point());
            return ret;
        }

    protected:
        row_vec m_rows[R];

        static T det_helper(const matrix &a, uint32_t n)
        {
            // Algorithm ported from Numerical Recipes in C.
            T d;
            matrix m;
            if (n == 2)
                d = a(0, 0) * a(1, 1) - a(1, 0) * a(0, 1);
            else
            {
                d = 0;
                for (uint32_t j1 = 1; j1 <= n; j1++)
                {
                    for (uint32_t i = 2; i <= n; i++)
                    {
                        int j2 = 1;
                        for (uint32_t j = 1; j <= n; j++)
                        {
                            if (j != j1)
                            {
                                m(i - 2, j2 - 1) = a(i - 1, j - 1);
                                j2++;
                            }
                        }
                    }
                    d += (((1 + j1) & 1) ? -1.0f : 1.0f) * a(1 - 1, j1 - 1) * det_helper(m, n - 1);
                }
            }
            return d;
        }
    };

    typedef matrix<2, 2, float> matrix22F;
    typedef matrix<2, 2, double> matrix22D;

    typedef matrix<3, 3, float> matrix33F;
    typedef matrix<3, 3, double> matrix33D;

    typedef matrix<4, 4, float> matrix44F;
    typedef matrix<4, 4, double> matrix44D;

    typedef matrix<8, 8, float> matrix88F;

    // These helpers create good old D3D-style matrices.
    inline matrix44F matrix44F_make_perspective_offcenter_lh(float l, float r, float b, float t, float nz, float fz)
    {
        float two_nz = 2.0f * nz;
        float one_over_width = 1.0f / (r - l);
        float one_over_height = 1.0f / (t - b);

        matrix44F view_to_proj;
        view_to_proj[0].set(two_nz * one_over_width, 0.0f, 0.0f, 0.0f);
        view_to_proj[1].set(0.0f, two_nz * one_over_height, 0.0f, 0.0f);
        view_to_proj[2].set(-(l + r) * one_over_width, -(t + b) * one_over_height, fz / (fz - nz), 1.0f);
        view_to_proj[3].set(0.0f, 0.0f, -view_to_proj[2][2] * nz, 0.0f);
        return view_to_proj;
    }

    // fov_y: full Y field of view (radians)
    // aspect: viewspace width/height
    inline matrix44F matrix44F_make_perspective_fov_lh(float fov_y, float aspect, float nz, float fz)
    {
        double sin_fov = sin(0.5f * fov_y);
        double cos_fov = cos(0.5f * fov_y);

        float y_scale = static_cast<float>(cos_fov / sin_fov);
        float x_scale = static_cast<float>(y_scale / aspect);

        matrix44F view_to_proj;
        view_to_proj[0].set(x_scale, 0, 0, 0);
        view_to_proj[1].set(0, y_scale, 0, 0);
        view_to_proj[2].set(0, 0, fz / (fz - nz), 1);
        view_to_proj[3].set(0, 0, -nz * fz / (fz - nz), 0);
        return view_to_proj;
    }

    inline matrix44F matrix44F_make_ortho_offcenter_lh(float l, float r, float b, float t, float nz, float fz)
    {
        matrix44F view_to_proj;
        view_to_proj[0].set(2.0f / (r - l), 0.0f, 0.0f, 0.0f);
        view_to_proj[1].set(0.0f, 2.0f / (t - b), 0.0f, 0.0f);
        view_to_proj[2].set(0.0f, 0.0f, 1.0f / (fz - nz), 0.0f);
        view_to_proj[3].set((l + r) / (l - r), (t + b) / (b - t), nz / (nz - fz), 1.0f);
        return view_to_proj;
    }

    inline matrix44F matrix44F_make_ortho_lh(float w, float h, float nz, float fz)
    {
        return matrix44F_make_ortho_offcenter_lh(-w * .5f, w * .5f, -h * .5f, h * .5f, nz, fz);
    }

    inline matrix44F matrix44F_make_projection_to_screen_d3d(int x, int y, int w, int h, float min_z, float max_z)
    {
        matrix44F proj_to_screen;
        proj_to_screen[0].set(w * .5f, 0.0f, 0.0f, 0.0f);
        proj_to_screen[1].set(0, h * -.5f, 0.0f, 0.0f);
        proj_to_screen[2].set(0, 0.0f, max_z - min_z, 0.0f);
        proj_to_screen[3].set(x + w * .5f, y + h * .5f, min_z, 1.0f);
        return proj_to_screen;
    }

    inline matrix44F matrix44F_make_lookat_lh(const vec3F &camera_pos, const vec3F &look_at, const vec3F &camera_up, float camera_roll_ang_in_radians)
    {
        vec4F col2(look_at - camera_pos);
        VOGL_ASSERT(col2.is_vector());
        if (col2.normalize() == 0.0f)
            col2.set(0, 0, 1, 0);

        vec4F col1(camera_up);
        VOGL_ASSERT(col1.is_vector());
        if (!col2[0] && !col2[2])
            col1.set(-1.0f, 0.0f, 0.0f, 0.0f);

        if ((col1.dot(col2)) > .9999f)
            col1.set(0.0f, 1.0f, 0.0f, 0.0f);

        vec4F col0(vec4F::cross3(col1, col2).normalize_in_place());
        col1 = vec4F::cross3(col2, col0).normalize_in_place();

        matrix44F rotm(matrix44F::make_identity_matrix());
        rotm.set_col(0, col0);
        rotm.set_col(1, col1);
        rotm.set_col(2, col2);
        return matrix44F::make_translate_matrix(-camera_pos[0], -camera_pos[1], -camera_pos[2]) * rotm * matrix44F::make_rotate_matrix(2, camera_roll_ang_in_radians);
    }

} // namespace vogl
