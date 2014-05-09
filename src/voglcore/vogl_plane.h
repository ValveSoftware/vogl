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

// File: vogl_plane.h
#pragma once

#include "vogl_core.h"
#include "vogl_matrix.h"

namespace vogl
{
    class oriented_plane;

    class plane
    {
    public:
        float m_dist;
        vec3F m_normal;

        inline plane()
        {
        }

        inline plane(const vec3F &norm, float dist)
            : m_dist(dist), m_normal(norm)
        {
        }

        inline plane(const vec4F &norm, float dist)
            : m_dist(dist), m_normal(norm)
        {
        }

        inline plane(float nx, float ny, float nz, float dist)
            : m_dist(dist), m_normal(nx, ny, nz)
        {
        }

        inline plane(const vec3F &norm, const vec3F &org)
            : m_dist(norm.dot(org)), m_normal(norm)
        {
        }

        inline plane(const vec3F &p, const vec3F &q, const vec3F &r)
        {
            init(p, q, r);
        }

        inline plane(const vec4F &equation)
            : m_dist(-equation[3]), m_normal(equation)
        {
        }

        inline plane(const oriented_plane &op)
        {
            init(op);
        }

        inline bool is_valid() const
        {
            return (m_normal[0] != 0.0f) || (m_normal[1] != 0.0f) || (m_normal[2] != 0.0f);
        }

        inline plane &clear()
        {
            m_normal.clear();
            m_dist = 0;
            return *this;
        }

        inline const vec3F &get_normal() const
        {
            return m_normal;
        }

        inline vec3F &get_normal()
        {
            return m_normal;
        }

        inline const float &get_distance() const
        {
            return m_dist;
        }

        inline float &get_distance()
        {
            return m_dist;
        }

        inline vec4F get_equation() const
        {
            return vec4F(m_normal[0], m_normal[1], m_normal[2], -m_dist);
        }

        inline vec3F get_point_on_plane() const
        {
            return m_normal * m_dist;
        }

        inline const plane &init(float nx, float ny, float nz, float dist)
        {
            m_normal.set(nx, ny, nz);
            m_dist = dist;
            return *this;
        }

        inline const plane &init(const vec4F &eq)
        {
            m_normal = eq;
            m_dist = -eq[3];
            return *this;
        }

        inline bool init(const vec3F &p, const vec3F &q, const vec3F &r, float epsilon = 1e-6f)
        {
            m_normal = vec3F::cross3((r - q), (p - q));

            double normal_len2 = m_normal.norm();
            if (normal_len2 <= epsilon)
            {
                clear();
                return false;
            }

            m_normal *= static_cast<float>(1.0f / sqrt(normal_len2));
            m_dist = m_normal.dot(q);
            return true;
        }

        inline void init_denormalized(const vec3F &p, const vec3F &q, const vec3F &r)
        {
            m_normal = vec3F::cross3(r - q, p - q);
            m_dist = m_normal.dot(q);
        }

        inline plane &init(const vec3F &norm, const vec3F &point_on_plane)
        {
            m_normal = norm;
            m_dist = norm.dot(point_on_plane);
            return *this;
        }

        inline plane &init(const oriented_plane &op);

        inline plane &normalize_in_place()
        {
            double normal_len = m_normal.length();
            if (normal_len == 0.0f)
                clear();
            else
            {
                double one_over_normal_len = 1.0f / normal_len;
                m_normal *= static_cast<float>(one_over_normal_len);
                m_dist *= static_cast<float>(one_over_normal_len);
            }
            return *this;
        }

        inline bool operator==(const plane &rhs) const
        {
            return (m_normal == rhs.m_normal) && (m_dist == rhs.m_dist);
        }

        inline bool operator!=(const plane &rhs) const
        {
            return !(*this == rhs);
        }

        inline float get_distance(const vec3F &p) const
        {
            return p.dot(m_normal) - m_dist;
        }

        inline plane get_flipped() const
        {
            return plane(-m_normal, -m_dist);
        }

        // computes how much solve_axis changes with movement along exclusively movement_axis
        inline float get_gradient(uint32_t solve_axis, uint32_t movement_axis, float epsilon = 1e-6f) const
        {
            const float dA = -m_normal[movement_axis];
            const float dB = m_normal[solve_axis];
            if (fabs(dB) <= epsilon)
            {
                VOGL_ASSERT(0.0f);
                return 0.0f;
            }
            return dA / dB;
        }

