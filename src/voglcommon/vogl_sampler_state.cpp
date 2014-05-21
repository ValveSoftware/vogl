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

// File: vogl_sampler_state.cpp
#include "vogl_common.h"
#include "vogl_sampler_state.h"

vogl_sampler_state::vogl_sampler_state()
    : m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_sampler_state::~vogl_sampler_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_sampler_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);
    VOGL_CHECK_GL_ERROR;
    (void)target;

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<uint32_t>(handle);

    bool any_gl_errors = false;

#define GET_INT(pname)                                                            \
    do                                                                            \
    {                                                                             \
        int values[4] = { 0, 0, 0, 0 };                                           \
        GL_ENTRYPOINT(glGetSamplerParameteriv)(m_snapshot_handle, pname, values); \
        if (vogl_check_gl_error())                                                 \
            any_gl_errors = true;                                                 \
        m_params.insert(pname, 0, values, sizeof(values[0]));                     \
    } while (0)
#define GET_FLOAT(pname)                                                          \
    do                                                                            \
    {                                                                             \
        float values[4] = { 0, 0, 0, 0 };                                         \
        GL_ENTRYPOINT(glGetSamplerParameterfv)(m_snapshot_handle, pname, values); \
        if (vogl_check_gl_error())                                                 \
            any_gl_errors = true;                                                 \
        m_params.insert(pname, 0, values, sizeof(values[0]));                     \
    } while (0)

    GET_INT(GL_TEXTURE_MAG_FILTER);
    GET_INT(GL_TEXTURE_MIN_FILTER);
    GET_FLOAT(GL_TEXTURE_MIN_LOD);
    GET_FLOAT(GL_TEXTURE_MAX_LOD);
    GET_INT(GL_TEXTURE_WRAP_S);
    GET_INT(GL_TEXTURE_WRAP_T);
    GET_INT(GL_TEXTURE_WRAP_R);
    GET_FLOAT(GL_TEXTURE_BORDER_COLOR);
    GET_INT(GL_TEXTURE_COMPARE_MODE);
    GET_INT(GL_TEXTURE_COMPARE_FUNC);

    if (context_info.supports_extension("GL_EXT_texture_filter_anisotropic"))
    {
        GET_FLOAT(GL_TEXTURE_MAX_ANISOTROPY_EXT);
    }

    if (context_info.supports_extension("GL_EXT_texture_sRGB_decode"))
    {
        GET_INT(GL_TEXTURE_SRGB_DECODE_EXT);
    }

#undef GET_INT
#undef GET_FLOAT

    if (any_gl_errors)
    {
        clear();

        vogl_error_printf("GL error while enumerating sampler %" PRIu64 "'s' params\n", (uint64_t)handle);
        return false;
    }

    m_is_valid = true;

    return true;
}

bool vogl_sampler_state::set_sampler_parameter(GLuint handle, GLenum pname) const
{
    VOGL_FUNC_TRACER

    const vogl_state_data *pData = m_params.find(pname);
    if (!pData)
        return false;

    enum
    {
        cMaxElements = 16
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
            GL_ENTRYPOINT(glSamplerParameterf)(handle, pname, fvals[0]);
        else
            GL_ENTRYPOINT(glSamplerParameterfv)(handle, pname, fvals);
    }
    else
    {
        int ivals[cMaxElements];
        pData->get_int(ivals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glSamplerParameteri)(handle, pname, ivals[0]);
        else
            GL_ENTRYPOINT(glSamplerParameteriv)(handle, pname, ivals);
    }

    return !vogl_check_gl_error();
}

bool vogl_sampler_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    VOGL_CHECK_GL_ERROR;

    if (!handle)
    {
        GLuint handle32 = 0;
        GL_ENTRYPOINT(glGenSamplers)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            return false;
        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_SAMPLERS, m_snapshot_handle, handle, GL_NONE);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_SAMPLERS, m_snapshot_handle) == handle);
    }

    bool any_failures = false;

#define SET_INT(pname)                                                  \
    do                                                                  \
    {                                                                   \
        if (!set_sampler_parameter(static_cast<GLuint>(handle), pname)) \
            any_failures = true;                                        \
    } while (0)
#define SET_FLOAT(pname)                                                \
    do                                                                  \
    {                                                                   \
        if (!set_sampler_parameter(static_cast<GLuint>(handle), pname)) \
            any_failures = true;                                        \
    } while (0)

    SET_INT(GL_TEXTURE_MAG_FILTER);
    SET_INT(GL_TEXTURE_MIN_FILTER);
    SET_FLOAT(GL_TEXTURE_MIN_LOD);
    SET_FLOAT(GL_TEXTURE_MAX_LOD);
    SET_INT(GL_TEXTURE_WRAP_S);
    SET_INT(GL_TEXTURE_WRAP_T);
    SET_INT(GL_TEXTURE_WRAP_R);
    SET_FLOAT(GL_TEXTURE_BORDER_COLOR);
    SET_INT(GL_TEXTURE_COMPARE_MODE);
    SET_INT(GL_TEXTURE_COMPARE_FUNC);

    if (context_info.supports_extension("GL_EXT_texture_filter_anisotropic"))
    {
        SET_FLOAT(GL_TEXTURE_MAX_ANISOTROPY_EXT);
    }

    if (context_info.supports_extension("GL_EXT_texture_sRGB_decode"))
    {
        SET_INT(GL_TEXTURE_SRGB_DECODE_EXT);
    }

    if (any_failures)
    {
        vogl_warning_printf("One or more sampler params could not be set on trace sampler %u, replay sampler %" PRIu64 "\n",
                           m_snapshot_handle, (uint64_t)handle);
    }

    return true;
}

bool vogl_sampler_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_snapshot_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_SAMPLERS, m_snapshot_handle));

    return true;
}

void vogl_sampler_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_params.clear();
    m_is_valid = false;
}

bool vogl_sampler_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    node.add_key_value("handle", m_snapshot_handle);

    return m_params.serialize(node.add_object("params"), blob_manager);
}

bool vogl_sampler_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    m_snapshot_handle = node.value_as_uint32("handle");

    const json_node *pParams_node = node.find_child_object("params");
    if ((!pParams_node) || (!m_params.deserialize(*pParams_node, blob_manager)))
    {
        clear();
        return false;
    }

    m_is_valid = true;

    return true;
}

bool vogl_sampler_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTSampler)
        return false;

    const vogl_sampler_state &rhs = static_cast<const vogl_sampler_state &>(rhs_obj);

    if (this == &rhs)
        return true;

    return m_params == rhs.m_params;
}
