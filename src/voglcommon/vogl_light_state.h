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

// File: vogl_light_state.h
#ifndef VOGL_LIGHT_STATE_H
#define VOGL_LIGHT_STATE_H

#include "vogl_core.h"
#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"

class vogl_light_state
{
public:
    vogl_light_state();
    ~vogl_light_state();

    bool snapshot(const vogl_context_info &context_info);

    bool restore(const vogl_context_info &context_info) const;

    void clear();

    bool is_valid() const
    {
        return m_valid;
    }

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    uint32_t get_num_lights() const
    {
        return m_lights.size();
    }

    const vogl_state_vector &get_light(uint32_t index) const
    {
        VOGL_ASSERT(index < m_lights.size());

        return m_lights[index];
    }

    static GLenum get_light_enum(uint32_t light)
    {
        return GL_LIGHT0 + light;
    }

private:
    vogl::vector<vogl_state_vector> m_lights;

    bool m_valid;

    bool set_light_parameter(uint32_t light, GLenum pname) const;
};

#endif // VOGL_LIGHT_STATE_H
