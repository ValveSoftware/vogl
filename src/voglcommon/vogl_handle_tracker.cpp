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

//----------------------------------------------------------------------------------------------------------------------
// File: vogl_handle_tracker.cpp
//----------------------------------------------------------------------------------------------------------------------
#include "vogl_handle_tracker.h"

vogl_handle_tracker::vogl_handle_tracker()
{
    VOGL_FUNC_TRACER
}

vogl_handle_tracker::vogl_handle_tracker(vogl_namespace_t handle_namespace)
    : m_namespace(handle_namespace)
{
    VOGL_FUNC_TRACER
}

vogl_handle_tracker::~vogl_handle_tracker()
{
    VOGL_FUNC_TRACER
}

void vogl_handle_tracker::clear()
{
    VOGL_FUNC_TRACER

    m_handles.clear();
    m_inv_handles.clear();
}

void vogl_handle_tracker::get_handles(uint_vec &handles)
{
    VOGL_FUNC_TRACER

    handles.resize(0);
    handles.reserve(m_inv_handles.size());

    for (uint32_t handle = 0; handle < m_handles.size(); handle++)
    {
        const handle_def &def = m_handles[handle];
        if (def.is_valid())
            handles.push_back(handle);
    }
}

void vogl_handle_tracker::get_inv_handles(uint_vec &inv_handles)
{
    VOGL_FUNC_TRACER

    inv_handles.resize(0);
    inv_handles.reserve(m_inv_handles.size());

    for (uint32_t handle = 0; handle < m_handles.size(); handle++)
    {
        const handle_def &def = m_handles[handle];
        if (def.is_valid())
            inv_handles.push_back(def.get_inv_handle());
    }
}

bool vogl_handle_tracker::insert(handle_t handle, handle_t inv_handle, GLenum target)
{
    VOGL_FUNC_TRACER

    if (is_valid_handle(handle))
    {
        VOGL_ASSERT(m_handles[handle].get_inv_handle() == inv_handle);
        VOGL_ASSERT(is_valid_inv_handle(inv_handle));
        VOGL_ASSERT(m_inv_handles.value(inv_handle) == handle);
        return false;
    }

    if (is_valid_inv_handle(inv_handle))
        return false;

    handle_def *pHandle = m_handles.ensure_present(handle);
    pHandle->init(handle, inv_handle, target);

    bool inv_insert_success = m_inv_handles.insert(inv_handle, handle).second;
    VOGL_ASSERT(inv_insert_success);
    VOGL_NOTE_UNUSED(inv_insert_success);

    return true;
}

bool vogl_handle_tracker::conditional_update(handle_t handle, handle_t inv_handle, GLenum compare_target, GLenum target)
{
    VOGL_FUNC_TRACER

    if (is_valid_handle(handle))
    {
        handle_def &def = m_handles.get_or_create_element(handle);
        if (def.get_target() == compare_target)
            def.set_target(target);
        return true;
    }

    return insert(handle, inv_handle, target);
}

bool vogl_handle_tracker::update(handle_t handle, handle_t inv_handle, GLenum target)
{
    VOGL_FUNC_TRACER

    if (is_valid_handle(handle))
    {
        handle_def &def = m_handles.get_or_create_element(handle);

        if ((def.get_target() != GL_NONE) && (target != def.get_target()))
            vogl_debug_printf("Object target is being changed from %s to %s, handle %u inv handle %u, namespace %s\n", get_gl_enums().find_gl_name(def.get_target()), get_gl_enums().find_gl_name(target), def.get_handle(), def.get_inv_handle(), vogl_get_namespace_name(m_namespace));

        def.set_target(target);
        return true;
    }

    return insert(handle, inv_handle, target);
}

bool vogl_handle_tracker::update_inv(handle_t inv_handle, handle_t handle, GLenum target)
{
    VOGL_FUNC_TRACER

    handle_t actual_handle;
    if (map_inv_handle_to_handle(inv_handle, actual_handle))
    {
        VOGL_ASSERT(is_valid_handle(actual_handle));

        handle_def &def = m_handles.get_or_create_element(actual_handle);

        if ((def.get_target() != GL_NONE) && (target != def.get_target()))
            vogl_debug_printf("Object target is being changed from %s to %s, handle %u inv handle %u, namespace %s\n", get_gl_enums().find_gl_name(def.get_target()), get_gl_enums().find_gl_name(target), def.get_handle(), def.get_inv_handle(), vogl_get_namespace_name(m_namespace));

        def.set_target(target);

        return true;
    }

    return insert(handle, inv_handle, target);
}

bool vogl_handle_tracker::set_target(handle_t handle, GLenum target)
{
    VOGL_FUNC_TRACER

    if (!is_valid_handle(handle))
        return false;
    m_handles.get_or_create_element(handle).set_target(target);
    return true;
}

bool vogl_handle_tracker::set_target_inv(handle_t inv_handle, GLenum target)
{
    VOGL_FUNC_TRACER

    handle_hash_map_t::const_iterator it(m_inv_handles.find(inv_handle));
    if (it == m_inv_handles.end())
        return false;
    return set_target(it->second, target);
}

bool vogl_handle_tracker::map_inv_handle_to_handle(handle_t inv_handle, handle_t &handle) const
{
    VOGL_FUNC_TRACER

    handle_hash_map_t::const_iterator it(m_inv_handles.find(inv_handle));
    if (it == m_inv_handles.end())
        return false;

    handle = it->second;

    VOGL_ASSERT(contains(handle));

    return handle != 0;
}

