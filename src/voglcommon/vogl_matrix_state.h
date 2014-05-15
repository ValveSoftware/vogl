/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
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

// File: vogl_matrix_state.h
#ifndef VOGL_MATRIX_STATE_H
#define VOGL_MATRIX_STATE_H

#include "vogl_core.h"
#include "vogl_matrix.h"
#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"

class vogl_matrix_state
{
public:
    vogl_matrix_state();
    ~vogl_matrix_state();

    bool snapshot(const vogl_context_info &context_info);

    bool restore(const vogl_context_info &context_info) const;

    void clear();

    bool is_valid() const
    {
        return m_valid;
    }

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    uint32_t get_matrix_stack_depth(GLenum target, uint32_t index) const;

    const matrix44D *get_matrix(GLenum target, uint32_t index, uint32_t depth) const;

private:
    typedef vogl::vector<matrix44D> matrix_vec;

    class matrix_key
    {
    public:
        matrix_key(GLenum target = GL_NONE, uint32_t index = 0)
            : m_target(target),
              m_index(index)
        {
            VOGL_FUNC_TRACER
        }

        bool operator==(const matrix_key &other) const
        {
            VOGL_FUNC_TRACER

            return (m_target == other.m_target) && (m_index == other.m_index);
        }

        bool operator<(const matrix_key &other) const
        {
            VOGL_FUNC_TRACER

            if (m_target < other.m_target)
                return true;
            if (m_target == other.m_target)
                return m_index < other.m_index;
            return false;
        }

        GLenum m_target;
        uint32_t m_index;
    };

    typedef vogl::map<matrix_key, matrix_vec> matrix_map;
    matrix_map m_matrices;

    bool m_valid;

    bool restore_matrix_stack(const vogl_context_info &context_info, GLenum matrix, uint32_t index) const;
    bool save_matrix_stack(const vogl_context_info &context_info, GLenum matrix, uint32_t index, GLenum depth_get, GLenum matrix_get);
};

#endif // VOGL_MATRIX_STATE_H
