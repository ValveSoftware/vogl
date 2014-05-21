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

// File: vogl_display_list_state.cpp
#include "vogl_display_list_state.h"

vogl_display_list::vogl_display_list()
    : m_handle(0),
      m_xfont_glyph(0),
      m_xfont(false),
      m_generating(false),
      m_valid(false)
{
    VOGL_FUNC_TRACER
}

void vogl_display_list::clear()
{
    VOGL_FUNC_TRACER

    m_xfont_name.clear();
    m_xfont_glyph = 0;
    m_xfont = false;
    m_packets.clear();
    m_generating = false;
    m_valid = false;
}

void vogl_display_list::init_xfont(const char *pName, int glyph)
{
    VOGL_FUNC_TRACER

    m_packets.clear();
    m_xfont_name = pName ? pName : "";
    m_xfont_glyph = glyph;
    m_xfont = true;
    m_generating = false;
    m_valid = true;
}

void vogl_display_list::begin_gen()
{
    VOGL_FUNC_TRACER

    clear();
    m_generating = true;
}

void vogl_display_list::end_gen()
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(m_generating);
    m_generating = false;
    m_valid = true;
}

bool vogl_display_list::serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const
{
    VOGL_FUNC_TRACER

    node.add_key_value("handle", m_handle);
    node.add_key_value("valid", m_valid);
    node.add_key_value("generating", m_generating);
    node.add_key_value("xfont", m_xfont);

    if (m_xfont)
    {
        node.add_key_value("xfont_glyph", m_xfont_glyph);
        node.add_key_value("xfont_name", m_xfont_name);
    }

    if (m_packets.size())
    {
        vogl_trace_packet::json_serialize_params params;
        params.m_pBlob_manager = &blob_manager;

        json_node &packets_array = node.add_array("packets");
        for (uint32_t i = 0; i < m_packets.size(); i++)
        {
            const uint8_vec &packet_buf = m_packets.get_packet_buf(i);

            vogl_trace_packet packet(pCtypes);
            if (!packet.deserialize(packet_buf.get_ptr(), packet_buf.size(), true))
                return false;

            if (!packet.json_serialize(packets_array.add_object(), params))
                return false;
        }
    }

    return true;
}

bool vogl_display_list::deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes)
{
    VOGL_FUNC_TRACER

    clear();

    m_handle = node.value_as_uint32("handle");
    m_valid = node.value_as_bool("valid", true);
    m_generating = node.value_as_bool("generating");
    m_xfont = node.value_as_bool("xfont");

    if (m_xfont)
    {
        m_xfont_glyph = node.value_as_int("xfont_glyph");
        m_xfont_name = node.value_as_string("xfont_name");
    }

    const json_node *pPackets_array = node.find_child_array("packets");
    if (pPackets_array)
    {
        if (!pPackets_array->are_all_children_objects())
        {
            clear();
            return false;
        }

        vogl_trace_packet packet(pCtypes);

        m_packets.resize(pPackets_array->size());
        for (uint32_t i = 0; i < pPackets_array->size(); i++)
        {
            if (!packet.json_deserialize(*pPackets_array->get_child(i), "<display_list>", &blob_manager))
            {
                clear();
                return false;
            }

            if (!packet.serialize(m_packets.get_packet_buf(i)))
            {
                clear();
                return false;
            }
        }
    }

    return true;
}

vogl_display_list_state::vogl_display_list_state()
{
    VOGL_FUNC_TRACER
}

vogl_display_list_state::vogl_display_list_state(const vogl_display_list_state &other)
{
    VOGL_FUNC_TRACER

    *this = other;
}

vogl_display_list_state::~vogl_display_list_state()
{
    VOGL_FUNC_TRACER
}

vogl_display_list_state &vogl_display_list_state::operator=(const vogl_display_list_state &rhs)
{
    VOGL_FUNC_TRACER

    if (this == &rhs)
        return *this;

    clear();

    m_display_lists = rhs.m_display_lists;

    return *this;
}

void vogl_display_list_state::clear()
{
    VOGL_FUNC_TRACER

    m_display_lists.clear();
}

