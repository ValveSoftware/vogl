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

// File: vogl_sso_state.h
#ifndef VOGL_SSO_STATE_H
#define VOGL_SSO_STATE_H

#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include "vogl_map.h"

#include "vogl_common.h"
#include "vogl_general_context_state.h"

class vogl_sso_state : public vogl_gl_object_state
{
public:
    vogl_sso_state();
    virtual ~vogl_sso_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTProgramPipeline;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_PIPELINES;
    }

    GLint get_shader_int(GLenum pname) const;
    
    // Creates snapshot of SSO handle, or 0 for the default framebuffer.
    virtual bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target);

    // Creates and restores SSO
    virtual bool restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const;

    virtual bool remap_handles(vogl_handle_remapper &remapper);

    virtual void clear();

    virtual bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    virtual bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    virtual GLuint64 get_snapshot_handle() const
    {
        return m_snapshot_handle;
    }
    virtual bool is_valid() const
    {
        return m_is_valid;
    }
    uint32_t get_active_program() const
    {
        return m_active_program;
    }
    uint32_t get_info_log_length() const
    {
        return m_info_log_length;
    }
    uint32_t get_shader_program(uint32_t i) const
    {
        VOGL_ASSERT(i < cNumShaders);
        return m_shader_objs[i];
    }

    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs_obj) const;
    
    static const GLenum* gl_shader_type_mapping;
    enum
    {
        cVertexShader,
        cFragmentShader,
        cGeometryShader,
        cTessControlShader,
        cTessEvalShader,
        cNumShaders
    };

private:
    GLuint m_snapshot_handle;
    bool m_has_been_bound;
    GLuint m_shader_objs[cNumShaders];
    GLuint m_active_program;
    GLuint m_info_log_length;

    bool m_is_valid;
    static const char *get_shader_index_name(uint32_t index)
    {
        VOGL_ASSERT(index < cNumShaders);
        switch (index)
        {
            case (cVertexShader):
                return "vertex";
            case (cFragmentShader):
                return "fragment";
            case (cGeometryShader):
                return "geometry";
            case (cTessControlShader):
                return "tesselation_control";
            case (cTessEvalShader):
                return "tesselation_eval";
            default:
                VOGL_ASSERT_ALWAYS;
                return "unknown";
        }
    }
};

#endif // #ifndef VOGL_SSO_STATE_H
