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

// File: vogl_matrix_state.cpp
#include "vogl_matrix_state.h"

vogl_matrix_state::vogl_matrix_state()
    : m_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_matrix_state::~vogl_matrix_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_matrix_state::save_matrix_stack(const vogl_context_info &context_info, GLenum matrix, uint32_t index, GLenum depth_get, GLenum matrix_get)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    GL_ENTRYPOINT(glMatrixMode)(matrix);
    if (vogl_check_gl_error())
        return false;

    GLint depth = 0;
    GL_ENTRYPOINT(glGetIntegerv)(depth_get, &depth);
    if (vogl_check_gl_error())
    {
        // This *should* work on AMD because it supports the ARB_imaging subset on compat contexts, and it supports the GL_COLOR matrix and the max stack depth is reported as 10.
        if (depth_get == GL_COLOR_MATRIX_STACK_DEPTH)
        {
            vogl_warning_printf("Using GL_COLOR_MATRIX_STACK_DEPTH workaround for AMD drivers - this will purposely force a stack underflow!\n");

            vogl::vector<matrix44D> matrices;
            for (;;)
            {
                matrix44D mat;
                GL_ENTRYPOINT(glGetDoublev)(matrix_get, mat.get_ptr());
                if (vogl_check_gl_error())
                    return false;

                matrices.push_back(mat);

                GL_ENTRYPOINT(glPopMatrix)();
                bool failed_popping = vogl_check_gl_error();

                GL_ENTRYPOINT(glGetDoublev)(matrix_get, mat.get_ptr());
                // AMD fails with a stack underflow on the get, not the pop - at least during tracing?
                if (vogl_check_gl_error())
                    failed_popping = true;

                if (failed_popping)
                    break;
            }

            for (int i = matrices.size() - 1; i >= 0; i--)
            {
                if (i != static_cast<int>(matrices.size()) - 1)
                {
                    GL_ENTRYPOINT(glPushMatrix)();
                    if (vogl_check_gl_error())
                        return false;
                }

                GL_ENTRYPOINT(glLoadMatrixd)(matrices[i].get_ptr());
                if (vogl_check_gl_error())
                    return false;
            }

            depth = matrices.size();
        }
        else
        {
            return false;
        }
    }

    if (depth < 1)
        return false;

    matrix_vec &vec = m_matrices[matrix_key(matrix, index)];
    vec.resize(depth);

    // deepest .. current
    // 0       1   2

    for (int i = 0; i < depth; i++)
    {
        matrix44D mat;
        GL_ENTRYPOINT(glGetDoublev)(matrix_get, mat.get_ptr());
        if (vogl_check_gl_error())
            return false;

        vec[depth - 1 - i] = mat;

        if (i != (depth - 1))
        {
            GL_ENTRYPOINT(glPopMatrix)();
            if (vogl_check_gl_error())
                return false;
        }
    }

    for (int i = 1; i < depth; i++)
    {
        GL_ENTRYPOINT(glPushMatrix)();
        if (vogl_check_gl_error())
            return false;

        GL_ENTRYPOINT(glLoadMatrixd)(vec[i].get_ptr());
        if (vogl_check_gl_error())
            return false;
    }

    return true;
}

bool vogl_matrix_state::restore_matrix_stack(const vogl_context_info &context_info, GLenum matrix, uint32_t index) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    const matrix_vec *pVec = m_matrices.find_value(matrix_key(matrix, index));
    if ((!pVec) || (pVec->size() < 1))
        return false;

    GL_ENTRYPOINT(glMatrixMode)(matrix);
    if (vogl_check_gl_error())
        return false;

    // deepest .. current
    // 0       1   2

    for (uint32_t i = 0; i < pVec->size(); i++)
    {
        if (i)
        {
            GL_ENTRYPOINT(glPushMatrix)();
            if (vogl_check_gl_error())
                return false;
        }

        GL_ENTRYPOINT(glLoadMatrixd)((*pVec)[i].get_ptr());
        if (vogl_check_gl_error())
            return false;
    }

    return true;
}

bool vogl_matrix_state::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    clear();

    bool any_errors = false;

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_state_saver state_saver(cGSTActiveTexture, cGSTMatrixMode);

    if (vogl_check_gl_error())
        any_errors = true;

    if (!save_matrix_stack(context_info, GL_PROJECTION, 0, GL_PROJECTION_STACK_DEPTH, GL_PROJECTION_MATRIX))
        any_errors = true;

    if (!save_matrix_stack(context_info, GL_MODELVIEW, 0, GL_MODELVIEW_STACK_DEPTH, GL_MODELVIEW_MATRIX))
        any_errors = true;

    if (context_info.supports_extension("GL_ARB_imaging") && !save_matrix_stack(context_info, GL_COLOR, 0, GL_COLOR_MATRIX_STACK_DEPTH, GL_COLOR_MATRIX))
        any_errors = true;

    for (uint32_t texcoord_index = 0; texcoord_index < context_info.get_max_texture_coords(); texcoord_index++)
    {
        GL_ENTRYPOINT(glActiveTexture)(GL_TEXTURE0 + texcoord_index);

        if (vogl_check_gl_error())
            any_errors = true;

        if (!save_matrix_stack(context_info, GL_TEXTURE, texcoord_index, GL_TEXTURE_STACK_DEPTH, GL_TEXTURE_MATRIX))
            any_errors = true;
    }

    for (uint32_t i = 0; i < context_info.get_max_arb_program_matrices(); i++)
    {
        if (!save_matrix_stack(context_info, GL_MATRIX0_ARB + i, 0, GL_CURRENT_MATRIX_STACK_DEPTH_ARB, GL_CURRENT_MATRIX_ARB))
            any_errors = true;
    }

    if (any_errors)
        clear();
    else
        m_valid = true;

    return !any_errors;
}

