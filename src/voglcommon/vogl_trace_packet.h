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

#ifndef VOGL_TRACE_PACKET_H
#define VOGL_TRACE_PACKET_H

#include "vogl_core.h"
#include "vogl_data_stream.h"
#include "vogl_value.h"
#include "vogl_console.h"

#include "vogl_trace_stream_types.h"
#include "vogl_common.h"
#include "vogl_blob_manager.h"

#include "TypeTraits.h"

//----------------------------------------------------------------------------------------------------------------------
// class vogl_client_memory_array
//----------------------------------------------------------------------------------------------------------------------
class vogl_client_memory_array
{
public:
    inline vogl_client_memory_array(vogl_ctype_t element_ctype, const void *p, uint32_t element_size, uint32_t num_elements)
        : m_element_ctype(element_ctype), m_pPtr(static_cast<const uint8_t *>(p)), m_element_size(element_size), m_num_elements(num_elements)
    {
        VOGL_FUNC_TRACER
    }

    inline vogl_ctype_t get_element_ctype() const
    {
        return m_element_ctype;
    }

    inline const void *get_ptr() const
    {
        return m_pPtr;
    }

    template <typename T>
    inline const T *get_value_ptr() const
    {
        return reinterpret_cast<const T *>(m_pPtr);
    }

    inline uint32_t size() const
    {
        return m_num_elements;
    }
    inline uint32_t get_element_size() const
    {
        return m_element_size;
    }
    inline uint32_t get_size_in_bytes() const
    {
        return m_element_size * m_num_elements;
    }

    template <typename T>
    inline T get_element(uint32_t index) const
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(index < m_num_elements);
        T result;
        if ((!m_pPtr) || (!m_num_elements))
        {
            VOGL_ASSERT_ALWAYS;
            utils::zero_object(result);
        }
        else
        {
            uint32_t n = math::minimum<uint32_t>(sizeof(T), m_element_size);
            if (n < sizeof(T))
                utils::zero_object(result);
            memcpy(&result, m_pPtr + m_element_size * index, n);
        }
        return result;
    }

    vogl_ctype_t m_element_ctype;

    const uint8_t *m_pPtr;
    uint32_t m_element_size;
    uint32_t m_num_elements;
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_trace_packet
// Keep this in sync with class vogl_entrypoint_serializer
// This object is somewhat expensive to construct - so keep it around!
//----------------------------------------------------------------------------------------------------------------------
class vogl_trace_packet
{
public:
    inline vogl_trace_packet(const vogl_ctypes *pCtypes)
        : m_pCTypes(pCtypes),
          m_total_params(0),
          m_has_return_value(false),
          m_is_valid(false)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSUME(VOGL_INVALID_CTYPE == 0);