        inline plane get_transformed(const matrix44F &m)
        {
            matrix44F m_inverted;
            if (!m.invert(m_inverted))
            {
                VOGL_ASSERT_ALWAYS;
            }

            plane result;
            result.m_normal = matrix44F::transform_vector_transposed(m_normal, m_inverted).normalize_in_place();
            result.m_dist = matrix44F::transform_point(get_point_on_plane(), m).dot3(result.m_normal);
            return result;
        }

        inline plane get_transformed_ortho(const matrix44F &m)
        {
            plane result;
            result.m_normal = matrix44F::transform_vector(m_normal, m);
            result.m_dist = m_dist + vec3F(m[3]).dot(result.m_normal);
            return result;
        }

        inline vec3F project_to_plane(const vec3F &p) const
        {
            return p - m_normal * get_distance(p);
        }

        // computes z given xy
        inline float solve_for_z(float x, float y) const
        {
            return m_normal[2] ? ((m_dist - x * m_normal[0] - y * m_normal[1]) / m_normal[2]) : 0.0f;
        }

        // generalized for any axis
        inline float solve_for_axis(const vec3F &p, uint32_t axis) const
        {
            VOGL_ASSERT(axis < 3);
            static const uint8_t axis2[] = { 1, 0, 0 };
            static const uint8_t axis3[] = { 2, 2, 1 };
            return m_normal[axis] ? ((m_dist - p[axis2[axis]] * m_normal[axis2[axis]] - p[axis3[axis]] * m_normal[axis3[axis]]) / m_normal[axis]) : 0.0f;
        }
    };

    class oriented_plane
    {
    public:
        vec3F m_u, m_v;
        vec3F m_origin;

        inline oriented_plane()
        {
        }

        inline oriented_plane(const plane &p)
        {
            init(p);
        }

        inline oriented_plane(const vec3F &origon, const vec3F &u_axis, const vec3F &v_axis)
            : m_u(u_axis), m_v(v_axis), m_origin(origon)
        {
        }

        inline oriented_plane(const vec3F &origon, const vec3F &norm)
        {
            init(origon, norm);
        }

        inline void clear()
        {
            m_origin.clear();
            m_u.clear();
            m_v.clear();
        }

        inline const vec3F &get_origin() const
        {
            return m_origin;
        }
        inline vec3F &get_origin()
        {
            return m_origin;
        }

        inline const vec3F &get_u() const
        {
            return m_u;
        }
        inline vec3F &get_u()
        {
            return m_u;
        }

        inline const vec3F &get_v() const
        {
            return m_v;
        }
        inline vec3F &get_v()
        {
            return m_v;
        }

        inline oriented_plane &init(const vec3F &origon, const vec3F &u_axis, const vec3F &v_axis)
        {
            m_origin = origon;
            m_u = u_axis;
            m_v = v_axis;
            return *this;
        }

        inline oriented_plane &init(const plane &p)
        {
            m_origin = p.get_point_on_plane();
            m_u = vec3F::make_axis(p.m_normal.get_minor_axis()).remove_unit_direction(p.m_normal).normalize_in_place();
            m_v = vec3F::cross3(p.m_normal, m_u).normalize_in_place();
            return *this;
        }

        inline oriented_plane &init(const vec3F &origon, const vec3F &norm)
        {
            m_origin = origon;
            m_u = vec3F::make_axis(norm.get_minor_axis()).remove_unit_direction(norm).normalize_in_place();
            m_v = vec3F::cross3(norm, m_u).normalize_in_place();
            return *this;
        }

        inline vec2F get_projected(const vec3F &p) const
        {
            vec3F rel(p - m_origin);
            return vec2F(m_u.dot(rel), m_v.dot(rel));
        }

        inline vec3F get_point(const vec2F &p) const
        {
            return m_u * p[0] + m_v * p[1] + m_origin;
        }

        inline vec3F get_normal() const
        {
            return vec3F::cross3(m_u, m_v).normalize_in_place();
        }

        inline oriented_plane &normalize_in_place()
        {
            m_u.normalize();
            m_v.normalize();
            return *this;
        }

        inline oriented_plane &init_planar_projection(const plane &p)
        {
            const uint32_t major_axis = p.get_normal().get_major_axis();
            uint32_t axis0 = math::next_wrap<uint32_t>(major_axis, 3U);
            uint32_t axis1 = math::next_wrap<uint32_t>(axis0, 3U);
            if (p.get_normal()[major_axis] > 0.0f)
                std::swap(axis0, axis1);
            return init(vec3F(0), vec3F::make_axis(axis0), vec3F::make_axis(axis1));
        }
    };

    inline plane &plane::init(const oriented_plane &op)
    {
        m_normal = op.get_normal();
        m_dist = op.get_origin().dot(op.get_normal());
        return *this;
    }

