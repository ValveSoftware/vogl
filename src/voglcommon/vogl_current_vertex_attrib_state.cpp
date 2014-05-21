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

// File: vogl_current_vertex_attrib_state.cpp
#include "vogl_current_vertex_attrib_state.h"
#include "vogl_context_info.h"

vogl_current_vertex_attrib_state::vogl_current_vertex_attrib_state()
    : m_valid(false)
{
}

vogl_current_vertex_attrib_state::~vogl_current_vertex_attrib_state()
{
}

bool vogl_current_vertex_attrib_state::snapshot(const vogl_context_info &context_info)
{
    clear();

    VOGL_CHECK_GL_ERROR;

    m_current_vertex_attribs.resize(context_info.get_max_vertex_attribs());

    if (context_info.get_max_vertex_attribs())
    {
        m_current_vertex_attribs[0].set(0, 0, 0, 1);

        for (uint32_t i = 1; i < context_info.get_max_vertex_attribs(); i++)
        {
            GL_ENTRYPOINT(glGetVertexAttribdv)(i, GL_CURRENT_VERTEX_ATTRIB, m_current_vertex_attribs[i].get_ptr());

            VOGL_CHECK_GL_ERROR;
        }
    }

    m_valid = true;

    return true;
}

bool vogl_current_vertex_attrib_state::restore(const vogl_context_info &context_info) const
{
    if (!m_valid)
        return false;

    VOGL_CHECK_GL_ERROR;

    if (m_current_vertex_attribs.size() > context_info.get_max_vertex_attribs())
    {
        vogl_error_printf("Unable to restore all current vertex attribs, object has %u attribs, context only supports %u attribs\n", m_current_vertex_attribs.size(), context_info.get_max_vertex_attribs());
    }

    uint32_t max_vertex_attribs_to_restore = math::minimum<uint32_t>(m_current_vertex_attribs.size(), context_info.get_max_vertex_attribs());

    for (uint32_t i = 1; i < max_vertex_attribs_to_restore; i++)
    {
        GL_ENTRYPOINT(glVertexAttrib4dv)(i, m_current_vertex_attribs[i].get_ptr());

        VOGL_CHECK_GL_ERROR;
    }

    return true;
}

void vogl_current_vertex_attrib_state::clear()
{
    m_current_vertex_attribs.clear();
    m_valid = false;
}

bool vogl_current_vertex_attrib_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_valid)
        return false;

    node.init_array();
    for (uint32_t i = 0; i < m_current_vertex_attribs.size(); i++)
    {
        json_node &vec_node = node.add_array();
        for (uint32_t j = 0; j < 4; j++)
            vec_node.add_value(m_current_vertex_attribs[i][j]);
    }

    return true;
}

bool vogl_current_vertex_attrib_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    if (!node.are_all_children_arrays())
        return false;

    m_current_vertex_attribs.resize(node.size());
    for (uint32_t i = 0; i < node.size(); i++)
    {
        const json_node &vec_node = *node.get_child(i);
        if (vec_node.size() != 4)
        {
            clear();
            return false;
        }

        // TODO: We could have roundtrip errors here.
        m_current_vertex_attribs[i].set(vec_node[0U].as_double(), vec_node[1U].as_double(), vec_node[2U].as_double(), vec_node[3U].as_double());
    }

    m_valid = true;

    return true;
}
