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

// File: vogl_vao_state.cpp
#include "vogl_vao_state.h"

bool vogl_vertex_attrib_desc::operator==(const vogl_vertex_attrib_desc &rhs) const
{
    VOGL_FUNC_TRACER

#define CMP(x)      \
    if (x != rhs.x) \
        return false;
    CMP(m_pointer);
    CMP(m_array_binding);
    CMP(m_size);
    CMP(m_type);
    CMP(m_stride);
    CMP(m_integer);
    CMP(m_divisor);
    CMP(m_enabled);
    CMP(m_normalized);
#undef CMP
    return true;
}

vogl_vao_state::vogl_vao_state()
    : m_snapshot_handle(0),
      m_element_array_binding(0),
      m_is_valid(false),
      m_has_been_bound(false)
{
    VOGL_FUNC_TRACER
}

vogl_vao_state::~vogl_vao_state()
{
    VOGL_FUNC_TRACER
}

static int vogl_get_vertex_attrib_int(uint32_t index, GLenum pname)
{
    VOGL_FUNC_TRACER

    int values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetVertexAttribIiv)(index, pname, values);
    return values[0];
}

static uint32_t vogl_get_vertex_attrib_uint(uint32_t index, GLenum pname)
{
    VOGL_FUNC_TRACER

    uint32_t values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetVertexAttribIuiv)(index, pname, values);
    return values[0];
}

#if 0
static vec4D vogl_get_vertex_attrib_vec4D(uint32_t index, GLenum pname)
{
	vec4D values(0);
	ACTUAL_GL_ENTRYPOINT(glGetVertexAttribdv)(index, pname, &values[0]);
	return values;
}
#endif

