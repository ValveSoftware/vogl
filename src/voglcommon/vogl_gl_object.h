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

// File: vogl_gl_object.h
#ifndef VOGL_GL_OBJECT_H
#define VOGL_GL_OBJECT_H

#include "vogl_common.h"

class vogl_blob_manager;

struct vogl_handle_remapper
{
    virtual ~vogl_handle_remapper()
    {
    }

    virtual bool is_default_remapper() const
    {
        return true;
    }

    virtual uint64_t remap_handle(vogl_namespace_t handle_namespace, uint64_t from_handle)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(handle_namespace);
        return from_handle;
    }

    // Note: This may not be fully implemented during tracing!
    virtual bool is_valid_handle(vogl_namespace_t handle_namespace, uint64_t from_handle)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(handle_namespace);
        VOGL_NOTE_UNUSED(from_handle);
        VOGL_VERIFY(0);
        return false;
    }

    virtual int32_t remap_location(uint32_t program, int32_t from_location)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(program);
        return from_location;
    }

    virtual vogl_trace_ptr_value remap_vertex_attrib_ptr(uint32_t index, vogl_trace_ptr_value ptr_val)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(index);
        return ptr_val;
    }

    virtual vogl_trace_ptr_value remap_vertex_array_ptr(vogl_client_side_array_desc_id_t id, uint32_t index, vogl_trace_ptr_value ptr_val)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(id);
        VOGL_NOTE_UNUSED(index);
        return ptr_val;
    }

    virtual void declare_handle(vogl_namespace_t handle_namespace, uint64_t from_handle, uint64_t to_handle, GLenum target)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(handle_namespace);
        VOGL_NOTE_UNUSED(from_handle);
        VOGL_NOTE_UNUSED(to_handle);
        VOGL_NOTE_UNUSED(target);
    }

    virtual void delete_handle_and_object(vogl_namespace_t handle_namespace, uint64_t from_handle, uint64_t to_handle);

    virtual void declare_location(uint32_t from_program_handle, uint32_t to_program_handle, int32_t from_location, int32_t to_location)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(from_program_handle);
        VOGL_NOTE_UNUSED(to_program_handle);
        VOGL_NOTE_UNUSED(from_location);
        VOGL_NOTE_UNUSED(to_location);
    }

    // Note: This may not be fully implemented during tracing!
    virtual bool determine_from_object_target(vogl_namespace_t handle_namespace, uint64_t from_handle, GLenum &target)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(handle_namespace);
        VOGL_NOTE_UNUSED(from_handle);

        VOGL_VERIFY(0);
        target = GL_NONE;
        return false;
    }

    // Note: This may not be fully implemented during tracing!
    virtual bool determine_to_object_target(vogl_namespace_t handle_namespace, uint64_t to_handle, GLenum &target)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(handle_namespace);
        VOGL_NOTE_UNUSED(to_handle);

        VOGL_VERIFY(0);
        target = GL_NONE;
        return false;
    }
};

// Update this macro if you add more GL state object types!
#define VOGL_GL_OBJECT_STATE_TYPES \
    DEF(Invalid)                  \
    DEF(Texture)                  \
    DEF(Renderbuffer)             \
    DEF(Buffer)                   \
    DEF(Framebuffer)              \
    DEF(Query)                    \
    DEF(Shader)                   \
    DEF(Program)                  \
    DEF(Sampler)                  \
    DEF(VertexArray)              \
    DEF(Sync)                     \
    DEF(ARBProgram)               \
    DEF(ProgramPipeline)

enum vogl_gl_object_state_type
{
#define DEF(x) cGLST##x,
    VOGL_GL_OBJECT_STATE_TYPES
#undef DEF
        cGLSTTotalTypes
};

const char *get_gl_object_state_type_str(vogl_gl_object_state_type type);

vogl_gl_object_state_type determine_gl_object_state_type_from_str(const char *pStr);

void vogl_destroy_gl_object(vogl_namespace_t handle_namespace, GLuint64 handle);
void vogl_destroy_gl_object(vogl_gl_object_state_type type, GLuint64 handle);

class vogl_gl_object_state
{
public:
    vogl_gl_object_state()
    {
    }

    virtual ~vogl_gl_object_state()
    {
    }

    virtual vogl_gl_object_state_type get_type() const = 0;

    virtual vogl_namespace_t get_handle_namespace() const = 0;

    virtual bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target) = 0;

    virtual bool restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const = 0;

    virtual bool remap_handles(vogl_handle_remapper &remapper) = 0;

    virtual GLuint64 get_snapshot_handle() const = 0;

    virtual void clear() = 0;

    virtual bool is_valid() const = 0;

    virtual bool serialize(json_node &node, vogl_blob_manager &blob_manager) const = 0;
    virtual bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager) = 0;

    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs) const = 0;

    virtual bool get_marked_for_deletion() const
    {
        return false;
    }
};

typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;
typedef vogl::vector<const vogl_gl_object_state *> vogl_const_gl_object_state_ptr_vec;

vogl_gl_object_state *vogl_gl_object_state_factory(vogl_gl_object_state_type type);

#endif // VOGL_GL_OBJECT_H
