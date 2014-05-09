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

// File: vogl_state_vector.h
#ifndef VOGL_STATE_VECTOR_H
#define VOGL_STATE_VECTOR_H

#include "vogl_common.h"
#include "vogl_json.h"
#include "vogl_map.h"

class vogl_snapshot_context_info;
class vogl_state_vector;

const char *vogl_get_state_type_name(vogl_state_type s);
vogl_state_type vogl_get_state_type_from_name(const char *pName);
uint32_t vogl_get_state_type_size(vogl_state_type s);

class vogl_state_id
{
public:
    vogl_state_id(GLenum val, uint32_t index, bool indexed_variant)
        : m_enum_val(val),
          m_index(index),
          m_indexed_variant(indexed_variant)
    {
        VOGL_FUNC_TRACER
    }

    void clear()
    {
        VOGL_FUNC_TRACER

        m_enum_val = 0;
        m_index = 0;
        m_indexed_variant = false;
    }

    bool operator==(const vogl_state_id &rhs) const
    {
        //VOGL_FUNC_TRACER

        return (m_enum_val == rhs.m_enum_val) && (m_index == rhs.m_index) && (m_indexed_variant == rhs.m_indexed_variant);
    }

    bool operator!=(const vogl_state_id &rhs) const
    {
        //VOGL_FUNC_TRACER

        return !(*this == rhs);
    }

    bool operator<(const vogl_state_id &rhs) const
    {
        //VOGL_FUNC_TRACER

        if (m_enum_val < rhs.m_enum_val)
            return true;
        else if (m_enum_val == rhs.m_enum_val)
        {
            if (m_index < rhs.m_index)
                return true;
            else if (m_index == rhs.m_index)
                return m_indexed_variant < rhs.m_indexed_variant;
        }
        return false;
    }

    void set(GLenum val, uint32_t index, bool indexed_variant)
    {
        VOGL_FUNC_TRACER

        m_enum_val = val;
        m_index = index;
        m_indexed_variant = indexed_variant;
    }

    GLenum m_enum_val;
    uint32_t m_index;
    bool m_indexed_variant;
};

class vogl_state_data
{
    friend class vogl_state_vector;

public:
    vogl_state_data();
    vogl_state_data(GLenum enum_val, uint32_t index, uint32_t n, vogl_state_type data_type, bool indexed_variant);
    vogl_state_data(GLenum enum_val, uint32_t index, const void *pData, uint32_t element_size, bool indexed_variant);

    void init(GLenum enum_val, uint32_t index, uint32_t n, vogl_state_type data_type, bool indexed_variant);

    template <typename T>
    inline T *init_and_get_data_ptr(GLenum enum_val, uint32_t index, uint32_t n, vogl_state_type data_type, bool indexed_variant)
    {
        init(enum_val, index, n, data_type, indexed_variant);

        VOGL_ASSERT(sizeof(T) == get_data_type_size());

        return reinterpret_cast<T *>(m_data.get_ptr());
    }

    bool init(GLenum enum_val, uint32_t index, const void *pData, uint32_t element_size, bool indexed_variant);

    void debug_check();

    inline void clear()
    {
        m_id.clear();
        m_data_type = cSTInvalid;
        m_data.clear();
    }

    inline uint32_t get_data_type_size() const
    {
        return vogl_get_state_type_size(m_data_type);
    }

    template <typename T>
    const T *get_data_ptr() const
    {
        return reinterpret_cast<const T *>(m_data.get_ptr());
    }
    template <typename T>
    T *get_data_ptr()
    {
        return reinterpret_cast<T *>(m_data.get_ptr());
    }

    template <typename T>
    const T &get_element(uint32_t index) const
    {
        VOGL_ASSERT(index < m_num_elements);
        return get_data_ptr<T>()[index];
    }
    template <typename T>
    T &get_element(uint32_t index)
    {
        VOGL_ASSERT(index < m_num_elements);
        return get_data_ptr<T>()[index];
    }

    void swap(vogl_state_data &other);

    bool is_equal(const vogl_state_data &rhs) const;

    bool operator<(const vogl_state_data &rhs) const
    {
        return m_id < rhs.m_id;
    }

    inline const vogl_state_id &get_id() const
    {
        return m_id;
    }
    vogl_state_id &get_id()
    {
        return m_id;
    }