bool vogl_display_list_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    vogl_display_list_map new_display_lists;

    for (vogl_display_list_map::const_iterator it = m_display_lists.begin(); it != m_display_lists.end(); ++it)
    {
        uint32_t new_handle = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_LISTS, it->first));

        new_display_lists[new_handle] = it->second;
    }

    new_display_lists.swap(m_display_lists);

    return true;
}

bool vogl_display_list_state::serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const
{
    VOGL_FUNC_TRACER

    if (m_display_lists.size())
    {
        json_node &lists_node = node.add_object("lists");

        for (vogl_display_list_map::const_iterator it = m_display_lists.begin(); it != m_display_lists.end(); ++it)
        {
            // It's OK if the display list isn't valid yet, it might just have been genned and not used.
            if (!it->second.serialize(lists_node.add_object(uint_to_string(it->first)), blob_manager, pCtypes))
                return false;
        }
    }

    return true;
}

bool vogl_display_list_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes)
{
    VOGL_FUNC_TRACER

    clear();

    const json_node *pLists_node = node.find_child_object("lists");
    if (pLists_node)
    {
        if (!pLists_node->are_all_children_objects())
            return false;

        for (uint32_t i = 0; i < pLists_node->size(); i++)
        {
            if (!m_display_lists[string_to_uint(pLists_node->get_key(i).get_ptr())].deserialize(*pLists_node->get_child(i), blob_manager, pCtypes))
            {
                clear();
                return false;
            }
        }
    }

    return true;
}

bool vogl_display_list_state::define_list(GLuint trace_handle, GLuint replay_handle, const vogl_display_list &list)
{
    VOGL_FUNC_TRACER

    vogl_display_list_map::insert_result res(m_display_lists.insert(trace_handle));
    if (!res.second)
        return false;

    vogl_display_list_map::iterator it = res.first;

    vogl_display_list &new_list = it->second;
    new_list = list;
    new_list.set_handle(replay_handle);

    return true;
}

void vogl_display_list_state::new_list(GLuint trace_handle, GLuint replay_handle)
{
    VOGL_FUNC_TRACER

    vogl_display_list_map::insert_result res(m_display_lists.insert(trace_handle));
    vogl_display_list_map::iterator it = res.first;
    vogl_display_list &new_list = it->second;

    new_list.clear();
    new_list.set_handle(replay_handle);
    new_list.begin_gen();
}

bool vogl_display_list_state::end_list(GLuint trace_handle)
{
    VOGL_FUNC_TRACER

    vogl_display_list_map::iterator it = m_display_lists.find(trace_handle);
    if (it == m_display_lists.end())
        return false;

    it->second.end_gen();
    return true;
}

bool vogl_display_list_state::gen_lists(GLuint first, GLsizei n, GLuint *pObject_handles)
{
    VOGL_FUNC_TRACER

    bool success = true;
    for (GLsizei i = 0; i < n; i++)
    {
        vogl_display_list_map::insert_result res(m_display_lists.insert(first + i));
        if (!res.second)
        {
            vogl_error_printf("Failed inserting list handle %u into display list shadow!\n", first + i);
            success = false;
        }
        else
        {
            (res.first)->second.set_handle(pObject_handles ? pObject_handles[i] : first + i);
        }
    }
    return success;
}

bool vogl_display_list_state::del_lists(GLuint first, GLsizei n)
{
    VOGL_FUNC_TRACER

    bool success = true;
    for (GLsizei i = 0; i < n; i++)
    {
        if (!m_display_lists.erase(first + i))
            success = false;
    }
    return success;
}

bool vogl_display_list_state::glx_font(const char *pFont, int first, int count, int listBase)
{
    VOGL_FUNC_TRACER

    bool success = true;
    for (int i = 0; i < count; i++)
    {
        int handle = listBase + i;
        if (handle < 1)
        {
            vogl_error_printf("Invalid display list handle %i\n", handle);
            success = false;
            continue;
        }

        vogl_display_list *pList = &m_display_lists[handle];
        pList->init_xfont(pFont, first + i);
    }

    return success;
}

