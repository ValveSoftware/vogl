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

// File: vogl_light_state.cpp
#include "vogl_light_state.h"

vogl_light_state::vogl_light_state()
    : m_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_light_state::~vogl_light_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_light_state::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    clear();

    VOGL_CHECK_GL_ERROR;

    bool any_gl_errors = false;

    m_lights.resize(context_info.get_max_lights());
    for (uint32_t light = 0; light < context_info.get_max_lights(); light++)
    {
#define GET_FLOAT(light, pname)                                            \
    do                                                                     \
    {                                                                      \
        float values[4] = { 0, 0, 0, 0 };                                  \
        GL_ENTRYPOINT(glGetLightfv)(get_light_enum(light), pname, values); \
        if (vogl_check_gl_error())                                          \
            any_gl_errors = true;                                          \
        m_lights[light].insert(pname, 0, values, sizeof(values[0]));       \
    } while (0)
        GET_FLOAT(light, GL_CONSTANT_ATTENUATION);
        GET_FLOAT(light, GL_LINEAR_ATTENUATION);
        GET_FLOAT(light, GL_QUADRATIC_ATTENUATION);
        GET_FLOAT(light, GL_SPOT_EXPONENT);
        GET_FLOAT(light, GL_SPOT_CUTOFF);
        GET_FLOAT(light, GL_AMBIENT);
        GET_FLOAT(light, GL_DIFFUSE);
        GET_FLOAT(light, GL_SPECULAR);
        GET_FLOAT(light, GL_POSITION);
        GET_FLOAT(light, GL_SPOT_DIRECTION);
#undef GET_FLOAT
    }

    if (any_gl_errors)
    {
        clear();

        vogl_error_printf("GL error while enumerating light params\n");

        return false;
    }

    m_valid = true;

    return true;
}

bool vogl_light_state::set_light_parameter(uint32_t light, GLenum pname) const
{
    VOGL_FUNC_TRACER

    GLenum light_enum = get_light_enum(light);

    const vogl_state_data *pData = m_lights[light].find(pname);
    if (!pData)
        return false;

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
            GL_ENTRYPOINT(glLightf)(light_enum, pname, fvals[0]);
        else
            GL_ENTRYPOINT(glLightfv)(light_enum, pname, fvals);
    }
    else
    {
        int ivals[cMaxElements];
        pData->get_int(ivals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glLighti)(light_enum, pname, ivals[0]);
        else
            GL_ENTRYPOINT(glLightiv)(light_enum, pname, ivals);
    }

    return !vogl_check_gl_error();
}

bool vogl_light_state::restore(const vogl_context_info &context_info) const
{
    VOGL_FUNC_TRACER

    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
    }

    const uint32_t num_lights = math::minimum<uint32_t>(m_lights.size(), context_info.get_max_lights());
    if (m_lights.size() > context_info.get_max_lights())
    {
        vogl_warning_printf("Object has %u lights, but the context only supports %u lights!\n", m_lights.size(), context_info.get_max_lights());
    }

    for (uint32_t light = 0; light < num_lights; light++)
    {
#define SET_FLOAT(light, pname) set_light_parameter(light, pname)
        SET_FLOAT(light, GL_CONSTANT_ATTENUATION);
        SET_FLOAT(light, GL_LINEAR_ATTENUATION);
        SET_FLOAT(light, GL_QUADRATIC_ATTENUATION);
        SET_FLOAT(light, GL_SPOT_EXPONENT);
        SET_FLOAT(light, GL_SPOT_CUTOFF);
        SET_FLOAT(light, GL_AMBIENT);
        SET_FLOAT(light, GL_DIFFUSE);
        SET_FLOAT(light, GL_SPECULAR);
        SET_FLOAT(light, GL_POSITION);
        SET_FLOAT(light, GL_SPOT_DIRECTION);
#undef SET_FLOAT
    }

    return !vogl_check_gl_error();
}

void vogl_light_state::clear()
{
    m_lights.clear();
    m_valid = false;
}

bool vogl_light_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    json_node &light_array_node = node.add_array("lights");
    for (uint32_t i = 0; i < m_lights.size(); i++)
        if (!m_lights[i].serialize(light_array_node.add_object(), blob_manager))
            return false;

    return true;
}

bool vogl_light_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    const json_node *pLight_array_node = node.find_child_array("lights");
    if (!pLight_array_node)
        return false;
    if (!pLight_array_node->are_all_children_objects())
        return false;

    m_lights.resize(pLight_array_node->size());

    for (uint32_t i = 0; i < m_lights.size(); i++)
    {
        if (!m_lights[i].deserialize(*pLight_array_node->get_child(i), blob_manager))
        {
            clear();
            return false;
        }
    }

    m_valid = true;

    return true;
}
