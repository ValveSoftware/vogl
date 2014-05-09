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

// File: vogl_intersect.h
#pragma once

#include "vogl_core.h"
#include "vogl_ray.h"
#include "vogl_plane.h"

namespace vogl
{
    namespace intersection
    {
        enum result
        {
            cBackfacing = -1,
            cFailure = 0,
            cSuccess,
            cParallel,
            cInside,
        };

        // Returns cInside, cSuccess, or cFailure.
        // Algorithm: Graphics Gems 1
        template <typename vector_type, typename scalar_type, typename ray_type, typename aabb_type>
        inline result ray_aabb(vector_type &coord, scalar_type &t, const ray_type &ray, const aabb_type &box)
        {
            enum
            {
                cNumDim = vector_type::num_elements,
                cRight = 0,
                cLeft = 1,
                cMiddle = 2
            };

            bool inside = true;
            int quadrant[cNumDim];
            scalar_type candidate_plane[cNumDim];

            for (int i = 0; i < cNumDim; i++)
            {
                if (ray.get_origin()[i] < box[0][i])
                {
                    quadrant[i] = cLeft;
                    candidate_plane[i] = box[0][i];
                    inside = false;
                }
                else if (ray.get_origin()[i] > box[1][i])
                {
                    quadrant[i] = cRight;
                    candidate_plane[i] = box[1][i];
                    inside = false;
                }
                else
                {
                    quadrant[i] = cMiddle;
                }
            }

            if (inside)
            {
                coord = ray.get_origin();
                t = 0.0f;
                return cInside;
            }

            scalar_type max_t[cNumDim];
            for (int i = 0; i < cNumDim; i++)
            {
                if ((quadrant[i] != cMiddle) && (ray.get_direction()[i] != 0.0f))
                    max_t[i] = (candidate_plane[i] - ray.get_origin()[i]) / ray.get_direction()[i];
                else
                    max_t[i] = -1.0f;
            }

            int which_plane = 0;
            for (int i = 1; i < cNumDim; i++)
                if (max_t[which_plane] < max_t[i])
                    which_plane = i;

            if (max_t[which_plane] < 0.0f)
                return cFailure;

            for (int i = 0; i < cNumDim; i++)
            {
                if (i != which_plane)
                {
                    coord[i] = ray.get_origin()[i] + max_t[which_plane] * ray.get_direction()[i];

                    if ((coord[i] < box[0][i]) || (coord[i] > box[1][i]))
                        return cFailure;
                }
                else
                {
                    coord[i] = candidate_plane[i];
                }

                VOGL_ASSERT(coord[i] >= box[0][i] && coord[i] <= box[1][i]);
            }

            t = max_t[which_plane];
            return cSuccess;
        }

        template <typename vector_type, typename scalar_type, typename ray_type, typename aabb_type>
        inline result ray_aabb(bool &started_within, vector_type &coord, scalar_type &t, const ray_type &ray, const aabb_type &box)
        {
            if (!box.contains(ray.get_origin()))
            {
                started_within = false;
                return ray_aabb(coord, t, ray, box);
            }

            started_within = true;

            float diag_dist = box.diagonal_length() * 1.5f;
            ray_type outside_ray(ray.eval(diag_dist), -ray.get_direction());

            result res(ray_aabb(coord, t, outside_ray, box));
            if (res != cSuccess)
                return res;

            t = math::maximum(0.0f, diag_dist - t);
            return cSuccess;
        }

        template <typename ray_type, typename scalar_type>
        inline result ray_plane(scalar_type &result, uint32_t plane_axis, scalar_type positive_plane_dist, const ray_type &the_ray, bool front_only, scalar_type epsilon = 1e-6f)
        {
            const scalar_type dir_dot = the_ray.get_direction()[plane_axis];
            if (fabs(dir_dot) <= epsilon)
            {
                result = 0;
                return cParallel;
            }

            const scalar_type pos_dot = the_ray.get_origin()[plane_axis];

            scalar_type dist = positive_plane_dist - pos_dot;

            if ((front_only) && (dist > 0.0f))
            {
                result = 0;
                return cFailure;
            }

            result = dist / dir_dot;
            return cSuccess;
        }

        template <typename plane_type, typename ray_type, typename scalar_type>
        inline result ray_plane(scalar_type &result, const plane_type &the_plane, const ray_type &the_ray, bool front_only, scalar_type epsilon = 1e-6f)
        {
            const scalar_type dir_dot = the_plane.get_normal() * the_ray.get_direction();
            if (fabs(dir_dot) <= epsilon)
            {
                result = 0;
                return cParallel;
            }

            const scalar_type pos_dot = the_plane.get_normal() * the_ray.get_origin();

            scalar_type dist = the_plane.get_distance() - pos_dot;

            if ((front_only) && (dist > 0.0f))
            {
                result = 0;
                return cFailure;
            }

            result = dist / dir_dot;
            return cSuccess;
        }

        inline result plane2D_plane2D(vec2F &p, const plane2D &x, const plane2D &y, float epsilon = 1e-8f)
        {
            const double v = y.m_normal[0] * x.m_normal[1] - y.m_normal[1] * x.m_normal[0];
            if (fabs(v) <= epsilon)
            {
                p.clear();
                return cParallel;
            }
            double one_over_v = 1.0 / v;
            p.set(static_cast<float>((y.m_dist * x.m_normal[1] - y.m_normal[1] * x.m_dist) * one_over_v), static_cast<float>((y.m_normal[0] * x.m_dist - y.m_dist * x.m_normal[0]) * one_over_v));
            return cSuccess;
        }

        inline result plane2D_x_plane(vec2F &p, const plane2D &a, float pos_x_dist, float epsilon = 1e-8f)
        {
            const double v = a.m_normal[1];
            if (fabs(v) <= epsilon)
            {
                p.clear();
                return cParallel;
            }
            double one_over_v = 1.0 / v;
            p.set(static_cast<float>((pos_x_dist * a.m_normal[1]) * one_over_v), static_cast<float>((a.m_dist - pos_x_dist * a.m_normal[0]) * one_over_v));
            return cSuccess;
        }

        inline result plane2D_y_plane(vec2F &p, const plane2D &a, float pos_y_dist, float epsilon = 1e-8f)
        {
            const double v = -a.m_normal[0];
            if (fabs(v) <= epsilon)
            {
                p.clear();
                return cParallel;
            }
            double one_over_v = 1.0 / v;
            p.set(static_cast<float>((pos_y_dist * a.m_normal[1] - a.m_dist) * one_over_v), static_cast<float>((-pos_y_dist * a.m_normal[0]) * one_over_v));
            return cSuccess;
        }

    } // intersection

} // namespace vogl