bool vogl_vao_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(target);
    VOGL_NOTE_UNUSED(remapper);

    VOGL_CHECK_GL_ERROR;

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);

    // TODO: Core profile support
    m_has_been_bound = handle ? (GL_ENTRYPOINT(glIsVertexArray)(static_cast<GLuint>(handle)) != 0) : true;

    if (m_has_been_bound)
    {
        vogl_scoped_binding_state orig_binding(GL_VERTEX_ARRAY);

        GL_ENTRYPOINT(glBindVertexArray)(m_snapshot_handle);
        VOGL_CHECK_GL_ERROR;

        m_element_array_binding = vogl_get_gl_integer(GL_ELEMENT_ARRAY_BUFFER_BINDING);

        m_vertex_attribs.resize(context_info.get_max_vertex_attribs());

        for (uint32_t i = 0; i < context_info.get_max_vertex_attribs(); i++)
        {
            vogl_vertex_attrib_desc &desc = m_vertex_attribs[i];

            desc.m_array_binding = vogl_get_vertex_attrib_int(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING);
            desc.m_enabled = vogl_get_vertex_attrib_int(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED) != 0;
            desc.m_size = vogl_get_vertex_attrib_int(i, GL_VERTEX_ATTRIB_ARRAY_SIZE);
            desc.m_type = vogl_get_vertex_attrib_int(i, GL_VERTEX_ATTRIB_ARRAY_TYPE);
            desc.m_normalized = vogl_get_vertex_attrib_int(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED) != 0;
            desc.m_stride = vogl_get_vertex_attrib_int(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE);
            desc.m_integer = vogl_get_vertex_attrib_int(i, GL_VERTEX_ATTRIB_ARRAY_INTEGER) != 0;
            desc.m_divisor = vogl_get_vertex_attrib_uint(i, GL_VERTEX_ATTRIB_ARRAY_DIVISOR);

            GLvoid *ptr = NULL;
            GL_ENTRYPOINT(glGetVertexAttribPointerv)(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
            desc.m_pointer = reinterpret_cast<vogl_trace_ptr_value>(ptr);

            VOGL_CHECK_GL_ERROR;
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_vao_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_CHECK_GL_ERROR;

    if (!m_is_valid)
        return false;

    vogl_scoped_binding_state orig_binding(GL_VERTEX_ARRAY, GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER);

    if ((!m_snapshot_handle) && (!handle))
    {
        GL_ENTRYPOINT(glBindVertexArray)(0);
        VOGL_CHECK_GL_ERROR;
    }
    else
    {
        if (!handle)
        {
            GLuint handle32 = 0;
            GL_ENTRYPOINT(glGenVertexArrays)(1, &handle32);
            if ((vogl_check_gl_error()) || (!handle32))
                return false;
            handle = handle32;

            if (m_snapshot_handle)
            {
                remapper.declare_handle(VOGL_NAMESPACE_VERTEX_ARRAYS, m_snapshot_handle, handle, GL_NONE);
                VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_VERTEX_ARRAYS, m_snapshot_handle) == handle);
            }
        }

        if (m_has_been_bound)
        {
            GL_ENTRYPOINT(glBindVertexArray)(static_cast<GLuint>(handle));
            VOGL_CHECK_GL_ERROR;
        }
    }

    if (m_has_been_bound)
    {
        GL_ENTRYPOINT(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, m_element_array_binding)));
        VOGL_CHECK_GL_ERROR;

        if (m_vertex_attribs.size() > context_info.get_max_vertex_attribs())
        {
            vogl_warning_printf("Saved VAO state has %u attribs, but context only allows %u attribs\n", m_vertex_attribs.size(), context_info.get_max_vertex_attribs());
        }

        for (uint32_t i = 0; i < math::minimum<uint32_t>(context_info.get_max_vertex_attribs(), m_vertex_attribs.size()); i++)
        {
            const vogl_vertex_attrib_desc &desc = m_vertex_attribs[i];

            GL_ENTRYPOINT(glBindBuffer)(GL_ARRAY_BUFFER, static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, desc.m_array_binding)));
            VOGL_CHECK_GL_ERROR;

            vogl_trace_ptr_value trace_ptr_val = desc.m_pointer;

            vogl_trace_ptr_value restore_ptr_val = trace_ptr_val;
            if ((!desc.m_array_binding) && (trace_ptr_val) && (context_info.is_compatibility_profile()))
                restore_ptr_val = remapper.remap_vertex_attrib_ptr(i, trace_ptr_val);

            void *pRestore_ptr = reinterpret_cast<void *>(restore_ptr_val);

            if ((handle) && (desc.m_array_binding == 0))
            {
                // If it's a non-default VAO, and there's no array binding, we can't call glVertexAttribPointer() because it's not allowed by AMD drivers (it thinks we're trying to set client side array data)
                // "OpenGL: glVertexAttribPointer failed because it is not allowed to specify a client-side vertex or element array when a non-default vertex array object is bound (GL_INVALID_OPERATION) [source=API type=ERROR severity=HIGH id=2100]"

                // Sanity checks.
                if ((pRestore_ptr != NULL) || (desc.m_stride) || (desc.m_enabled))
                {
                    vogl_warning_printf("Can't bind client side vertex array data on a non-default VAO, trace handle %u GL handle %u, restore ptr %p, size %i stride %i enabled %u\n",
                                        m_snapshot_handle, static_cast<uint32_t>(handle), pRestore_ptr, desc.m_size, desc.m_stride, desc.m_enabled);
                }
            }
            else
            {
                if (desc.m_integer)
                {
                    GL_ENTRYPOINT(glVertexAttribIPointer)(i, desc.m_size, desc.m_type, desc.m_stride, pRestore_ptr);
                    VOGL_CHECK_GL_ERROR;
                }
                else
                {
                    GL_ENTRYPOINT(glVertexAttribPointer)(i, desc.m_size, desc.m_type, desc.m_normalized, desc.m_stride, pRestore_ptr);
                    VOGL_CHECK_GL_ERROR;
                }
            }

            GL_ENTRYPOINT(glVertexAttribDivisor)(i, desc.m_divisor);
            VOGL_CHECK_GL_ERROR;

            if (desc.m_enabled)
            {
                GL_ENTRYPOINT(glEnableVertexAttribArray)(i);
                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                GL_ENTRYPOINT(glDisableVertexAttribArray)(i);
                VOGL_CHECK_GL_ERROR;
            }
        }
    }

    return true;
}

