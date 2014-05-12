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

// File: vogl_general_context_state.h
#ifndef VOGL_BASIC_CONTEXT_STATE_H
#define VOGL_BASIC_CONTEXT_STATE_H

#include "vogl_context_info.h"
#include "vogl_state_vector.h"
#include "vogl_blob_manager.h"
#include "vogl_gl_object.h"

bool vogl_gl_enum_is_dependent_on_client_active_texture(GLenum enum_val);
bool vogl_gl_enum_is_dependent_on_active_texture(GLenum enum_val);

class vogl_general_context_state : public vogl_state_vector
{
public:
    vogl_general_context_state();

    // Snapshots current context
    bool snapshot(const vogl_context_info &context_info);

    // Restores state to current context
    class vogl_persistent_restore_state
    {
    public:
        vogl_persistent_restore_state()
            : m_pSelect_buffer(NULL)
        {
            VOGL_FUNC_TRACER
        }

        void clear()
        {
            VOGL_FUNC_TRACER

            m_pSelect_buffer = NULL;
        }

        // May be NULL, if so the selection buffer can't be restored
        vogl::vector<GLuint> *m_pSelect_buffer;
    };

    bool restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, vogl_persistent_restore_state &persistent_state) const;

    bool remap_handles(vogl_handle_remapper &remapper);

private:
    bool can_snapshot_state(const vogl_context_info &context_info, vogl_snapshot_context_info &context, uint32_t get_desc_index);
    bool snapshot_state(const vogl_context_info &context_info, vogl_snapshot_context_info &context, uint32_t get_desc_index, uint32_t index, bool indexed_variant);
    bool restore_buffer_binding(GLenum binding_enum, GLenum set_enum, vogl_handle_remapper &remapper) const;
    bool restore_buffer_binding_range(GLenum binding_enum, GLenum start_enum, GLenum size_enum, GLenum set_enum, uint32_t index, bool indexed_variant, vogl_handle_remapper &remapper) const;
    bool snapshot_active_queries(const vogl_context_info &context_info);
};

class vogl_polygon_stipple_state
{
public:
    vogl_polygon_stipple_state();
    ~vogl_polygon_stipple_state();

    bool snapshot(const vogl_context_info &context_info);
    bool restore(const vogl_context_info &context_info) const;

    void clear();

    bool is_valid() const
    {
        return m_valid;
    }

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    uint32_t get_num_pattern_rows() const;
    uint32_t get_pattern_row(uint32_t row_index) const;

private:
    bool m_valid;
    uint8_t m_pattern[32 * 4];
};

#endif // VOGL_BASIC_CONTEXT_STATE_H
