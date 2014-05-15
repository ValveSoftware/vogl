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

// File: vogl_default_framebuffer_state.h
#ifndef VOGL_DEFAULT_FRAMEBUFFER_STATE_H
#define VOGL_DEFAULT_FRAMEBUFFER_STATE_H

#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include "vogl_map.h"

#include "vogl_common.h"
#include "vogl_general_context_state.h"
#include "vogl_texture_state.h"

struct vogl_default_framebuffer_attribs
{
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_r_size;
    uint32_t m_g_size;
    uint32_t m_b_size;
    uint32_t m_a_size;
    uint32_t m_depth_size;
    uint32_t m_stencil_size;
    uint32_t m_samples;
    bool m_double_buffered;

    vogl_default_framebuffer_attribs()
    {
        clear();
    }

    void clear()
    {
        utils::zero_object(*this);
    }

    bool serialize(json_node &node) const
    {
        node.add_key_value("width", m_width);
        node.add_key_value("height", m_height);
        node.add_key_value("r_size", m_r_size);
        node.add_key_value("g_size", m_g_size);
        node.add_key_value("b_size", m_b_size);
        node.add_key_value("a_size", m_a_size);
        node.add_key_value("depth_size", m_depth_size);
        node.add_key_value("stencil_size", m_stencil_size);
        node.add_key_value("samples", m_samples);
        node.add_key_value("double_buffered", m_double_buffered);
        return true;
    }

    bool deserialize(const json_node &node)
    {
        m_width = node.value_as_uint32("width");
        m_height = node.value_as_uint32("height");
        m_r_size = node.value_as_uint32("r_size");
        m_g_size = node.value_as_uint32("g_size");
        m_b_size = node.value_as_uint32("b_size");
        m_a_size = node.value_as_uint32("a_size");
        m_depth_size = node.value_as_uint32("depth_size");
        m_stencil_size = node.value_as_uint32("stencil_size");
        m_samples = node.value_as_uint32("samples");
        m_double_buffered = node.value_as_bool("double_buffered");
        return true;
    }
};

bool vogl_get_default_framebuffer_attribs(vogl_default_framebuffer_attribs &attribs, uint32_t screen);

enum vogl_default_framebuffer_t
{
    cDefFramebufferFrontLeft,
    cDefFramebufferBackLeft,
    cDefFramebufferFrontRight,
    cDefFramebufferBackRight,
    cDefFramebufferDepthStencil,

    cDefFramebufferTotal
};

class vogl_default_framebuffer_state
{
public:
    vogl_default_framebuffer_state();
    ~vogl_default_framebuffer_state();

    bool snapshot(const vogl_context_info &context_info, const vogl_default_framebuffer_attribs &fb_attribs);
    bool restore(const vogl_context_info &context_info, bool restore_front_buffer) const;

    void clear();

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    bool is_valid() const
    {
        return m_valid;
    }

    const vogl_texture_state &get_texture(vogl_default_framebuffer_t index) const
    {
        VOGL_ASSERT(index < cDefFramebufferTotal);
        return m_textures[index];
    }

    vogl_texture_state &get_texture(vogl_default_framebuffer_t index)
    {
        VOGL_ASSERT(index < cDefFramebufferTotal);
        return m_textures[index];
    }


private:
    vogl_default_framebuffer_attribs m_fb_attribs;

    vogl_texture_state m_textures[cDefFramebufferTotal];

    bool m_valid;
};

#endif // VOGL_DEFAULT_FRAMEBUFFER_STATE_H