    inline void clip_convex_polygon_against_plane(vogl::vector<vec3F> &result, const vogl::vector<vec3F> &src_poly, const plane &p)
    {
        result.resize(0);
        if (!src_poly.size())
            return;

        uint32_t prev_index = src_poly.size() - 1;
        float prev_dist = p.get_distance(src_poly[prev_index]);
        for (uint32_t cur_index = 0; cur_index < src_poly.size(); ++cur_index)
        {
            if (prev_dist >= 0.0f)
                result.push_back(src_poly[prev_index]);

            float cur_dist = p.get_distance(src_poly[cur_index]);
            if (((prev_dist < 0.0f) && (cur_dist > 0.0f)) || ((prev_dist > 0.0f) && (cur_dist < 0.0f)))
                result.push_back(vec3F::lerp(src_poly[prev_index], src_poly[cur_index], prev_dist / (prev_dist - cur_dist)));

            prev_index = cur_index;
            prev_dist = cur_dist;
        }
    }

    // This stuff was derived from the plane equation for a 3D triangle, with XY in screenspace and Z the component to interpolate:
    //compute denormalized plane normal/distance:
    //m_normal = vec3F::cross3(r - q, p - q);
    //m_dist = m_normal * q;
    //return vec<3, T>(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
    struct tri_gradient_common
    {
        double m_qx, m_qy;
        double m_ax, m_ay, m_bx, m_by;

        double m_norm_z;
        double m_one_over_norm_z;

        inline void clear()
        {
            utils::zero_object(*this);
        }

        // q is treated as the origin
        // vector_type must be at least 2 dimension, xy can be in screenspace (for example), component to be interpolated is treated as z axis.
        template <typename vector_type>
        inline void init(const vector_type &p, const vector_type &q, const vector_type &r)
        {
            m_qx = q[0];
            m_qy = q[1];

            m_ax = r[0] - q[0];
            m_ay = r[1] - q[1];

            m_bx = p[0] - q[0];
            m_by = p[1] - q[1];

            m_norm_z = m_ax * m_by - m_ay * m_bx;
            m_one_over_norm_z = m_norm_z ? (1.0 / m_norm_z) : 0.0;
        }
    };

    template <typename scalar_type>
    struct tri_gradient_component
    {
        scalar_type m_qz;
        scalar_type m_norm_x, m_norm_y;
        scalar_type m_dist;

        inline void clear()
        {
            utils::zero_object(*this);
        }

        void init(const tri_gradient_common &common, float pz, float qz, float rz)
        {
            m_qz = qz;

            double az = rz - qz;
            double bz = pz - qz;

            m_norm_x = static_cast<scalar_type>(common.m_ay * bz - az * common.m_by);
            m_norm_y = static_cast<scalar_type>(az * common.m_bx - common.m_ax * bz);

            m_dist = static_cast<scalar_type>(m_norm_x * common.m_qx + m_norm_y * common.m_qy + common.m_norm_z * m_qz);
        }

        // returns denormalized plane
        inline plane get_plane(const tri_gradient_common &common) const
        {
            return plane(static_cast<float>(m_norm_x), static_cast<float>(m_norm_y), static_cast<float>(common.m_norm_z), static_cast<float>(m_dist));
        }

        // not accurate at float precision
        inline double solve_for_z(const tri_gradient_common &common, float x, float y) const
        {
            return (m_dist - x * m_norm_x - y * m_norm_y) * common.m_one_over_norm_z;
        }

        // more accurate at float precision
        inline double solve_for_z_alt(const tri_gradient_common &common, float x, float y) const
        {
            double dz_dx = get_dz_over_dx_gradient(common);
            double dz_dy = get_dz_over_dy_gradient(common);
            return m_qz + (x - common.m_qx) * dz_dx + (y - common.m_qy) * dz_dy;
        }

        inline double get_dz_over_dx_gradient(const tri_gradient_common &common) const
        {
            return -m_norm_x * common.m_one_over_norm_z;
        }

        inline double get_dz_over_dy_gradient(const tri_gradient_common &common) const
        {
            return -m_norm_y * common.m_one_over_norm_z;
        }
    };

    class plane2D
    {
    public:
        float m_dist;
        vec2F m_normal;

        inline plane2D()
        {
        }

        inline plane2D(const vec2F &norm, float dist)
            : m_dist(dist), m_normal(norm)
        {
        }

        inline plane2D(float nx, float ny, float dist)
            : m_dist(dist), m_normal(nx, ny)
        {
        }

        inline plane2D(const vec2F &p, const vec2F &q, float epsilon = 1e-6f)
        {
            init(p, q, epsilon);
        }

