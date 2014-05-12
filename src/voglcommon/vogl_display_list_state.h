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

// File: vogl_display_list_state.h
#ifndef VOGL_DISPLAY_LIST_STATE_H
#define VOGL_DISPLAY_LIST_STATE_H

#include "vogl_core.h"
#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"
#include "vogl_trace_file_reader.h"

//----------------------------------------------------------------------------------------------------------------------
// class vogl_display_list
//----------------------------------------------------------------------------------------------------------------------
class vogl_display_list
{
public:
    vogl_display_list();

    void clear();

    GLuint get_handle() const
    {
        return m_handle;
    }
    void set_handle(GLuint handle)
    {
        m_handle = handle;
    }

    bool is_valid() const
    {
        return m_valid;
    }
    bool is_generating() const
    {
        return m_generating;
    }

    const dynamic_string &get_xfont_name() const
    {
        return m_xfont_name;
    }
    int get_xfont_glyph() const
    {
        return m_xfont_glyph;
    }
    bool is_xfont() const
    {
        return m_xfont;
    }

    const vogl_trace_packet_array &get_packets() const
    {
        return m_packets;
    }
    vogl_trace_packet_array &get_packets()
    {
        return m_packets;
    }

    void init_xfont(const char *pName, int glyph);

    void begin_gen();
    void end_gen();

    bool serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes);

private:
    GLuint m_handle;
    vogl_trace_packet_array m_packets;

    dynamic_string m_xfont_name;
    int m_xfont_glyph;
    bool m_xfont;
    bool m_generating;
    bool m_valid;
};

typedef vogl::map<GLuint, vogl_display_list> vogl_display_list_map;

//----------------------------------------------------------------------------------------------------------------------
// class vogl_display_list_state
//----------------------------------------------------------------------------------------------------------------------
class vogl_display_list_state
{
public:
    vogl_display_list_state();
    vogl_display_list_state(const vogl_display_list_state &other);
    ~vogl_display_list_state();

    vogl_display_list_state &operator=(const vogl_display_list_state &rhs);

    void clear();

    uint32_t size() const
    {
        return m_display_lists.size();
    }

    vogl_display_list *find_list(GLuint list)
    {
        return m_display_lists.find_value(list);
    }

    const vogl_display_list_map &get_display_list_map() const
    {
        return m_display_lists;
    }
    vogl_display_list_map &get_display_list_map()
    {
        return m_display_lists;
    }

    bool remap_handles(vogl_handle_remapper &remapper);

    bool serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes);

    bool define_list(GLuint trace_handle, GLuint replay_handle, const vogl_display_list &list);

    bool gen_lists(GLuint first, GLsizei n, GLuint *pObject_handles = NULL);

    bool del_lists(GLuint first, GLsizei n);

    bool glx_font(const char *pFont, int first, int count, int listBase);

    static bool is_call_listable(gl_entrypoint_id_t func, const vogl_trace_packet &packet);

    void new_list(GLuint trace_handle, GLuint replay_handle);

    bool add_packet_to_list(GLuint handle, gl_entrypoint_id_t func, const vogl_trace_packet &packet);

    bool end_list(GLuint trace_handle);

    typedef void (*pBind_callback_func_ptr)(vogl_namespace_t handle_namespace, GLenum target, GLuint handle, void *pOpaque);

    bool parse_list_and_update_shadows(GLuint handle, pBind_callback_func_ptr pBind_callback, void *pBind_callback_opaque);
    bool parse_lists_and_update_shadows(GLsizei n, GLenum type, const GLvoid *lists, pBind_callback_func_ptr pBind_callback, void *pBind_callback_opaque);

private:
    vogl_display_list_map m_display_lists;
};

#endif // VOGL_DISPLAY_LIST_STATE_H
