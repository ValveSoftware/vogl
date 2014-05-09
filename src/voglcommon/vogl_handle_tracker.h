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
// File: vogl_handle_tracker.h
//----------------------------------------------------------------------------------------------------------------------
#ifndef VOGL_HANDLE_TRACKER_H
#define VOGL_HANDLE_TRACKER_H

#include "vogl_common.h"
#include "vogl_hash_map.h"
#include "vogl_sparse_vector.h"
#include "vogl_json.h"

class vogl_handle_tracker
{
public:
    typedef uint32_t handle_t;

    class handle_def
    {
        friend class vogl_handle_tracker;

    public:
        handle_def()
        {
            clear();
        }
        handle_def(handle_t handle, handle_t inv_handle, GLenum target)
            : m_handle(handle), m_inv_handle(inv_handle), m_target(target), m_is_valid(true)
        {
            VOGL_FUNC_TRACER
        }

        void init(handle_t handle, handle_t inv_handle, GLenum target)
        {
            m_handle = handle;
            m_inv_handle = inv_handle;
            m_target = target;
            m_is_valid = true;
            VOGL_FUNC_TRACER
        }

        void clear()
        {
            utils::zero_object(*this);
        }

        bool is_valid() const
        {
            return m_is_valid;
        }

        handle_t get_handle() const
        {
            return m_handle;
        }
        handle_t get_inv_handle() const
        {
            return m_inv_handle;
        }
        GLenum get_target() const
        {
            return m_target;
        }

        void set_target(GLenum target)
        {
            m_target = target;
        }

        bool operator==(const handle_def &rhs) const
        {
            return (m_handle == rhs.m_handle) && (m_inv_handle == rhs.m_inv_handle) && (m_target == rhs.m_target) && (m_is_valid == rhs.m_is_valid);
        }

    private:
        handle_t m_handle;
        handle_t m_inv_handle;
        GLenum m_target;
        bool m_is_valid;

        void set_handle(handle_t handle)
        {
            m_handle = handle;
        }
        void set_inv_handle(handle_t inv_handle)
        {
            m_inv_handle = inv_handle;
        }
    };

    typedef vogl::sparse_vector<handle_def, 5> handle_def_vec;
    typedef vogl::hash_map<handle_t, uint32_t> handle_hash_map_t;

    vogl_handle_tracker();
    vogl_handle_tracker(vogl_namespace_t handle_namespace);
    ~vogl_handle_tracker();

    void clear();

    vogl_namespace_t get_namespace() const
    {
        return m_namespace;
    }
    void set_namespace(vogl_namespace_t handle_namespace)
    {
        m_namespace = handle_namespace;
    }

    uint32_t get_total_valid_handles() const
    {
        return m_inv_handles.size();
    }

    // size() and operator[] allow you to iterate over the entire handle namespace, but beware that not all handles may be valid!
    // size() is not necessarily always equal to get_total_valid_handles()!
    uint32_t size() const
    {
        return m_handles.size();
    }
    const handle_def &operator[](uint32_t index) const
    {
        return m_handles[index];
    }

    void get_handles(uint_vec &handles);
    void get_inv_handles(uint_vec &inv_handles);

    const handle_def_vec &get_handles() const
    {
        return m_handles;
    }
    const handle_hash_map_t get_inv_handles() const
    {
        return m_inv_handles;
    }

    // true if insertion occured, otherwise it already existed and wasn't updated.
    bool insert(handle_t handle, handle_t inv_handle, GLenum target);
    //bool insert(handle_t handle, GLenum target) { return insert(handle, handle, target); }

    // If handle is missing, the handle/inv_handle/target is added.
    // Otherwise, it's only updated if the target is compare_target (and the inv_handle is ignored).
    bool conditional_update(handle_t handle, handle_t inv_handle, GLenum compare_target, GLenum target);
    bool conditional_update(handle_t handle, GLenum compare_target, GLenum target)
    {
        return conditional_update(handle, handle, compare_target, target);
    }

    // If handle is missing, the handle/inv_handle/target is added.
    // Otherwise, the target of the handle is changed, and the inv_handle is ignored.
    // Returns false if the insertion failed.
    bool update(handle_t handle, handle_t inv_handle, GLenum target);
    bool update(handle_t handle, GLenum target)
    {
        return update(handle, handle, target);
    }

    bool update_inv(handle_t inv_handle, handle_t handle, GLenum target);

    // Sets handle's target, false if the handle does not exist.
    bool set_target(handle_t handle, GLenum target);
    bool set_target_inv(handle_t inv_handle, GLenum target);

    bool contains(handle_t handle) const
    {
        return is_valid_handle(handle);
    }
    bool contains_inv(handle_t inv_handle) const
    {
        return is_valid_inv_handle(inv_handle);
    }

    // Given an inverse handle, return the handle associated with it, or false.
    // Does not set handle on error.
    bool map_inv_handle_to_handle(handle_t inv_handle, handle_t &handle) const;

    // Given a handle, return the inverse handle associated with it, or false.
    // Does not set inv_handle on error.
    bool map_handle_to_inv_handle(handle_t handle, handle_t &inv_handle) const;

    GLenum get_target(handle_t handle) const;
    GLenum get_target_inv(handle_t inv_handle) const;

    // true if erasure successfully occurred
    bool erase(handle_t handle);
    bool erase_inv(handle_t inv_handle);

    bool invert(vogl_handle_tracker &inverted_map) const;

    // Consistency check
    bool check() const;

    bool operator==(const vogl_handle_tracker &other) const;
    bool operator!=(const vogl_handle_tracker &other) const
    {
        return !(*this == other);
    }

    bool serialize(json_node &node) const;
    bool deserialize(const json_node &node);

private:
    vogl_namespace_t m_namespace;
    handle_def_vec m_handles;
    handle_hash_map_t m_inv_handles;

    bool is_valid_handle(handle_t handle) const
    {
        return (handle < m_handles.size()) && (m_handles[handle].is_valid());
    }
    bool is_valid_inv_handle(handle_t inv_handle) const
    {
        return m_inv_handles.contains(inv_handle);
    }
};

#endif // VOGL_HANDLE_TRACKER_H
