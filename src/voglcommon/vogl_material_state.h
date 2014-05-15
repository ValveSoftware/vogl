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

// File: vogl_material_state.h
#ifndef VOGL_MATERIAL_STATE_H
#define VOGL_MATERIAL_STATE_H

#include "vogl_core.h"
#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"

class vogl_material_state
{
public:
    vogl_material_state();
    ~vogl_material_state();

    bool snapshot(const vogl_context_info &context_info);

    bool restore(const vogl_context_info &context_info) const;

    void clear();

    bool is_valid() const
    {
        return m_valid;
    }

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    vogl_state_vector *get_state_vector(int side);

    const vogl_state_vector *get_state_vector(int side) const;

    enum
    {
        cFront = 0,
        cBack = 1,
        cTotalSides
    };

private:
    vogl_state_vector m_params[cTotalSides];

    bool m_valid;

    bool set_material_parameter(uint32_t side, GLenum pname) const;

    static inline GLenum get_face(uint32_t side)
    {
        return side ? GL_BACK : GL_FRONT;
    }
};

#endif // VOGL_MATERIAL_STATE_H