        utils::zero_object(m_packet);
    }

    inline void clear()
    {
        VOGL_FUNC_TRACER

        reset();
    }

    // reset() tries its best to not free any memory.
    inline void reset()
    {
        VOGL_FUNC_TRACER

        m_is_valid = false;
        m_total_params = 0;
        m_has_return_value = false;

        utils::zero_object(m_packet);

        m_packet.init();

        utils::zero_object(m_param_ctype);
        utils::zero_object(m_param_data);
        utils::zero_object(m_param_size);

        for (uint32_t i = 0; i < cMaxParams; i++)
            m_client_memory_descs[i].clear();

        m_client_memory.resize(0);
        m_key_value_map.reset();
    }

    void set_ctypes(const vogl_ctypes *pCtypes)
    {
        VOGL_FUNC_TRACER

        m_pCTypes = pCtypes;
    }

    const vogl_ctypes *get_ctypes() const
    {
        VOGL_FUNC_TRACER

        return m_pCTypes;
    }

    bool compare(const vogl_trace_packet &other, bool deep) const;

    bool check() const;

    bool pretty_print(dynamic_string &str, bool type_info) const;

    // serialization/deserialization
    bool serialize(data_stream &stream) const;
    bool serialize(uint8_vec &buf) const;

    bool deserialize(const uint8_t *pPacket_data, uint32_t packet_data_buf_size, bool check_crc);
    bool deserialize(const uint8_vec &packet_buf, bool check_crc);

    class json_serialize_params
    {
    public:
        json_serialize_params()
        {
            VOGL_FUNC_TRACER

            clear();
        }

        void clear()
        {
            VOGL_FUNC_TRACER

            m_pBlob_manager = NULL;
            m_cur_frame = 0;
            m_blob_file_size_threshold = 2048;
            m_write_debug_info = false;
        }

        dynamic_string m_output_basename;
        vogl_blob_manager *m_pBlob_manager;

        uint32_t m_cur_frame;
        uint32_t m_blob_file_size_threshold;
        bool m_write_debug_info;
    };

    bool json_serialize(json_node &node, const json_serialize_params &params) const;

    // pDocument_filename is used to print context of warnings/errors
    bool json_deserialize(const json_node &node, const char *pDocument_filename, const vogl_blob_manager *pBlob_manager);

    inline const vogl_trace_gl_entrypoint_packet &get_entrypoint_packet() const
    {
        return m_packet;
    }
    inline vogl_trace_gl_entrypoint_packet &get_entrypoint_packet()
    {
        return m_packet;
    }

    inline gl_entrypoint_id_t get_entrypoint_id() const
    {
        return static_cast<gl_entrypoint_id_t>(m_packet.m_entrypoint_id);
    }
    inline const gl_entrypoint_desc_t &get_entrypoint_desc() const
    {
        return g_vogl_entrypoint_descs[m_packet.m_entrypoint_id];
    }

    inline uint64_t get_context_handle() const
    {
        return m_packet.m_context_handle;
    }
    inline uint64_t get_call_counter() const
    {
        return m_packet.m_call_counter;
    }
    inline uint64_t get_thread_id() const
    {
        return m_packet.m_thread_id;
    }

    inline bool is_valid() const
    {
        return m_is_valid;
    }
    inline uint32_t total_params() const
    {
        return m_total_params;
    }
    inline bool has_return_value() const
    {
        return m_has_return_value;
    }

    inline const key_value_map &get_key_value_map() const
    {
        return m_key_value_map;
    }
    inline key_value_map &get_key_value_map()
    {
        return m_key_value_map;
    }

    // packet construction
    inline void begin_construction(gl_entrypoint_id_t id, uint64_t context_handle, uint64_t call_counter, uint64_t thread_id, uint64_t begin_rdtsc)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(id < VOGL_NUM_ENTRYPOINTS);

        m_total_params = 0;
        m_has_return_value = false;

        VOGL_ASSUME(VOGL_INVALID_CTYPE == 0);
        utils::zero_object(m_param_ctype);

        const uint32_t total_params_to_serialize = g_vogl_entrypoint_descs[id].m_num_params + (g_vogl_entrypoint_descs[id].m_return_ctype != VOGL_VOID);
        for (uint32_t i = 0; i < total_params_to_serialize; ++i)
            m_client_memory_descs[i].clear();

        m_client_memory.resize(0);
        m_key_value_map.reset();

        m_packet.init();
        m_packet.init_rnd();
        m_packet.m_packet_begin_rdtsc = begin_rdtsc;
        m_packet.m_entrypoint_id = id;
        m_packet.m_context_handle = context_handle;
        m_packet.m_call_counter = call_counter;
        m_packet.m_thread_id = thread_id;
        m_packet.m_backtrace_hash_index = 0;

        m_is_valid = true;
    }

    inline void set_begin_rdtsc(uint64_t val)
    {
        m_packet.m_packet_begin_rdtsc = val;
    }

    inline void set_gl_begin_rdtsc(uint64_t val)
    {
        m_packet.m_gl_begin_rdtsc = val;
    }
    inline void set_gl_end_rdtsc(uint64_t val)
    {
        m_packet.m_gl_end_rdtsc = val;
    }

    inline void set_backtrace_hash_index(uint32_t hash_index)
    {
        m_packet.m_backtrace_hash_index = hash_index;
    }
    inline uint32_t get_backtrace_hash_index() const
    {
        return m_packet.m_backtrace_hash_index;
    }

    inline void end_construction(uint64_t end_rdtsc)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(m_is_valid);
        VOGL_ASSERT(m_total_params == get_entrypoint_desc().m_num_params);
        VOGL_ASSERT(m_has_return_value == (get_entrypoint_desc().m_return_ctype != VOGL_VOID));

        m_packet.m_packet_end_rdtsc = end_rdtsc;
    }

    // composition
    inline void set_return_param(vogl_ctype_t ctype, const void *pParam, uint32_t param_size)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(m_is_valid);
        VOGL_ASSERT(ctype != VOGL_INVALID_CTYPE);
        VOGL_ASSERT((uint32_t)trace_ctypes()[ctype].m_size == param_size);
        VOGL_ASSERT((param_size >= sizeof(uint8_t)) && (param_size <= sizeof(uint64_t)));
        VOGL_ASSERT(g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_return_ctype == ctype);

        uint32_t param_index = g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_num_params;
        memcpy(&m_param_data[param_index], pParam, param_size);
        m_param_ctype[param_index] = ctype;
        m_param_size[param_index] = static_cast<uint8_t>(param_size);

        m_has_return_value = true;
    }

    inline void set_param(uint8_t param_id, vogl_ctype_t ctype, const void *pParam, uint32_t param_size)
    {
        VOGL_FUNC_TRACER

        if (param_id == VOGL_RETURN_PARAM_INDEX)
        {
            set_return_param(ctype, pParam, param_size);
            return;
        }

        VOGL_ASSERT(m_is_valid);
        VOGL_ASSERT(ctype != VOGL_INVALID_CTYPE);
        VOGL_ASSERT(param_id < g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_num_params);
        VOGL_ASSERT(g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_num_params < cMaxParams);
        VOGL_ASSERT((uint32_t)trace_ctypes()[ctype].m_size == param_size);
        VOGL_ASSERT((param_size >= sizeof(uint8_t)) && (param_size <= sizeof(uint64_t)));
        VOGL_ASSERT(g_vogl_entrypoint_param_descs[m_packet.m_entrypoint_id][param_id].m_ctype == ctype);

        m_total_params += (m_param_ctype[param_id] == VOGL_INVALID_CTYPE);
        VOGL_ASSERT(m_total_params <= g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_num_params);

        memcpy(&m_param_data[param_id], pParam, param_size);
        m_param_ctype[param_id] = ctype;
        m_param_size[param_id] = static_cast<uint8_t>(param_size);
    }

    inline void set_array_client_memory(uint8_t param_id, vogl_ctype_t pointee_ctype, uint64_t array_size, const void *pData, uint64_t data_size)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(array_size);

        VOGL_ASSERT(m_is_valid);
        //VOGL_ASSERT(param_id <= cUINT8_MAX);
        VOGL_ASSERT(static_cast<int>(pointee_ctype) <= cUINT8_MAX);
        if (trace_ctypes()[pointee_ctype].m_size > 0)
        {
            VOGL_ASSERT((data_size % trace_ctypes()[pointee_ctype].m_size) == 0);
        }
        if (data_size >= static_cast<uint64_t>(cINT32_MAX))
        {
            VOGL_FAIL("vogl_entrypoint_serializer::add_param_client_memory: Need to support streaming more than 2GB of client memory per call!\n");
        }

        uint32_t param_index = param_id;
        if (param_id == VOGL_RETURN_PARAM_INDEX)
        {
            VOGL_ASSERT(trace_ctypes()[g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_return_ctype].m_is_pointer);
            param_index = g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_num_params;
        }
        else
        {
            VOGL_ASSERT(trace_ctypes()[g_vogl_entrypoint_param_descs[m_packet.m_entrypoint_id][param_id].m_ctype].m_is_pointer);
        }

        uint32_t data_size32 = static_cast<uint32_t>(data_size);

        m_client_memory_descs[param_index].m_pointee_ctype = pointee_ctype;

        if ((m_client_memory_descs[param_index].m_vec_ofs >= 0) && (data_size32 <= m_client_memory_descs[param_index].m_data_size))
        {
            memcpy(&m_client_memory[m_client_memory_descs[param_index].m_vec_ofs], pData, static_cast<size_t>(data_size));
        }
        else
        {
            m_client_memory_descs[param_index].m_vec_ofs = m_client_memory.size();
            m_client_memory_descs[param_index].m_data_size = data_size32;
            m_client_memory.append(static_cast<const uint8_t *>(pData), data_size32);
        }
    }

    inline void set_ref_client_memory(uint8_t param_id, vogl_ctype_t pointee_ctype, const void *pData, uint64_t data_size)
    {
        VOGL_FUNC_TRACER

        set_array_client_memory(param_id, pointee_ctype, 1U, pData, data_size);
    }

    inline bool set_key_value(const value &key, const value &value)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(m_is_valid);
        return m_key_value_map.insert(key, value).second;
    }

    // ownership of blob's buffer is taken
    inline bool set_key_value_blob_take_ownership(const value &key, uint8_vec &blob)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(m_is_valid);
        value_to_value_hash_map::insert_result res(m_key_value_map.insert(key, value()));
        (res.first)->second.set_blob_take_ownership(blob);
        return res.second;
    }

    inline bool set_key_value_blob(const value &key, const void *pData, uint32_t data_size)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(m_is_valid);
        value_to_value_hash_map::insert_result res(m_key_value_map.insert(key, value()));
        (res.first)->second.set_blob(static_cast<const uint8_t *>(pData), data_size);
        return res.second;
    }

    inline bool set_key_value_json_document(const value &key, const json_document &doc)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(m_is_valid);
        value_to_value_hash_map::insert_result res(m_key_value_map.insert(key, value()));
        (res.first)->second.set_json_document(doc);
        return res.second;
    }

    // param accessors
    inline const uint64_t &get_param_data(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return m_param_data[param_index];
    }
    inline uint32_t get_param_size(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return m_param_size[param_index];
    }
    inline vogl_ctype_t get_param_ctype(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return m_param_ctype[param_index];
    }
    inline const gl_entrypoint_param_desc_t &get_param_desc(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return g_vogl_entrypoint_param_descs[m_packet.m_entrypoint_id][param_index];
    }
    inline const vogl_ctype_desc_t &get_param_ctype_desc(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return trace_ctypes()[m_param_ctype[param_index]];
    }
    inline vogl_namespace_t get_param_namespace(uint32_t param_index) const
    {
        return get_param_desc(param_index).m_namespace;
    }

    // It's acceptable to call this on params which have sizeof's less than T, the upper bytes are 0's.
    template <typename T>
    inline T get_param_value(uint32_t param_index) const
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(sizeof(T) <= sizeof(uint64_t));
        VOGL_ASSUME(!Loki::type_is_ptr<T>::result);
        VOGL_ASSERT(sizeof(T) >= static_cast<uint32_t>(get_param_ctype_desc(param_index).m_size));
        VOGL_ASSERT(!get_param_ctype_desc(param_index).m_is_pointer);

        if (sizeof(T) != get_param_size(param_index))
        {
            if (!validate_value_conversion(sizeof(T), Loki::TypeTraits<T>::typeFlags, param_index))
            {
                vogl_warning_printf("Parameter value conversion of call counter %llu func %s parameter \"%s %s\" to dest type size %u will fail, size %u value=0x%08llx\n",
                                 (unsigned long long)m_packet.m_call_counter,
                                 get_entrypoint_desc().m_pName,
                                 get_param_ctype_desc(param_index).m_pName, get_param_desc(param_index).m_pName,
                                 (uint32_t)sizeof(T),
                                 get_param_size(param_index),
                                 (unsigned long long)get_param_data(param_index));
            }
        }

        return *reinterpret_cast<const T *>(&get_param_data(param_index));
    }

    inline vogl_trace_ptr_value get_param_ptr_value(uint32_t param_index) const
    {
        VOGL_FUNC_TRACER

        VOGL_ASSUME(sizeof(vogl_trace_ptr_value) <= sizeof(uint64_t));
        VOGL_ASSERT(get_param_ctype_desc(param_index).m_is_pointer);
        return *reinterpret_cast<const vogl_trace_ptr_value *>(&get_param_data(param_index));
    }

    // Returns cInvalidIndex if not found
    inline int find_param_index(const char *pName) const
    {
        VOGL_FUNC_TRACER

        const gl_entrypoint_param_desc_t *pParams = &g_vogl_entrypoint_param_descs[m_packet.m_entrypoint_id][0];
        VOGL_ASSERT(g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_num_params == m_total_params);

        for (uint32_t i = 0; i < m_total_params; i++)
            if (vogl_strcmp(pName, pParams->m_pName) == 0)
                return i;

        return cInvalidIndex;
    }

    bool pretty_print_param(dynamic_string &str, uint32_t param_index, bool type_info) const;
    bool pretty_print_return_value(dynamic_string &str, bool type_info) const;

    // return value
    inline const uint64_t &get_return_value_data() const
    {
        VOGL_ASSERT(m_has_return_value);
        return m_param_data[m_total_params];
    }
    inline uint32_t get_return_value_size() const
    {
        VOGL_ASSERT(m_has_return_value);
        return m_param_size[m_total_params];
    }
    inline vogl_ctype_t get_return_value_ctype() const
    {
        VOGL_ASSERT(m_has_return_value);
        return m_param_ctype[m_total_params];
    }
    inline const vogl_ctype_desc_t &get_return_value_ctype_desc() const
    {
        return trace_ctypes()[get_return_value_ctype()];
    }
    inline vogl_namespace_t get_return_value_namespace() const
    {
        return get_entrypoint_desc().m_return_namespace;
    }

    // It's acceptable to call this on params which have sizeof's less than T, the upper bytes are 0's.
    template <typename T>
    inline T get_return_value() const
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT(sizeof(T) <= sizeof(uint64_t));
        VOGL_ASSUME(!Loki::type_is_ptr<T>::result);
        VOGL_ASSERT(sizeof(T) >= static_cast<uint32_t>(get_return_value_ctype_desc().m_size));
        VOGL_ASSERT(!get_return_value_ctype_desc().m_is_pointer);

        if (sizeof(T) != get_return_value_size())
        {
            if (!validate_value_conversion(sizeof(T), Loki::TypeTraits<T>::typeFlags, -1))
            {
                vogl_warning_printf("Return value conversion of call counter %llu func %s return value type \"%s\" to dest type size %u will fail, size %u value=0x%08llx\n",
                                 (unsigned long long)m_packet.m_call_counter,
                                 get_entrypoint_desc().m_pName,
                                 get_return_value_ctype_desc().m_pName,
                                 (uint32_t)sizeof(T),
                                 get_return_value_size(),
                                 (unsigned long long)get_return_value_data());
            }
        }

        return *reinterpret_cast<const T *>(&get_return_value_data());
    }

    inline vogl_trace_ptr_value get_return_ptr_value() const
    {
        VOGL_FUNC_TRACER

        VOGL_ASSUME(sizeof(vogl_trace_ptr_value) <= sizeof(uint64_t));
        VOGL_ASSERT(get_return_value_ctype_desc().m_is_pointer);
        return *reinterpret_cast<const vogl_trace_ptr_value *>(&get_return_value_data());
    }

    // param client memory accessors
    inline bool has_param_client_memory(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return m_client_memory_descs[param_index].m_vec_ofs != -1;
    }
    inline const void *get_param_client_memory_ptr(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        int ofs = m_client_memory_descs[param_index].m_vec_ofs;
        return (ofs < 0) ? NULL : &m_client_memory[ofs];
    }
    inline void *get_param_client_memory_ptr(uint32_t param_index)
    {
        VOGL_ASSERT(param_index < m_total_params);
        int ofs = m_client_memory_descs[param_index].m_vec_ofs;
        return (ofs < 0) ? NULL : &m_client_memory[ofs];
    }
    inline uint32_t get_param_client_memory_data_size(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return m_client_memory_descs[param_index].m_data_size;
    }
    inline vogl_ctype_t get_param_client_memory_ctype(uint32_t param_index) const
    {
        VOGL_ASSERT(param_index < m_total_params);
        return static_cast<vogl_ctype_t>(m_client_memory_descs[param_index].m_pointee_ctype);
    }
    inline const vogl_ctype_desc_t &get_param_client_memory_ctype_desc(uint32_t param_index) const
    {
        return trace_ctypes()[get_param_client_memory_ctype(param_index)];
    }

    template <typename T>
    inline const T *get_param_client_memory(uint32_t param_index) const
    {
        VOGL_FUNC_TRACER

        VOGL_ASSUME(!Loki::type_is_ptr<T>::result);
        return static_cast<const T *>(get_param_client_memory_ptr(param_index));
    };

    template <typename T>
    inline T *get_param_client_memory(uint32_t param_index)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSUME(!Loki::type_is_ptr<T>::result);
        return static_cast<T *>(get_param_client_memory_ptr(param_index));
    };

    inline const vogl_client_memory_array get_param_client_memory_array(uint32_t param_index) const
    {
        VOGL_FUNC_TRACER

        const vogl_ctype_desc_t &ctype_desc = get_param_client_memory_ctype_desc(param_index);
        uint32_t element_size = ctype_desc.m_size;
        uint32_t data_size = get_param_client_memory_data_size(param_index);
        if (element_size <= 0)
            return vogl_client_memory_array(get_param_client_memory_ctype(param_index), get_param_client_memory_ptr(param_index), data_size, 1);
        else
        {
            VOGL_ASSERT((data_size % element_size) == 0);
            return vogl_client_memory_array(get_param_client_memory_ctype(param_index), get_param_client_memory_ptr(param_index), element_size, data_size / element_size);
        }
    }

    // return client memory accessors
    inline bool has_return_client_memory() const
    {
        VOGL_ASSERT(m_has_return_value);
        return m_client_memory_descs[m_total_params].m_vec_ofs != -1;
    }
    inline const void *get_return_client_memory_ptr() const
    {
        VOGL_ASSERT(m_has_return_value);
        int ofs = m_client_memory_descs[m_total_params].m_vec_ofs;
        return (ofs < 0) ? NULL : &m_client_memory[ofs];
    }
    inline int get_return_client_memory_data_size() const
    {
        VOGL_ASSERT(m_has_return_value);
        return m_client_memory_descs[m_total_params].m_data_size;
    }
    inline vogl_ctype_t get_return_client_memory_ctype() const
    {
        VOGL_ASSERT(m_has_return_value);
        return static_cast<vogl_ctype_t>(m_client_memory_descs[m_total_params].m_pointee_ctype);
    }
    inline const vogl_ctype_desc_t &get_return_client_memory_ctype_desc() const
    {
        return trace_ctypes()[get_return_client_memory_ctype()];
    }

    inline const vogl_client_memory_array get_return_client_memory_array() const
    {
        VOGL_FUNC_TRACER

        const vogl_ctype_desc_t &ctype_desc = get_return_client_memory_ctype_desc();
        uint32_t element_size = ctype_desc.m_size;
        uint32_t data_size = get_return_client_memory_data_size();
        if (element_size <= 0)
            return vogl_client_memory_array(get_return_client_memory_ctype(), get_return_client_memory_ptr(), data_size, 1);
        else
        {
            VOGL_ASSERT((data_size % element_size) == 0);
            return vogl_client_memory_array(get_return_client_memory_ctype(), get_return_client_memory_ptr(), element_size, data_size / element_size);
        }
    }