bool vogl_display_list_state::is_call_listable(gl_entrypoint_id_t func, const vogl_trace_packet &packet)
{
    VOGL_FUNC_TRACER

    if ((!g_vogl_entrypoint_descs[func].m_is_listable) || (!g_vogl_entrypoint_descs[func].m_whitelisted_for_displaylists))
        return false;

    // These cases don't go into display lists, they are executed immediately.
    if (((func == VOGL_ENTRYPOINT_glTexImage1D) && (packet.get_param_value<GLenum>(0) == GL_PROXY_TEXTURE_1D)) ||
        ((func == VOGL_ENTRYPOINT_glTexImage2D) && (packet.get_param_value<GLenum>(0) == GL_PROXY_TEXTURE_2D)) ||
        ((func == VOGL_ENTRYPOINT_glTexImage3D) && (packet.get_param_value<GLenum>(0) == GL_PROXY_TEXTURE_3D)))
    {
        return false;
    }

    return true;
}

bool vogl_display_list_state::add_packet_to_list(GLuint handle, gl_entrypoint_id_t func, const vogl_trace_packet &packet)
{
    VOGL_FUNC_TRACER
    VOGL_NOTE_UNUSED(func);

    vogl_display_list *pList = m_display_lists.find_value(handle);
    VOGL_ASSERT(pList);
    if (!pList)
    {
        vogl_error_printf("Failed serializing trace packet into display list shadow! List %u was not genned.\n", handle);
        return false;
    }

    uint8_vec buf;
    if (!packet.serialize(buf))
    {
        vogl_error_printf("Failed serializing trace packet into display list shadow! List handle %u.\n", handle);
        return false;
    }

    pList->get_packets().push_back(buf);
    return true;
}

bool vogl_display_list_state::parse_list_and_update_shadows(GLuint handle, pBind_callback_func_ptr pBind_callback, void *pBind_callback_opaque)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(pBind_callback);

    const vogl_display_list *pList = find_list(handle);
    if (!pList)
        return false;

    const vogl_trace_packet_array &packets = pList->get_packets();

    vogl_trace_packet trace_packet(&get_vogl_process_gl_ctypes());

    for (uint32_t packet_index = 0; packet_index < packets.size(); packet_index++)
    {
        if (packets.get_packet_type(packet_index) != cTSPTGLEntrypoint)
        {
            VOGL_ASSERT_ALWAYS;
            continue;
        }

        // Quickly skip by those packets we don't care about
        // TODO: If display lists are used a lot we could precompute a flag for each one that indicates whether or not they should be parsed for binds.
        const vogl_trace_gl_entrypoint_packet &gl_packet = packets.get_packet<vogl_trace_gl_entrypoint_packet>(packet_index);
        switch (gl_packet.m_entrypoint_id)
        {
            case VOGL_ENTRYPOINT_glBindTexture:
            case VOGL_ENTRYPOINT_glBindTextureEXT:
            case VOGL_ENTRYPOINT_glBindMultiTextureEXT:
            case VOGL_ENTRYPOINT_glBindBuffer:
            case VOGL_ENTRYPOINT_glBindVertexArray:
            case VOGL_ENTRYPOINT_glBeginQuery:
            case VOGL_ENTRYPOINT_glBeginQueryARB:
            case VOGL_ENTRYPOINT_glCallList:
            case VOGL_ENTRYPOINT_glCallLists:
                break;
            default:
                continue;
        }

        if (!trace_packet.deserialize(packets.get_packet_buf(packet_index), false))
        {
            vogl_error_printf("Failed parsing GL entrypoint packet in display list %u\n", handle);
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        switch (trace_packet.get_entrypoint_id())
        {
            case VOGL_ENTRYPOINT_glBindTexture:
            case VOGL_ENTRYPOINT_glBindTextureEXT:
            {
                (*pBind_callback)(VOGL_NAMESPACE_TEXTURES, trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLuint>(1), pBind_callback_opaque);
                break;
            }
            case VOGL_ENTRYPOINT_glBindMultiTextureEXT:
            {
                (*pBind_callback)(VOGL_NAMESPACE_TEXTURES, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), pBind_callback_opaque);
                break;
            }
            case VOGL_ENTRYPOINT_glBindBuffer:
            {
                (*pBind_callback)(VOGL_NAMESPACE_BUFFERS, trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLuint>(1), pBind_callback_opaque);
                break;
            }
            case VOGL_ENTRYPOINT_glBindVertexArray:
            {
                (*pBind_callback)(VOGL_NAMESPACE_VERTEX_ARRAYS, GL_NONE, trace_packet.get_param_value<GLuint>(0), pBind_callback_opaque);
                break;
            }
            case VOGL_ENTRYPOINT_glBeginQuery:
            case VOGL_ENTRYPOINT_glBeginQueryARB:
            {
                (*pBind_callback)(VOGL_NAMESPACE_QUERIES, trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLuint>(1), pBind_callback_opaque);
                break;
            }
            case VOGL_ENTRYPOINT_glCallList:
            case VOGL_ENTRYPOINT_glCallLists:
            {
                vogl_warning_printf("Recursive display lists are not currently supported\n");
                VOGL_ASSERT_ALWAYS;
                return false;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }
        }
    }

    return true;
}