bool vogl_matrix_state::restore(const vogl_context_info &context_info) const
{
    VOGL_FUNC_TRACER

    if (!m_valid)
        return false;

    bool any_errors = false;

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_state_saver state_saver(cGSTActiveTexture, cGSTMatrixMode);

    if (vogl_check_gl_error())
        any_errors = true;

    if (!restore_matrix_stack(context_info, GL_PROJECTION, 0))
        any_errors = true;

    if (!restore_matrix_stack(context_info, GL_MODELVIEW, 0))
        any_errors = true;

    if (context_info.supports_extension("GL_ARB_imaging") && !restore_matrix_stack(context_info, GL_COLOR, 0))
        any_errors = true;

    // TODO: Check to make sure we can actually restore the proper # of matrices
    for (uint32_t texcoord_index = 0; texcoord_index < context_info.get_max_texture_coords(); texcoord_index++)
    {
        GL_ENTRYPOINT(glActiveTexture)(GL_TEXTURE0 + texcoord_index);

        if (vogl_check_gl_error())
            any_errors = true;

        restore_matrix_stack(context_info, GL_TEXTURE, texcoord_index);
    }

    // TODO: Check to make sure we can actually restore the proper # of matrices
    for (uint32_t i = 0; i < context_info.get_max_arb_program_matrices(); i++)
    {
        restore_matrix_stack(context_info, GL_MATRIX0_ARB + i, 0);
    }

    return !any_errors;
}

void vogl_matrix_state::clear()
{
    VOGL_FUNC_TRACER

    m_valid = false;
    m_matrices.clear();
}

bool vogl_matrix_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_valid)
        return false;

    node.init_array();

    for (matrix_map::const_iterator it = m_matrices.begin(); it != m_matrices.end(); ++it)
    {
        json_node &obj_node = node.add_object();
        obj_node.add_key_value("matrix", get_gl_enums().find_gl_name(it->first.m_target));
        if (it->first.m_index)
            obj_node.add_key_value("index", it->first.m_index);

        json_node &matrices_node = obj_node.add_array("matrices");
        for (uint32_t i = 0; i < it->second.size(); i++)
        {
            json_node &matrix_node = matrices_node.add_array();
            for (uint32_t j = 0; j < 4 * 4; j++)
                matrix_node.add_value(it->second[i].get_ptr()[j]);
        }
    }

    return true;
}

bool vogl_matrix_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    if (!node.is_array() || !node.are_all_children_objects())
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    for (uint32_t i = 0; i < node.size(); i++)
    {
        const json_node &obj_node = *node.get_child(i);

        GLenum matrix = vogl_get_json_value_as_enum(obj_node, "matrix", GL_NONE);
        if (matrix == GL_NONE)
        {
            clear();
            return false;
        }

        uint32_t index = obj_node.value_as_uint32("index");
        if ((index) && (matrix != GL_TEXTURE))
        {
            clear();
            return false;
        }

        const json_node *pMatrices = obj_node.find_child_array("matrices");
        if ((!pMatrices) || (!pMatrices->are_all_children_arrays()))
        {
            clear();
            return false;
        }

        matrix_vec &matrices = m_matrices[matrix_key(matrix, index)];
        matrices.resize(pMatrices->size());

        for (uint32_t i2 = 0; i2 < matrices.size(); i2++)
        {
            const json_node *pMatrix = pMatrices->get_value_as_array(i2);
            if ((!pMatrix) || (!pMatrix->is_array()) || (pMatrix->size() != 4 * 4))
                return false;

            for (uint32_t j = 0; j < 4 * 4; j++)
                matrices[i2].get_ptr()[j] = pMatrix->value_as_double(j);
        }
    }

    m_valid = true;

    return true;
}

uint32_t vogl_matrix_state::get_matrix_stack_depth(GLenum target, uint32_t index) const
{
    matrix_map::const_iterator iter = m_matrices.find(matrix_key(target, index));
    if (iter == m_matrices.end())
    {
        return 0;
    }

    return iter->second.size();
}

const matrix44D *vogl_matrix_state::get_matrix(GLenum target, uint32_t index, uint32_t depth) const
{
    matrix_map::const_iterator iter = m_matrices.find(matrix_key(target, index));
    if (iter == m_matrices.end())
    {
        return NULL;
    }

    if (depth >= iter->second.size())
    {
        return NULL;
    }

    return &(iter->second[depth]);
}