bool vogl_vao_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_VERTEX_ARRAYS, m_snapshot_handle));

    if (m_element_array_binding)
        m_element_array_binding = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, m_element_array_binding));

    for (uint32_t i = 0; i < m_vertex_attribs.size(); i++)
    {
        if (m_vertex_attribs[i].m_array_binding)
            m_vertex_attribs[i].m_array_binding = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, m_vertex_attribs[i].m_array_binding));

        else if (m_vertex_attribs[i].m_pointer)
            m_vertex_attribs[i].m_pointer = remapper.remap_vertex_attrib_ptr(i, m_vertex_attribs[i].m_pointer);
    }

    return true;
}

void vogl_vao_state::clear()
{
    VOGL_FUNC_TRACER

    m_vertex_attribs.clear();
    m_snapshot_handle = 0;
    m_element_array_binding = 0;
    m_is_valid = false;
    m_has_been_bound = false;
}

bool vogl_vao_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_is_valid)
        return false;

    node.add_key_value("handle", m_snapshot_handle);
    node.add_key_value("has_been_bound", m_has_been_bound);
    node.add_key_value("element_array_binding", m_element_array_binding);

    json_node &vertex_attribs_array = node.add_array("vertex_attribs");
    for (uint32_t i = 0; i < m_vertex_attribs.size(); i++)
    {
        const vogl_vertex_attrib_desc &desc = m_vertex_attribs[i];

        json_node &attribs_obj = vertex_attribs_array.add_object();
        attribs_obj.add_key_value("pointer", desc.m_pointer);
        attribs_obj.add_key_value("array_binding", desc.m_array_binding);
        attribs_obj.add_key_value("size", desc.m_size);
        attribs_obj.add_key_value("type", get_gl_enums().find_gl_name(desc.m_type));
        attribs_obj.add_key_value("stride", desc.m_stride);
        attribs_obj.add_key_value("integer", desc.m_integer);
        attribs_obj.add_key_value("divisor", desc.m_divisor);
        attribs_obj.add_key_value("enabled", desc.m_enabled);
        attribs_obj.add_key_value("normalized", desc.m_normalized);
    }

    return true;
}

bool vogl_vao_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    m_snapshot_handle = node.value_as_uint32("handle");
    m_has_been_bound = node.value_as_bool("has_been_bound", true);
    m_element_array_binding = node.value_as_uint32("element_array_binding");

    const json_node *pVertex_attribs_array = node.find_child_array("vertex_attribs");
    if (!pVertex_attribs_array)
        return false;

    m_vertex_attribs.resize(pVertex_attribs_array->size());
    for (uint32_t i = 0; i < pVertex_attribs_array->size(); i++)
    {
        vogl_vertex_attrib_desc &desc = m_vertex_attribs[i];

        const json_node *pAttribs_obj = pVertex_attribs_array->get_value_as_object(i);
        if (!pAttribs_obj)
            return false;

        desc.m_pointer = static_cast<vogl_trace_ptr_value>(pAttribs_obj->value_as_uint64("pointer"));

        // Fallback for old traces
        if ((!i) && (pAttribs_obj->has_key("element_array_binding")))
            m_element_array_binding = pAttribs_obj->value_as_uint32("element_array_binding");

        desc.m_array_binding = pAttribs_obj->value_as_uint32("array_binding");
        desc.m_size = pAttribs_obj->value_as_int("size");
        desc.m_type = vogl_get_json_value_as_enum(*pAttribs_obj, "type");
        desc.m_stride = pAttribs_obj->value_as_int("stride");
        desc.m_integer = pAttribs_obj->value_as_bool("integer");
        desc.m_divisor = pAttribs_obj->value_as_uint32("divisor");
        desc.m_enabled = pAttribs_obj->value_as_bool("enabled");
        desc.m_normalized = pAttribs_obj->value_as_bool("normalized");
    }

    m_is_valid = true;

    return true;
}

// Unused
bool vogl_vao_state::compare_all_state(const vogl_vao_state &rhs) const
{
    VOGL_FUNC_TRACER
    return false;
}

// Unused
bool vogl_vao_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER
    return false;
}