bool vogl_handle_tracker::map_handle_to_inv_handle(handle_t handle, handle_t &inv_handle) const
{
    VOGL_FUNC_TRACER

    if (handle >= m_handles.size())
        return false;

    if (!m_handles[handle].is_valid())
        return false;

    inv_handle = m_handles[handle].get_inv_handle();

    VOGL_ASSERT(contains_inv(inv_handle));

    return true;
}

GLenum vogl_handle_tracker::get_target(handle_t handle) const
{
    VOGL_FUNC_TRACER

    if (!is_valid_handle(handle))
        return GL_NONE;
    return m_handles[handle].get_target();
}

GLenum vogl_handle_tracker::get_target_inv(handle_t inv_handle) const
{
    VOGL_FUNC_TRACER

    handle_t handle;
    if (!map_inv_handle_to_handle(inv_handle, handle))
        return GL_NONE;
    VOGL_ASSERT(is_valid_handle(handle));
    return m_handles[handle].get_target();
}

bool vogl_handle_tracker::erase(handle_t handle)
{
    VOGL_FUNC_TRACER

    if (!is_valid_handle(handle))
        return false;

    VOGL_ASSERT(m_handles[handle].get_handle() == handle);

    uint32_t inv_handle = m_handles[handle].get_inv_handle();

    VOGL_ASSERT(m_inv_handles.value(inv_handle) == handle);

    m_handles.get_or_create_element(handle).clear();
    VOGL_ASSERT(!m_handles[handle].is_valid());

    bool success = m_inv_handles.erase(inv_handle);
    VOGL_NOTE_UNUSED(success);
    VOGL_ASSERT(success);

    return true;
}

bool vogl_handle_tracker::erase_inv(handle_t inv_handle)
{
    VOGL_FUNC_TRACER

    handle_hash_map_t::const_iterator it(m_inv_handles.find(inv_handle));
    if (it == m_inv_handles.end())
        return false;

    uint32_t handle = it->second;
    if (!is_valid_handle(handle))
    {
        VOGL_ASSERT_ALWAYS;
    }
    else
    {
        VOGL_ASSERT(m_handles[handle].get_handle() == handle);
        VOGL_ASSERT(m_handles[handle].get_inv_handle() == inv_handle);
        m_handles.get_or_create_element(handle).clear();
        VOGL_ASSERT(!m_handles[handle].is_valid());
    }

    m_inv_handles.erase(inv_handle);
    return true;
}

bool vogl_handle_tracker::invert(vogl_handle_tracker &inverted_map) const
{
    VOGL_FUNC_TRACER

    inverted_map.clear();

    for (uint32_t handle = 0; handle < m_handles.size(); handle++)
    {
        const handle_def &def = m_handles[handle];
        if (!def.is_valid())
            continue;

        if (!inverted_map.insert(def.get_inv_handle(), def.get_handle(), def.get_target()))
            return false;
    }

    return true;
}

bool vogl_handle_tracker::check() const
{
    VOGL_FUNC_TRACER

    handle_hash_map_t forward_to_inverse_map;
    forward_to_inverse_map.reserve(m_inv_handles.size());
    for (handle_hash_map_t::const_iterator it = m_inv_handles.begin(); it != m_inv_handles.end(); ++it)
        if (!forward_to_inverse_map.insert(it->second, it->first).second)
            return false;

    uint32_t total_found = 0;
    for (uint32_t handle = 0; handle < m_handles.size(); handle++)
    {
        if (!m_handles.is_present(handle))
        {
            if (forward_to_inverse_map.contains(handle))
                return false;
            continue;
        }

        const handle_def &def = m_handles[handle];
        if (!def.is_valid())
        {
            if (forward_to_inverse_map.contains(handle))
                return false;
            continue;
        }

        const uint32_t *pInv_handle = forward_to_inverse_map.find_value(handle);
        if (!pInv_handle)
            return false;

        uint32_t inv_handle = *pInv_handle;

        if (m_inv_handles.value(inv_handle) != handle)
            return false;

        if (def.get_handle() != handle)
            return false;
        if (def.get_inv_handle() != inv_handle)
            return false;

        total_found++;
    }

    if (total_found != m_inv_handles.size())
        return false;

    return true;
}

bool vogl_handle_tracker::operator==(const vogl_handle_tracker &other) const
{
    VOGL_FUNC_TRACER

    if (m_inv_handles != other.m_inv_handles)
        return false;

    if (m_handles != other.m_handles)
        return false;

    return true;
}

bool vogl_handle_tracker::serialize(json_node &node) const
{
    VOGL_FUNC_TRACER

    node.init_array();

    for (uint32_t i = 0; i < m_handles.size(); i++)
    {
        if (!is_valid_handle(i))
            continue;

        json_node &obj = node.add_object();
        obj.add_key_value("handle", m_handles[i].get_handle());
        obj.add_key_value("inv_handle", m_handles[i].get_inv_handle());
        obj.add_key_value("target", m_handles[i].get_target());
    }

    return true;
}

bool vogl_handle_tracker::deserialize(const json_node &node)
{
    VOGL_FUNC_TRACER

    clear();

    for (uint32_t i = 0; i < node.size(); i++)
    {
        const json_node *pObj = node.get_child(i);
        if ((!pObj) || (!pObj->is_object()))
            return false;

        uint32_t handle, inv_handle;
        GLenum target;
        if (!pObj->get_value_as_uint32("handle", handle) || !pObj->get_value_as_uint32("inv_handle", inv_handle) || !pObj->get_value_as_uint32("target", target))
            return false;

        if (!insert(handle, inv_handle, target))
            return false;
    }

    return true;
}