    inline GLenum get_enum_val() const
    {
        return m_id.m_enum_val;
    }
    inline uint32_t get_index() const
    {
        return m_id.m_index;
    }
    inline bool get_indexed_variant() const
    {
        return m_id.m_indexed_variant;
    }

    inline vogl_state_type get_data_type() const
    {
        return m_data_type;
    }
    inline uint32_t get_num_elements() const
    {
        return m_num_elements;
    }

    void get_bool(bool *pVals) const;
    void get_int(int *pVals) const;
    void get_uint(uint32_t *pVals) const;
    void get_enum(GLenum *pVals) const
    {
        return get_uint(pVals);
    }
    void get_int64(int64_t *pVals) const;
    void get_uint64(uint64_t *pVals) const;
    void get_float(float *pVals) const;
    void get_double(double *pVals) const;
    void get_pointer(void **ppVals) const;

    template <typename T>
    void get(T *pVals) const;

    bool serialize(json_node &node) const;
    bool deserialize(const json_node &node);

private:
    vogl_state_id m_id;

    vogl_state_type m_data_type;
    uint32_t m_num_elements;

    vogl::vector<uint8_t> m_data;
};

// These specializations can't be at class scope.
template <>
inline void vogl_state_data::get<bool>(bool *pVals) const
{
    return get_bool(pVals);
}
template <>
inline void vogl_state_data::get<int>(int *pVals) const
{
    return get_int(pVals);
}
template <>
inline void vogl_state_data::get<uint32_t>(uint32_t *pVals) const
{
    return get_uint(pVals);
}
template <>
inline void vogl_state_data::get<int64_t>(int64_t *pVals) const
{
    return get_int64(pVals);
}
template <>
inline void vogl_state_data::get<uint64_t>(uint64_t *pVals) const
{
    return get_uint64(pVals);
}
template <>
inline void vogl_state_data::get<float>(float *pVals) const
{
    return get_float(pVals);
}
template <>
inline void vogl_state_data::get<double>(double *pVals) const
{
    return get_double(pVals);
}
template <>
inline void vogl_state_data::get<void *>(void **ppVals) const
{
    return get_pointer(ppVals);
}

class vogl_state_vector
{
public:
    vogl_state_vector();

    void clear();

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);
    bool deserialize(const char *pChild_name, const json_node &parent_node, const vogl_blob_manager &blob_manager);

    bool operator==(const vogl_state_vector &rhs) const;
    bool operator!=(const vogl_state_vector &rhs) const
    {
        return !(*this == rhs);
    }

    bool insert(GLenum enum_val, uint32_t index, const void *pData, uint32_t element_size, bool indexed_variant = false);
    bool insert(const vogl_state_data &state_data);

    inline uint32_t size() const
    {
        return m_states.size();
    }

    typedef vogl::map<vogl_state_id, vogl_state_data> state_map;

    typedef state_map::const_iterator const_iterator;

    inline const_iterator begin() const
    {
        return m_states.begin();
    }
    inline const_iterator end() const
    {
        return m_states.end();
    }

    const state_map &get_states() const
    {
        return m_states;
    }
    state_map &get_states()
    {
        return m_states;
    }

    const vogl_state_data *find(GLenum enum_val, uint32_t index = 0, bool indexed_variant = false) const;

    // true on success, false on failure.
    // pVals is only modified on success.
    template <typename T>
    bool get(GLenum enum_val, uint32_t index, T *pVals, uint32_t n = 1, bool indexed_variant = false) const
    {
        VOGL_FUNC_TRACER

        const vogl_state_data *pData = find(enum_val, index, indexed_variant);
        if (!pData)
            return false;
        VOGL_ASSERT(n >= pData->get_num_elements());
        VOGL_NOTE_UNUSED(n);
        pData->get(pVals);
        return true;
    }

    // Returns the value, or the default.
    // Safely only returns the first value on states with multiple values.
    template <typename T>
    T get_value(GLenum enum_val, uint32_t index = 0, T def = (T)0, bool indexed_variant = false) const
    {
        VOGL_FUNC_TRACER

        const vogl_state_data *pData = find(enum_val, index, indexed_variant);
        if (!pData)
            return def;
        vogl::growable_array<T, 16> vals(pData->get_num_elements());
        pData->get(vals.get_ptr());
        return vals[0];
    }

protected:
    state_map m_states;
};

namespace vogl
{
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_state_id);
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_state_data);
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_state_vector);
}

#endif // VOGL_STATE_VECTOR_H