private:
    const vogl_ctypes *m_pCTypes;

    inline const vogl_ctypes &trace_ctypes() const
    {
        return *m_pCTypes;
    }

    // the "_size" fields in m_packet are not valid until binary serialization
    vogl_trace_gl_entrypoint_packet m_packet;

    uint32_t m_total_params;
    bool m_has_return_value;
    bool m_is_valid;

    enum
    {
        cMaxParams = 32
    };
    uint64_t m_param_data[cMaxParams];
    uint8_t m_param_size[cMaxParams];
    vogl_ctype_t m_param_ctype[cMaxParams];

    uint8_vec m_client_memory;

    key_value_map m_key_value_map;

#pragma pack(push)
#pragma pack(1)
    struct client_memory_desc_t
    {
        int32_t m_vec_ofs;
        uint32_t m_data_size;
        uint8_t m_pointee_ctype; // vogl_ctype_t

        inline void clear()
        {
            m_vec_ofs = -1;
            m_data_size = 0;
            m_pointee_ctype = VOGL_VOID;
        }
    };
#pragma pack(pop)

    client_memory_desc_t m_client_memory_descs[cMaxParams];

    mutable uint8_vec m_packet_buf;

    bool validate_value_conversion(uint32_t dest_type_size, uint32_t dest_type_loki_type_flags, int param_index) const;

    static bool should_always_write_as_blob_file(const char *pFunc_name);

    void ctype_to_json_value(json_value &val, gl_entrypoint_id_t entrypoint_id, int param_index, uint64_t data, vogl_ctype_t ctype) const;

    bool json_serialize_param(
        const char *pFunc_name,
        json_node &gl_params_node, int param_index, const char *pName, const uint64_t param_data, const uint32_t param_size, vogl_ctype_t param_ctype,
        const void *pClient_mem, uint32_t client_mem_size, vogl_ctype_t client_mem_ctype,
        const json_serialize_params &params) const;

    bool convert_json_value_to_ctype_data(uint64_t &data, const json_value &val, vogl_ctype_t ctype, const char *pName, int param_iter, gl_entrypoint_param_class_t param_class, bool permit_client_strings, const json_node &node, const char *pDocument_filename);

    bool json_deserialize_param(
        uint64_t cur_call_counter, const char *pFunc_name,
        const json_node &params_node, uint32_t param_iter, const char *pParam_name, gl_entrypoint_param_class_t param_class, const vogl_ctype_desc_t &param_ctype_desc,
        const char *pDocument_filename, const vogl_blob_manager *pBlob_manager);

    static bool get_uint64_from_json_node(const json_node &node, const char *pKey, uint64_t &val, uint64_t def = 0);

    static bool force_value_to_type(value &val, value_data_type type);

    bool fixup_manually_edited_params(gl_entrypoint_id_t gl_entrypoint_id, const char *pGL_func_name, uint64_t cur_call_counter);
};

bool vogl_does_packet_refer_to_program(const vogl_trace_packet &gl_packet, GLuint &program);
bool vogl_write_glInternalTraceCommandRAD(data_stream &stream, const vogl_ctypes *pCTypes, GLuint cmd, GLuint size, const GLubyte *data);

#endif // VOGL_TRACE_PACKET_H