bool vogl_display_list_state::parse_lists_and_update_shadows(GLsizei n, GLenum type, const GLvoid *lists, pBind_callback_func_ptr pBind_callback, void *pBind_callback_opaque)
{
    VOGL_FUNC_TRACER

    uint32_t type_size = vogl_get_gl_type_size(type);
    if (!type_size)
    {
        vogl_error_printf("type is invalid\n");
        return false;
    }

    if ((n) && (!lists))
    {
        vogl_error_printf("lists param is NULL\n");
        return false;
    }

    GLuint list_base = 0;
    {
        //vogl_scoped_gl_error_absorber gl_error_absorber(pContext); VOGL_NOTE_UNUSED(gl_error_absorber);
        GL_ENTRYPOINT(glGetIntegerv)(GL_LIST_BASE, reinterpret_cast<GLint *>(&list_base));
    }

    bool success = true;

    const uint8_t *pTrace_lists_ptr = static_cast<const uint8_t *>(lists);
    for (GLsizei i = 0; i < n; i++)
    {
        GLint handle = list_base;
        switch (type)
        {
            case GL_BYTE:
            {
                handle += *reinterpret_cast<const signed char *>(pTrace_lists_ptr);
                pTrace_lists_ptr++;
                break;
            }
            case GL_UNSIGNED_BYTE:
            {
                handle += *pTrace_lists_ptr;
                pTrace_lists_ptr++;
                break;
            }
            case GL_SHORT:
            {
                handle += *reinterpret_cast<const int16_t *>(pTrace_lists_ptr);
                pTrace_lists_ptr += sizeof(int16_t);
                break;
            }
            case GL_UNSIGNED_SHORT:
            {
                handle += *reinterpret_cast<const uint16_t *>(pTrace_lists_ptr);
                pTrace_lists_ptr += sizeof(uint16_t);
                break;
            }
            case GL_INT:
            {
                handle += *reinterpret_cast<const int32_t *>(pTrace_lists_ptr);
                pTrace_lists_ptr += sizeof(int32_t);
                break;
            }
            case GL_UNSIGNED_INT:
            {
                handle += *reinterpret_cast<const uint32_t *>(pTrace_lists_ptr);
                pTrace_lists_ptr += sizeof(uint32_t);
                break;
            }
            case GL_FLOAT:
            {
                handle += static_cast<GLint>(*reinterpret_cast<const float *>(pTrace_lists_ptr));
                pTrace_lists_ptr += sizeof(float);
                break;
            }
            case GL_2_BYTES:
            {
                handle += ((pTrace_lists_ptr[0] << 8U) + pTrace_lists_ptr[1]);
                pTrace_lists_ptr += 2;
                break;
            }
            case GL_3_BYTES:
            {
                handle += ((pTrace_lists_ptr[0] << 16U) + (pTrace_lists_ptr[1] << 8U) + pTrace_lists_ptr[2]);
                pTrace_lists_ptr += 3;
                break;
            }
            case GL_4_BYTES:
            {
                handle += ((pTrace_lists_ptr[0] << 24U) + (pTrace_lists_ptr[1] << 16U) + (pTrace_lists_ptr[2] << 8U) + pTrace_lists_ptr[3]);
                pTrace_lists_ptr += 4;
                break;
            }
            default:
            {
                vogl_error_printf("Invalid type parameter (0x%08X)\n", type);
                return false;
            }
        }

        if (handle <= 0)
        {
            vogl_error_printf("Trace handle after adding list base is negative (%i), skipping this list index\n", handle);
            success = false;
        }
        else
        {
            if (!parse_list_and_update_shadows(handle, pBind_callback, pBind_callback_opaque))
                success = false;
        }
    }

    return success;
}
