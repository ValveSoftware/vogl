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

// File: vogl_gl_object.cpp
#include "vogl_gl_object.h"

#include "vogl_general_context_state.h"
#include "vogl_texture_state.h"
#include "vogl_buffer_state.h"
#include "vogl_fbo_state.h"
#include "vogl_sampler_state.h"
#include "vogl_shader_state.h"
#include "vogl_program_state.h"
#include "vogl_renderbuffer_state.h"
#include "vogl_query_state.h"
#include "vogl_vao_state.h"
#include "vogl_sso_state.h"
#include "vogl_sync_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_arb_program_state.h"

void vogl_handle_remapper::delete_handle_and_object(vogl_namespace_t handle_namespace, uint64_t from_handle, uint64_t to_handle)
{
    VOGL_FUNC_TRACER

    //VOGL_NOTE_UNUSED(handle_namespace);
    VOGL_NOTE_UNUSED(from_handle);
    //VOGL_NOTE_UNUSED(to_handle);

    vogl_destroy_gl_object(handle_namespace, to_handle);
}

const char *get_gl_object_state_type_str(vogl_gl_object_state_type type)
{
    VOGL_FUNC_TRACER

    switch (type)
    {
#define DEF(x)     \
    case cGLST##x: \
        return #x;
        VOGL_GL_OBJECT_STATE_TYPES
#undef DEF
        default:
            break;
    }
    return "";
}

vogl_gl_object_state_type determine_gl_object_state_type_from_str(const char *pStr)
{
    VOGL_FUNC_TRACER

    for (uint32_t i = 0; i < cGLSTTotalTypes; i++)
        if (!vogl_stricmp(pStr, get_gl_object_state_type_str(static_cast<vogl_gl_object_state_type>(i))))
            return static_cast<vogl_gl_object_state_type>(i);
    return cGLSTInvalid;
}

void vogl_destroy_gl_object(vogl_namespace_t handle_namespace, GLuint64 handle)
{
    VOGL_FUNC_TRACER

    if (!handle)
        return;

    GLuint handle32 = static_cast<GLuint>(handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_FRAMEBUFFERS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteFramebuffers)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteTextures)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_RENDER_BUFFERS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteRenderbuffers)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_QUERIES:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteQueries)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_SAMPLERS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteSamplers)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_PROGRAM_ARB:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteProgramsARB)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_PROGRAMS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteProgram)(handle32);
            break;
        }
        case VOGL_NAMESPACE_VERTEX_ARRAYS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteVertexArrays)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_LISTS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteLists)(handle32, 1);
            break;
        }
        case VOGL_NAMESPACE_SYNCS:
        {
            GLsync sync = vogl_handle_to_sync(handle);
            GL_ENTRYPOINT(glDeleteSync)(sync);
            break;
        }
        case VOGL_NAMESPACE_PIPELINES:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteProgramPipelines)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_SHADERS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteShader)(handle32);
            break;
        }
        case VOGL_NAMESPACE_BUFFERS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteBuffers)(1, &handle32);
            break;
        }
        case VOGL_NAMESPACE_FEEDBACKS:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteTransformFeedbacks)(1, &handle32);
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }

    VOGL_CHECK_GL_ERROR;
}

void vogl_destroy_gl_object(vogl_gl_object_state_type type, GLuint64 handle)
{
    VOGL_FUNC_TRACER

    if (!handle)
        return;

    GLuint handle32 = static_cast<GLuint>(handle);

    switch (type)
    {
        case cGLSTTexture:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteTextures)(1, &handle32);
            break;
        }
        case cGLSTRenderbuffer:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteRenderbuffers)(1, &handle32);
            break;
        }
        case cGLSTBuffer:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteBuffers)(1, &handle32);
            break;
        }
        case cGLSTFramebuffer:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteFramebuffers)(1, &handle32);
            break;
        }
        case cGLSTQuery:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteQueries)(1, &handle32);
            break;
        }
        case cGLSTShader:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteShader)(handle32);
            break;
        }
        case cGLSTProgram:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteProgram)(handle32);
            break;
        }
        case cGLSTSampler:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteSamplers)(1, &handle32);
            break;
        }
        case cGLSTVertexArray:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteVertexArrays)(1, &handle32);
            break;
        }
        case cGLSTSync:
        {
            GLsync sync = vogl_handle_to_sync(handle);
            GL_ENTRYPOINT(glDeleteSync)(sync);
            break;
        }
        case cGLSTARBProgram:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteProgramsARB)(1, &handle32);
            break;
        }
        case cGLSTProgramPipeline:
        {
            VOGL_ASSERT(handle == handle32);
            GL_ENTRYPOINT(glDeleteProgramPipelines)(1, &handle32);
            break;
        }
        default:
            VOGL_ASSERT_ALWAYS;
            break;
    }
    VOGL_CHECK_GL_ERROR;
}

vogl_gl_object_state *vogl_gl_object_state_factory(vogl_gl_object_state_type type)
{
    VOGL_FUNC_TRACER

    switch (type)
    {
        case cGLSTTexture:
            return vogl_new(vogl_texture_state);
        case cGLSTRenderbuffer:
            return vogl_new(vogl_renderbuffer_state);
        case cGLSTBuffer:
            return vogl_new(vogl_buffer_state);
        case cGLSTFramebuffer:
            return vogl_new(vogl_framebuffer_state);
        case cGLSTQuery:
            return vogl_new(vogl_query_state);
        case cGLSTShader:
            return vogl_new(vogl_shader_state);
        case cGLSTProgram:
            return vogl_new(vogl_program_state);
        case cGLSTSampler:
            return vogl_new(vogl_sampler_state);
        case cGLSTVertexArray:
            return vogl_new(vogl_vao_state);
        case cGLSTSync:
            return vogl_new(vogl_sync_state);
        case cGLSTARBProgram:
            return vogl_new(vogl_arb_program_state);
        case cGLSTProgramPipeline:
            return vogl_new(vogl_sso_state);
        default:
            VOGL_ASSERT_ALWAYS;
            break;
    }
    return NULL;
}
