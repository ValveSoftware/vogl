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

// File: vogl_material_state.cpp
#include "vogl_material_state.h"

vogl_material_state::vogl_material_state()
    : m_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_material_state::~vogl_material_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_material_state::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    clear();

    VOGL_CHECK_GL_ERROR;

    bool any_gl_errors = false;

#define GET_FLOAT(side, pname)                                         \
    do                                                                 \
    {                                                                  \
        float values[4] = { 0, 0, 0, 0 };                              \
        GL_ENTRYPOINT(glGetMaterialfv)(get_face(side), pname, values); \
        if (vogl_check_gl_error())                                      \
            any_gl_errors = true;                                      \
        m_params[side].insert(pname, 0, values, sizeof(values[0]));    \
    } while (0)
    for (uint32_t s = 0; s < cTotalSides; s++)
    {
        GET_FLOAT(s, GL_AMBIENT);
        GET_FLOAT(s, GL_DIFFUSE);
        GET_FLOAT(s, GL_SPECULAR);
        GET_FLOAT(s, GL_EMISSION);
        GET_FLOAT(s, GL_SHININESS);
        GET_FLOAT(s, GL_COLOR_INDEXES);
    }
#undef GET_FLOAT

    if (any_gl_errors)
    {
        clear();

        vogl_error_printf("GL error while enumerating material params\n");
        return false;
    }

    m_valid = true;

    return true;
}

bool vogl_material_state::set_material_parameter(uint32_t side, GLenum pname) const
{
    VOGL_FUNC_TRACER

    const GLenum face = get_face(side);

    const vogl_state_data *pData = m_params[side].find(pname);
    if (!pData)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    enum
    {
        cMaxElements = 4
    };
    if (pData->get_num_elements() > cMaxElements)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((pData->get_data_type() == cSTFloat) || (pData->get_data_type() == cSTDouble))
    {
        float fvals[cMaxElements];
        pData->get_float(fvals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glMaterialf)(face, pname, fvals[0]);
        else
            GL_ENTRYPOINT(glMaterialfv)(face, pname, fvals);
    }
    else
    {
        int ivals[cMaxElements];
        pData->get_int(ivals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glMateriali)(face, pname, ivals[0]);
        else
            GL_ENTRYPOINT(glMaterialiv)(face, pname, ivals);
    }

    return !vogl_check_gl_error();
}

bool vogl_material_state::restore(const vogl_context_info &context_info) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    VOGL_CHECK_GL_ERROR;

#define SET_FLOAT(side, pname) set_material_parameter(side, pname)
    for (uint32_t s = 0; s < cTotalSides; s++)
    {
        SET_FLOAT(s, GL_AMBIENT);
        SET_FLOAT(s, GL_DIFFUSE);
        SET_FLOAT(s, GL_SPECULAR);
        SET_FLOAT(s, GL_EMISSION);
        SET_FLOAT(s, GL_SHININESS);
        SET_FLOAT(s, GL_COLOR_INDEXES);
    }
#undef GET_FLOAT

    return !vogl_check_gl_error();
}

void vogl_material_state::clear()
{
    VOGL_FUNC_TRACER

    for (uint32_t i = 0; i < cTotalSides; i++)
        m_params[i].clear();

    m_valid = false;
}

bool vogl_material_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_params[cFront].serialize(node.add_object("front"), blob_manager) || !m_params[cBack].serialize(node.add_object("back"), blob_manager))
        return false;

    return true;
}

bool vogl_material_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    if (!m_params[cFront].deserialize("front", node, blob_manager) || !m_params[cBack].deserialize("back", node, blob_manager))
    {
        clear();
        return false;
    }

    m_valid = true;

    return true;
}

vogl_state_vector *vogl_material_state::get_state_vector(int side)
{
    VOGL_FUNC_TRACER

    if (side < 0 || side >= cTotalSides)
    {
        return NULL;
    }

    return &(m_params[side]);
}

const vogl_state_vector *vogl_material_state::get_state_vector(int side) const
{
    VOGL_FUNC_TRACER

    if (side < 0 || side >= cTotalSides)
    {
        return NULL;
    }

    return &(m_params[side]);
}