        inline plane2D(const vec3F &equation)
            : m_dist(-equation[2]), m_normal(equation)
        {
        }

        inline bool is_valid() const
        {
            return (m_normal[0] != 0.0f) || (m_normal[1] != 0.0f);
        }

        inline plane2D &clear()
        {
            m_normal.clear();
            m_dist = 0;
            return *this;
        }

        inline const vec2F &get_normal() const
        {
            return m_normal;
        }

        inline vec2F &get_normal()
        {
            return m_normal;
        }

        inline const float &get_distance() const
        {
            return m_dist;
        }

        inline float &get_distance()
        {
            return m_dist;
        }

        inline vec3F get_equation() const
        {
            return vec3F(m_normal[0], m_normal[1], -m_dist);
        }

        inline vec2F get_point_on_plane() const
        {
            return m_normal * m_dist;
        }

        inline const plane2D &init(float nx, float ny, float dist)
        {
            m_normal.set(nx, ny);
            m_dist = dist;
            return *this;
        }

        inline const plane2D &init(const vec3F &eq)
        {
            m_normal = eq;
            m_dist = -eq[2];
            return *this;
        }

        inline bool init(const vec2F &p, const vec2F &q, float epsilon = 1e-6f)
        {
            m_normal = (p - q).perp();

            double normal_len2 = m_normal.norm();
            if (normal_len2 <= epsilon)
            {
                clear();
                return false;
            }

            m_normal *= static_cast<float>(1.0f / sqrt(normal_len2));
            m_dist = m_normal.dot(q);
            return true;
        }

        inline void init_denormalized(const vec2F &p, const vec2F &q)
        {
            m_normal = (p - q).perp();
            m_dist = m_normal.dot(q);
        }

        inline plane2D &init_from_normal_point(const vec2F &norm, const vec2F &point_on_plane)
        {
            m_normal = norm;
            m_dist = norm.dot(point_on_plane);
            return *this;
        }

        inline plane2D &normalize_in_place()
        {
            double normal_len = m_normal.length();
            if (normal_len == 0.0f)
                clear();
            else
            {
                double one_over_normal_len = 1.0f / normal_len;
                m_normal *= static_cast<float>(one_over_normal_len);
                m_dist *= static_cast<float>(one_over_normal_len);
            }
            return *this;
        }

        inline bool operator==(const plane2D &rhs) const
        {
            return (m_normal == rhs.m_normal) && (m_dist == rhs.m_dist);
        }

        inline bool operator!=(const plane2D &rhs) const
        {
            return !(*this == rhs);
        }

        inline float get_distance(const vec2F &p) const
        {
            return p.dot(m_normal) - m_dist;
        }

        inline plane2D get_flipped() const
        {
            return plane2D(-m_normal, -m_dist);
        }

        // computes how much solve_axis changes with movement along exclusively movement_axis
        inline float get_gradient(uint32_t solve_axis, uint32_t movement_axis, float epsilon = 1e-6f) const
        {
            const float dA = -m_normal[movement_axis];
            const float dB = m_normal[solve_axis];
            if (fabs(dB) <= epsilon)
            {
                VOGL_ASSERT(0.0f);
                return 0.0f;
            }
            return dA / dB;
        }

        inline plane2D get_transformed(const matrix33F &m)
        {
            matrix33F m_inverted;
            if (!m.invert(m_inverted))
            {
                VOGL_ASSERT_ALWAYS;
            }

            plane2D result;
            result.m_normal = matrix33F::transform_vector_transposed(m_normal, m_inverted).normalize_in_place();
            result.m_dist = matrix33F::transform_point(get_point_on_plane(), m).dot2(result.m_normal);
            return result;
        }

        inline plane2D get_transformed_ortho(const matrix33F &m)
        {
            plane2D result;
            result.m_normal = matrix33F::transform_vector(m_normal, m);
            result.m_dist = m_dist + vec2F(m[2]).dot(result.m_normal);
            return result;
        }

        inline vec2F project_to_plane(const vec2F &p) const
        {
            return p - m_normal * get_distance(p);
        }

        inline float solve_for_axis(const vec2F &p, uint32_t axis) const
        {
            return m_normal[axis] ? ((m_dist - p[axis ^ 1] * m_normal[axis ^ 1]) / m_normal[axis]) : 0.0f;
        }

        inline float solve_for_x(float y) const
        {
            return m_normal[0] ? ((m_dist - y * m_normal[1]) / m_normal[0]) : 0.0f;
        }

        inline float solve_for_y(float x) const
        {
            return m_normal[1] ? ((m_dist - x * m_normal[0]) / m_normal[1]) : 0.0f;
        }
    };

} // namespace vogl
