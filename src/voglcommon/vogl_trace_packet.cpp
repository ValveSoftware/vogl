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

// File: vogl_trace_packet.cpp
#include "vogl_common.h"
#include "vogl_trace_packet.h"
#include "vogl_console.h"
#include "vogl_file_utils.h"
#include "vogl_bigint128.h"
#include "vogl_dynamic_stream.h"

using namespace vogl;

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::compare
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::compare(const vogl_trace_packet &other, bool deep) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!other.m_is_valid))
        return false;

    if (other.get_ctypes() != get_ctypes())
        return false;

    if (m_total_params != other.m_total_params)
        return false;
    if (m_has_return_value != other.m_has_return_value)
        return false;

#define CMP(x)                          \
    if (m_packet.x != other.m_packet.x) \
        return false;

    CMP(m_entrypoint_id);
    CMP(m_context_handle);
    CMP(m_thread_id);
    CMP(m_call_counter);

    if (deep)
    {
        CMP(m_rnd);
        CMP(m_inv_rnd);
        CMP(m_packet_begin_rdtsc);
        CMP(m_gl_begin_rdtsc);
        CMP(m_gl_end_rdtsc);
        CMP(m_backtrace_hash_index);
        CMP(m_packet_end_rdtsc);

        CMP(m_param_size);
        CMP(m_client_memory_size);
        CMP(m_name_value_map_size);
    }
#undef CMP

    const uint32_t total_params_to_check = m_total_params + m_has_return_value;
    for (uint32_t i = 0; i < total_params_to_check; i++)
    {
        if (m_param_ctype[i] != other.m_param_ctype[i])
            return false;
        if (m_param_size[i] != other.m_param_size[i])
            return false;

#if 0
		if ((!deep) && ((*m_pCTypes)[m_param_ctype[i]].m_is_pointer))
		{
			bool lhs_null = (m_param_data[i] != 0);
			bool rhs_null = (other.m_param_data[i] != 0);
			if (lhs_null != rhs_null)
				return false;
		}
		else
#endif
        {
            if (m_param_data[i] != other.m_param_data[i])
                return false;
        }

        const client_memory_desc_t &desc_lhs = m_client_memory_descs[i];
        const client_memory_desc_t &desc_rhs = other.m_client_memory_descs[i];

        if (desc_lhs.m_vec_ofs >= 0)
        {
            if (desc_rhs.m_vec_ofs < 0)
                return false;
            if ((deep) && (desc_lhs.m_vec_ofs != desc_rhs.m_vec_ofs))
                return false;
            if (desc_lhs.m_data_size != desc_rhs.m_data_size)
                return false;
            if (desc_lhs.m_pointee_ctype != desc_rhs.m_pointee_ctype)
                return false;
            if (memcmp(m_client_memory.get_ptr() + desc_lhs.m_vec_ofs, other.m_client_memory.get_ptr() + desc_rhs.m_vec_ofs, desc_lhs.m_data_size) != 0)
                return false;
        }
        else if (desc_rhs.m_vec_ofs >= 0)
        {
            return false;
        }
    }

    if (m_key_value_map != other.m_key_value_map)
        return false;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::check
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::check() const
{
    VOGL_FUNC_TRACER

    VOGL_ASSUME(VOGL_NUM_CTYPES < cUINT8_MAX);

    if (!m_is_valid)
        return false;

    if (m_packet.m_prefix != vogl_trace_stream_packet_base::cTracePacketPrefix)
        return false;
    if (m_packet.m_type != cTSPTGLEntrypoint)
        return false;

    const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[m_packet.m_entrypoint_id];

    if (m_total_params != entrypoint_desc.m_num_params)
        return false;

    if (entrypoint_desc.m_return_ctype == VOGL_VOID)
    {
        if (m_has_return_value)
            return false;
        if (m_param_ctype[entrypoint_desc.m_num_params] != VOGL_INVALID_CTYPE)
            return false;
    }
    else
    {
        if (!m_has_return_value)
            return false;
        if (m_param_ctype[entrypoint_desc.m_num_params] != entrypoint_desc.m_return_ctype)
            return false;
        if (m_param_size[entrypoint_desc.m_num_params] != (*m_pCTypes)[entrypoint_desc.m_return_ctype].m_size)
            return false;
    }

    for (uint32_t param_iter = 0; param_iter < entrypoint_desc.m_num_params; param_iter++)
    {
        if (m_param_ctype[param_iter] == VOGL_INVALID_CTYPE)
            return false;

        const gl_entrypoint_param_desc_t &param_desc = g_vogl_entrypoint_param_descs[m_packet.m_entrypoint_id][param_iter];

        if (m_param_ctype[param_iter] != param_desc.m_ctype)
            return false;
        if (m_param_size[param_iter] != (*m_pCTypes)[param_desc.m_ctype].m_size)
            return false;
    }

    uint32_t total_params = entrypoint_desc.m_num_params + m_has_return_value;
    for (uint32_t param_iter = 0; param_iter < total_params; param_iter++)
    {
        const client_memory_desc_t &mem_desc = m_client_memory_descs[param_iter];
        if (mem_desc.m_vec_ofs >= 0)
        {
            if ((!mem_desc.m_data_size) || (mem_desc.m_data_size >= static_cast<uint32_t>(cINT32_MAX)))
                return false;

            if ((mem_desc.m_vec_ofs + mem_desc.m_data_size) > m_client_memory.size())
                return false;

            uint32_t pointee_ctype_size = 0;
            bool pointee_is_opaque = false;
            bool is_opaque_ptr = false;
            if (param_iter < entrypoint_desc.m_num_params)
            {
                const gl_entrypoint_param_desc_t &param_desc = g_vogl_entrypoint_param_descs[m_packet.m_entrypoint_id][param_iter];

                is_opaque_ptr = (*m_pCTypes)[param_desc.m_ctype].m_is_opaque_pointer;

                if (!(*m_pCTypes)[param_desc.m_ctype].m_is_pointer)
                    return false;

                if ((*m_pCTypes)[param_desc.m_ctype].m_pointee_ctype != VOGL_VOID)
                {
                    pointee_ctype_size = (*m_pCTypes)[(*m_pCTypes)[param_desc.m_ctype].m_pointee_ctype].m_size;
                    pointee_is_opaque = (*m_pCTypes)[(*m_pCTypes)[param_desc.m_ctype].m_pointee_ctype].m_is_opaque_type;
                }
            }
            else
            {
                is_opaque_ptr = (*m_pCTypes)[entrypoint_desc.m_return_ctype].m_is_opaque_pointer;

                if (!(*m_pCTypes)[entrypoint_desc.m_return_ctype].m_is_pointer)
                    return false;

                if ((*m_pCTypes)[entrypoint_desc.m_return_ctype].m_pointee_ctype != VOGL_VOID)
                {
                    pointee_ctype_size = (*m_pCTypes)[(*m_pCTypes)[entrypoint_desc.m_return_ctype].m_pointee_ctype].m_size;
                    pointee_is_opaque = (*m_pCTypes)[(*m_pCTypes)[entrypoint_desc.m_return_ctype].m_pointee_ctype].m_is_opaque_type;
                }
            }

            if ((pointee_ctype_size > 1) && (!is_opaque_ptr) && (!pointee_is_opaque))
            {
                if (mem_desc.m_data_size % pointee_ctype_size)
                    return false;
            }
        }
        else
        {
            if (mem_desc.m_vec_ofs != -1)
                return false;
            if (mem_desc.m_data_size)
                return false;
            if (mem_desc.m_pointee_ctype != VOGL_VOID)
                return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::deserialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::deserialize(const uint8_t *pPacket_data, uint32_t packet_data_buf_size, bool check_crc)
{
    VOGL_FUNC_TRACER

    reset();

    const vogl_trace_gl_entrypoint_packet *pTrace_gl_entrypoint_packet = reinterpret_cast<const vogl_trace_gl_entrypoint_packet *>(pPacket_data);

    if (check_crc)
    {
        if (!pTrace_gl_entrypoint_packet->full_validation(packet_data_buf_size))
        {
            vogl_error_printf("Trace packet failed full validation!\n");
            return false;
        }
    }
    else
    {
        if (!pTrace_gl_entrypoint_packet->basic_validation())
        {
            vogl_error_printf("Trace packet failed basic validation!\n");
            return false;
        }
    }

    m_packet = *reinterpret_cast<const vogl_trace_gl_entrypoint_packet *>(pTrace_gl_entrypoint_packet);

    VOGL_ASSERT(m_packet.m_type == cTSPTGLEntrypoint);

    if (m_packet.m_size < sizeof(vogl_trace_gl_entrypoint_packet))
        return false;

    if (m_packet.m_entrypoint_id >= VOGL_NUM_ENTRYPOINTS)
        return false;

    uint32_t entrypoint_index = m_packet.m_entrypoint_id;
    const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_index];
    const gl_entrypoint_param_desc_t *pParam_desc = &g_vogl_entrypoint_param_descs[entrypoint_index][0];

    m_total_params = entrypoint_desc.m_num_params;
    m_has_return_value = (entrypoint_desc.m_return_ctype != VOGL_VOID);

    const uint32_t total_params_to_deserialize = m_total_params + m_has_return_value;

    const uint8_t *pExtra_packet_data = pPacket_data + sizeof(vogl_trace_gl_entrypoint_packet);
    uint32_t num_bytes_remaining = m_packet.m_size - sizeof(vogl_trace_gl_entrypoint_packet);

    if (m_packet.m_param_size)
    {
        if (m_packet.m_param_size > num_bytes_remaining)
            return false;

        for (uint32_t param_index = 0; param_index < total_params_to_deserialize; ++param_index, ++pParam_desc)
        {
            vogl_ctype_t param_ctype = (param_index >= m_total_params) ? entrypoint_desc.m_return_ctype : pParam_desc->m_ctype;
            uint32_t param_size = (*m_pCTypes)[param_ctype].m_size;

            if (num_bytes_remaining < param_size)
                return false;

            m_param_ctype[param_index] = param_ctype;
            VOGL_ASSERT(param_size <= cUINT8_MAX);
            m_param_size[param_index] = param_size;

            m_param_data[param_index] = 0;
            memcpy(&m_param_data[param_index], pExtra_packet_data, param_size);

            pExtra_packet_data += param_size;
            num_bytes_remaining -= param_size;
        }
    }

#ifdef _DEBUG
    // verify that the parameter sizes were read correctly (by comparing the sum to the packet's specified param size)
    uint32_t totalParamSize = 0;
    for (uint32_t param_index = 0; param_index < total_params_to_deserialize; ++param_index, ++pParam_desc)
    {
        totalParamSize += m_param_size[param_index];
    }
    VOGL_ASSERT(totalParamSize == m_packet.m_param_size);
#endif

    if (m_packet.m_client_memory_size)
    {
        if (m_packet.m_client_memory_size > num_bytes_remaining)
            return false;

        uint32_t client_memory_descs_size = (total_params_to_deserialize * sizeof(client_memory_desc_t));
        if (num_bytes_remaining < client_memory_descs_size)
            return false;

        memcpy(m_client_memory_descs, pExtra_packet_data, client_memory_descs_size);
        pExtra_packet_data += client_memory_descs_size;
        num_bytes_remaining -= client_memory_descs_size;

        if (client_memory_descs_size > m_packet.m_client_memory_size)
            return false;

        uint32_t client_memory_vec_size = m_packet.m_client_memory_size - client_memory_descs_size;

        m_client_memory.append(pExtra_packet_data, client_memory_vec_size);
        pExtra_packet_data += client_memory_vec_size;
        num_bytes_remaining -= client_memory_vec_size;
    }

    if (m_packet.m_name_value_map_size)
    {
        if (m_packet.m_name_value_map_size > num_bytes_remaining)
            return false;

        if (!m_key_value_map.deserialize_from_buffer(pExtra_packet_data, num_bytes_remaining, true, false))
            return false;

        pExtra_packet_data += m_packet.m_name_value_map_size;
        num_bytes_remaining -= m_packet.m_name_value_map_size;
    }

    if (num_bytes_remaining)
        return false;

    m_is_valid = true;

    VOGL_ASSERT(check());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::deserialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::deserialize(const uint8_vec &packet_buf, bool check_crc)
{
    VOGL_FUNC_TRACER

    return deserialize(packet_buf.get_ptr(), packet_buf.size(), check_crc);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::serialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::serialize(data_stream &stream) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    VOGL_ASSERT(m_packet.m_gl_begin_rdtsc <= m_packet.m_gl_end_rdtsc);
    VOGL_ASSERT(m_packet.m_packet_begin_rdtsc <= m_packet.m_packet_end_rdtsc);

    vogl_trace_gl_entrypoint_packet packet(m_packet);

    uint8_t param_data[512];

    uint32_t total_params_to_serialize = m_total_params + m_has_return_value;
    uint8_t *pDst_param_data = param_data;
    for (uint32_t i = 0; i < total_params_to_serialize; i++)
    {
        uint32_t size = m_param_size[i];
        VOGL_ASSERT(size);
        memcpy(pDst_param_data, &m_param_data[i], size);
        pDst_param_data += size;
        VOGL_ASSERT((pDst_param_data - param_data) <= static_cast<long>(sizeof(param_data)));
    }

    VOGL_VERIFY((pDst_param_data - param_data) <= cUINT8_MAX);
    packet.m_param_size = static_cast<uint8_t>(pDst_param_data - param_data);

    uint32_t client_memory_descs_size = (total_params_to_serialize * sizeof(client_memory_desc_t));
    packet.m_client_memory_size = client_memory_descs_size + m_client_memory.size();

    uint64_t kvm_serialize_size = m_key_value_map.get_num_key_values() ? m_key_value_map.get_serialize_size(false) : 0;
    if (kvm_serialize_size > cUINT32_MAX)
        return false;
    packet.m_name_value_map_size = static_cast<uint32_t>(kvm_serialize_size);

    uint64_t total_packet_size = sizeof(vogl_trace_gl_entrypoint_packet) + packet.m_param_size + packet.m_client_memory_size + packet.m_name_value_map_size;
    if (total_packet_size > static_cast<uint64_t>(cINT32_MAX))
        return false;

    packet.m_size = static_cast<uint32_t>(total_packet_size);

    if (!m_packet_buf.try_resize(static_cast<uint32_t>(total_packet_size)))
        return false;

    uint8_t *pDst_buf = m_packet_buf.get_ptr();

#define APPEND_TO_DST_BUF(pSrc, n)                                         \
    do                                                                     \
    {                                                                      \
        VOGL_ASSERT((int64_t)n <= (int64_t)(m_packet_buf.end() - pDst_buf)); \
        memcpy(pDst_buf, pSrc, n);                                         \
        pDst_buf += n;                                                     \
    } while (0)

    APPEND_TO_DST_BUF(&packet, sizeof(packet));

    if (packet.m_param_size)
    {
        APPEND_TO_DST_BUF(param_data, packet.m_param_size);
    }

    if (packet.m_client_memory_size)
    {
        APPEND_TO_DST_BUF(m_client_memory_descs, client_memory_descs_size);
        APPEND_TO_DST_BUF(m_client_memory.get_ptr(), m_client_memory.size());
    }

    if (m_key_value_map.get_num_key_values())
    {
        if (pDst_buf >= m_packet_buf.end())
        {
            VOGL_VERIFY(0);
            return false;
        }

        uint32_t buf_remaining = static_cast<uint32_t>(m_packet_buf.end() - pDst_buf);
        int result = m_key_value_map.serialize_to_buffer(pDst_buf, buf_remaining, true, false);
        if (result != static_cast<int>(packet.m_name_value_map_size))
            return false;

        pDst_buf += result;
    }

    if (pDst_buf != m_packet_buf.end())
        return false;

#undef APPEND_TO_DST_BUF

    vogl_trace_gl_entrypoint_packet *pBuf_packet = reinterpret_cast<vogl_trace_gl_entrypoint_packet *>(m_packet_buf.get_ptr());
    pBuf_packet->finalize();

    VOGL_ASSERT(pBuf_packet->full_validation(m_packet_buf.size()));

    uint32_t n = stream.write(pBuf_packet, m_packet_buf.size());
    if (n != m_packet_buf.size())
        return false;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::serialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::serialize(uint8_vec &buf) const
{
    VOGL_FUNC_TRACER

    dynamic_stream dyn_stream;
    if (!dyn_stream.open(128))
        return false;
    if (!serialize(dyn_stream))
        return false;
    buf.swap(dyn_stream.get_buf());
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::json_serialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::json_serialize(json_node &node, const json_serialize_params &params) const
{
    VOGL_FUNC_TRACER

    const char *pFunc_name = g_vogl_entrypoint_descs[m_packet.m_entrypoint_id].m_pName;

    node.add_key_value("func", pFunc_name);

    node.add_key_value("thread_id", dynamic_string(cVarArg, "0x%" PRIX64, m_packet.m_thread_id));
    node.add_key_value("context", dynamic_string(cVarArg, "0x%" PRIX64, m_packet.m_context_handle));
    node.add_key_value("call_counter", static_cast<int64_t>(m_packet.m_call_counter));
    node.add_key_value("crc32", m_packet.m_crc);
    node.add_key_value("begin_rdtsc", m_packet.m_packet_begin_rdtsc);
    node.add_key_value("end_rdtsc", m_packet.m_packet_end_rdtsc);
    node.add_key_value("gl_begin_rdtsc", m_packet.m_gl_begin_rdtsc);
    node.add_key_value("gl_end_rdtsc", m_packet.m_gl_end_rdtsc);
    node.add_key_value("backtrace_hash_index", m_packet.m_backtrace_hash_index);
    node.add_key_value("rnd_check", m_packet.m_rnd);
    node.add_key_value("inv_rnd_check", m_packet.m_inv_rnd);

    json_node &gl_params_node = node.add_object("params");
    for (uint32_t param_index = 0; param_index < total_params(); param_index++)
    {
        if (!json_serialize_param(pFunc_name, gl_params_node, param_index, get_param_desc(param_index).m_pName,
                                  get_param_data(param_index), get_param_size(param_index), get_param_ctype(param_index),
                                  get_param_client_memory_ptr(param_index),
                                  get_param_client_memory_data_size(param_index),
                                  get_param_client_memory_ctype(param_index),
                                  params))
        {
            return false;
        }
    }

    if (has_return_value())
    {
        if (!json_serialize_param(pFunc_name, node, -1, "return",
                                  get_return_value_data(), get_return_value_size(), get_return_value_ctype(),
                                  get_return_client_memory_ptr(),
                                  get_return_client_memory_data_size(),
                                  get_return_client_memory_ctype(),
                                  params))
        {
            return false;
        }
    }

    if (m_packet.m_name_value_map_size)
    {
        json_node &gl_name_value_array = node.add_array("name_value_map");

        dynamic_string key_str, value_str;

        for (key_value_map::const_iterator it = m_key_value_map.begin(); it != m_key_value_map.end(); ++it)
        {
            const value &key = it->first;
            const value &val = it->second;

            json_node &entry_node = gl_name_value_array.add_object();

            entry_node.add_key_value("key_type", g_value_data_type_strings[key.get_data_type()]);
            entry_node.add_key_value("val_type", g_value_data_type_strings[val.get_data_type()]);

            key.get_string(key_str, false);

            if (key.get_data_type() == cDTStringHash)
            {
                string_hash hash(key.get_string_hash());

                const char *pStr = find_well_known_string_hash(hash);
                if (pStr)
                    key_str.set(pStr);
            }

            entry_node.add_key_value("key", key_str);

            bool handle_as_blob_file = false;

            if (val.is_blob())
            {
                if (val.get_blob()->size() >= params.m_blob_file_size_threshold)
                    handle_as_blob_file = true;
                else if (should_always_write_as_blob_file(pFunc_name))
                    handle_as_blob_file = true;
            }

            if (handle_as_blob_file)
            {
                dynamic_string id;

                uint64_t blob_crc64 = calc_crc64(CRC64_INIT, reinterpret_cast<const uint8_t *>(val.get_blob()->get_ptr()), val.get_blob()->size());

                if (params.m_pBlob_manager)
                {
                    //dynamic_string prefix(cVarArg, "%s_%s", params.m_output_basename.get_ptr(), pFunc_name);
                    dynamic_string prefix(cVarArg, "%s", pFunc_name);

                    id = params.m_pBlob_manager->add_buf_compute_unique_id(val.get_blob()->get_ptr(), val.get_blob()->size(), prefix, "blob", &blob_crc64);
                    if (id.is_empty())
                    {
                        vogl_error_printf("Failed adding blob %s to blob manager\n", prefix.get_ptr());
                        return false;
                    }

                    vogl_message_printf("Wrote blob id %s\n", id.get_ptr());
                }
                else
                {
                    id = "<blob_writing_disabled>";
                }

                json_node &blob_node = entry_node.add_object("data");
                blob_node.add_key_value("blob_id", id);
                blob_node.add_key_value("crc64", dynamic_string(cVarArg, "0x%016" PRIX64, blob_crc64));
                blob_node.add_key_value("size", val.get_blob()->size());
            }
            else
            {
                switch (val.get_data_type())
                {
                    case cDTInt8:
                    case cDTInt16:
                    case cDTInt:
                    case cDTInt64:
                    {
                        entry_node.add_key_value("data", val.get_int64());
                        break;
                    }
                    case cDTUInt8:
                    case cDTUInt16:
                    case cDTUInt:
                    {
                        entry_node.add_key_value("data", val.get_uint());
                        break;
                    }
                    case cDTStringHash:
                    {
                        string_hash hash(val.get_string_hash());

                        const char *pStr = find_well_known_string_hash(hash);
                        if (!pStr)
                            entry_node.add_key_value("data", hash.get_hash());
                        else
                            entry_node.add_key_value("data", pStr);
                        break;
                    }
                    case cDTFloat:
                    {
                        float flt_val = val.get_float();
                        json_value &new_val = entry_node.add_key_value("data", flt_val);

                        dynamic_string new_val_as_str;
                        new_val.serialize(new_val_as_str, false);

                        json_value test_val;
                        if ((!test_val.deserialize(new_val_as_str)) || (test_val.as_float() != flt_val))
                        {
                            new_val.set_value(dynamic_string(cVarArg, "0x%08X", *reinterpret_cast<const uint32_t *>(&flt_val)).get_ptr());
                        }

                        break;
                    }
                    case cDTDouble:
                    {
                        double dbl_val = val.get_double();
                        json_value &new_val = entry_node.add_key_value("data", dbl_val);

                        dynamic_string new_val_as_str;
                        new_val.serialize(new_val_as_str, false);

                        json_value test_val;
                        if ((!test_val.deserialize(new_val_as_str)) || (test_val.as_double() != dbl_val))
                        {
                            new_val.set_value(dynamic_string(cVarArg, "0x%" PRIX64, static_cast<uint64_t>(*reinterpret_cast<const uint64_t *>(&dbl_val))).get_ptr());
                        }

                        break;
                    }
                    case cDTUInt64:
                    {
                        entry_node.add_key_value("data", dynamic_string(cVarArg, "0x%" PRIX64, val.get_uint64()).get_ptr());
                        break;
                    }
                    case cDTJSONDoc:
                    {
                        entry_node.add_key_value("data", *val.get_json_document());
                        break;
                    }
                    default:
                    {
                        val.get_string(value_str, false);
                        entry_node.add_key_value("data", value_str);

                        break;
                    }
                }
            }
        }
    }

    if (params.m_write_debug_info)
    {
        json_node &gl_dbg_node = node.add_object("dbg");
        gl_dbg_node.add_key_value("entrypoint_id", m_packet.m_entrypoint_id);
        gl_dbg_node.add_key_value("total_params", total_params());
        gl_dbg_node.add_key_value("has_return_value", has_return_value());
        gl_dbg_node.add_key_value("is_whitelisted", get_entrypoint_desc().m_is_whitelisted);
        gl_dbg_node.add_key_value("packet_size", m_packet.m_size);
        gl_dbg_node.add_key_value("param_size", m_packet.m_param_size);
        gl_dbg_node.add_key_value("client_mem_size", m_packet.m_client_memory_size);
        gl_dbg_node.add_key_value("name_value_map_size", m_packet.m_name_value_map_size);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// print_json_context
//----------------------------------------------------------------------------------------------------------------------
static void print_json_context(const char *pDocument_filename, const json_node &node, eConsoleMessageType msg_category = cMsgError)
{
    VOGL_FUNC_TRACER

    console::printf(VOGL_FUNCTION_INFO_CSTR, msg_category, "Context: JSON filename %s, on or near line %u, node path: %s\n", pDocument_filename, node.get_line(), node.get_path_to_node().get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// print_json_context
//----------------------------------------------------------------------------------------------------------------------
static void print_json_context(const char *pDocument_filename, const json_node &node, uint32_t node_index, eConsoleMessageType msg_category = cMsgError)
{
    VOGL_FUNC_TRACER

    console::printf(VOGL_FUNCTION_INFO_CSTR, msg_category, "Context: JSON filename %s, on or near line %u, node item path: %s\n", pDocument_filename, node.get_line(), node.get_path_to_item(node_index).get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// print_json_context
//----------------------------------------------------------------------------------------------------------------------
static void print_json_context(const char *pDocument_filename, const json_value &val, const json_node &node, eConsoleMessageType msg_category = cMsgError)
{
    VOGL_FUNC_TRACER

    console::printf(VOGL_FUNCTION_INFO_CSTR, msg_category, "Context: JSON filename %s, on or near the node at line %u, on or near the value at line %u, node path: %s\n", pDocument_filename, node.get_line(), val.get_line(), node.get_path_to_node().get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// print_json_context
//----------------------------------------------------------------------------------------------------------------------
static void print_json_context(const char *pDocument_filename, const json_value &val, const json_node &node, uint32_t node_index, eConsoleMessageType msg_category = cMsgError)
{
    VOGL_FUNC_TRACER

    console::printf(VOGL_FUNCTION_INFO_CSTR, msg_category, "Context: JSON filename %s, on or near the node at line %u, on or near the value at line %u, item path: %s\n", pDocument_filename, node.get_line(), val.get_line(), node.get_path_to_item(node_index).get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::json_deserialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::json_deserialize(const json_node &node, const char *pDocument_filename, const vogl_blob_manager *pBlob_manager)
{
    VOGL_FUNC_TRACER

    reset();

    const dynamic_string gl_func_name(node.value_as_string("func"));
    if (gl_func_name.is_empty())
    {
        vogl_error_printf("Missing \"func\" node\n");
        print_json_context(pDocument_filename, node);
        return false;
    }

    entrypoint_name_hash_map_t::const_iterator it = get_vogl_entrypoint_hashmap().find(gl_func_name);
    if (it == get_vogl_entrypoint_hashmap().end())
    {
        vogl_error_printf("Unknown GL function name: \"%s\"\n", gl_func_name.get_ptr());
        print_json_context(pDocument_filename, node);
        return false;
    }

    gl_entrypoint_id_t gl_entrypoint_id = it->second;

    const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[gl_entrypoint_id];
    const gl_entrypoint_param_desc_t *pEntrypoint_params = &g_vogl_entrypoint_param_descs[gl_entrypoint_id][0];

    uint64_t thread_id = 0;
    get_uint64_from_json_node(node, "thread_id", thread_id);

    uint64_t context = 0;
    if (!get_uint64_from_json_node(node, "context", context))
        context = 1;

    uint64_t call_counter = 0;
    get_uint64_from_json_node(node, "call_counter", call_counter);

    const json_node *pParams_node = node.find_child("params");
    json_node dummy_node;
    if (!pParams_node)
    {
        if (entrypoint_desc.m_num_params)
        {
            vogl_error_printf("Failed finding params key!\n");
            print_json_context(pDocument_filename, node);
            return false;
        }
        else
        {
            vogl_warning_printf("Failed finding params key! Substituting a dummy empty node and trying to deserialize anyway.\n");
        }
        dummy_node.init_object();
        pParams_node = &dummy_node;
    }

    if (!pParams_node->is_object())
    {
        vogl_error_printf("Failed finding params key!\n");
        print_json_context(pDocument_filename, *pParams_node);
        return false;
    }

    uint32_t num_params = pParams_node->size();
    if (num_params != entrypoint_desc.m_num_params)
    {
        vogl_error_printf("Unexpected number of param keys (expected %u, found %u)\n", entrypoint_desc.m_num_params, num_params);
        print_json_context(pDocument_filename, *pParams_node);
        return false;
    }

    const bool has_return_key = node.has_key("return");
    if (entrypoint_desc.m_return_ctype == VOGL_VOID)
    {
        if (has_return_key)
        {
            vogl_error_printf("Unexpected return key\n");
            print_json_context(pDocument_filename, node);
            return false;
        }
    }
    else if (!has_return_key)
    {
        vogl_error_printf("Missing return key\n");
        print_json_context(pDocument_filename, node);
        return false;
    }

    m_packet.m_entrypoint_id = gl_entrypoint_id;
    m_packet.m_call_counter = call_counter;
    m_packet.m_context_handle = context;
    m_packet.m_thread_id = thread_id;

    // All of this is optional stuff
    m_packet.m_packet_begin_rdtsc = node.value_as_uint64("begin_rdtsc");
    m_packet.m_packet_end_rdtsc = node.value_as_uint64("end_rdtsc");
    m_packet.m_gl_begin_rdtsc = node.value_as_uint64("gl_begin_rdtsc");
    m_packet.m_gl_end_rdtsc = node.value_as_uint64("gl_end_rdtsc");
    m_packet.m_backtrace_hash_index = node.value_as_uint32("backtrace_hash_index");

    m_packet.m_rnd = node.value_as_uint32("rnd_check", 1);

    // The rnd value can't be 0,
    if (m_packet.m_rnd < vogl_trace_stream_packet_base::cMinimumPossibleRnd)
    {
        vogl_warning_printf("rnd_check value cannot be 0\n");
        print_json_context(pDocument_filename, node, cMsgWarning);
        m_packet.m_rnd = vogl_trace_stream_packet_base::cMinimumPossibleRnd;
    }
    m_packet.m_inv_rnd = ~m_packet.m_inv_rnd;

    m_total_params = num_params;
    m_has_return_value = has_return_key;

    for (uint32_t param_iter = 0; param_iter < num_params; param_iter++)
    {
        const gl_entrypoint_param_desc_t &param_desc = pEntrypoint_params[param_iter];

        if (!json_deserialize_param(
                 call_counter, gl_func_name.get_ptr(),
                 *pParams_node,
                 param_iter, param_desc.m_pName, param_desc.m_class, (*m_pCTypes)[param_desc.m_ctype], pDocument_filename, pBlob_manager))
        {
            return false;
        }
    }

    if (m_has_return_value)
    {
        if (!json_deserialize_param(
                 call_counter, gl_func_name.get_ptr(),
                 node,
                 num_params, "return", VOGL_VALUE_PARAM, (*m_pCTypes)[entrypoint_desc.m_return_ctype], pDocument_filename, pBlob_manager))
        {
            return false;
        }
    }

    const json_node *pName_value_array = node.find_child("name_value_map");
    if (pName_value_array)
    {
        if (!pName_value_array->is_array())
        {
            vogl_error_printf("name_value_map is not an array\n");
            print_json_context(pDocument_filename, *pName_value_array);
            return false;
        }

        uint64_t size = pName_value_array->size();
        VOGL_NOTE_UNUSED(size);

        uint32_t name_value_index = 0;

        for (uint32_t name_value_node_iter = 0; name_value_node_iter < pName_value_array->size(); name_value_node_iter++)
        {
            const json_node *pItem_node = pName_value_array->get_value_as_object(name_value_node_iter);
            if (!pItem_node)
            {
                vogl_error_printf("Expected object in name_value_map array\n");
                print_json_context(pDocument_filename, *pName_value_array, name_value_node_iter);
                return false;
            }
            const json_node &item_node = *pItem_node;

            dynamic_string key_str(item_node.value_as_string("key"));
            if (key_str.is_empty())
            {
                vogl_error_printf("Expected key in name_value_map array\n");
                print_json_context(pDocument_filename, *pName_value_array, name_value_node_iter);
                return false;
            }

            const char *pKey_type_str = item_node.value_as_string_ptr("key_type");
            const char *pValue_type_str = item_node.value_as_string_ptr("val_type");

            if ((!pKey_type_str) || (!pValue_type_str))
            {
                vogl_error_printf("Invalid data_types array in name_value_map object\n");
                print_json_context(pDocument_filename, *pName_value_array, name_value_node_iter);
                return false;
            }

            value_data_type key_type = string_to_value_data_type(pKey_type_str);
            value_data_type value_type = string_to_value_data_type(pValue_type_str);
            if ((key_type == cDTInvalid) || (value_type == cDTInvalid))
            {
                vogl_error_printf("Invalid data_types array in name_value_map object\n");
                print_json_context(pDocument_filename, *pName_value_array, name_value_node_iter);
                return false;
            }

            value key, val;
            if (!key.parse(key_str.get_ptr()))
            {
                vogl_error_printf("Failed parsing name_value_map key string %s\n", key_str.get_ptr());
                print_json_context(pDocument_filename, *pName_value_array, name_value_node_iter);
                return false;
            }

            if (!force_value_to_type(key, key_type))
            {
                vogl_error_printf("Failed parsing name_value_map key %s\n", key_str.get_ptr());
                print_json_context(pDocument_filename, *pName_value_array, name_value_node_iter);
                return false;
            }

            const json_value &json_val = item_node.find_value("data");

            if ((value_type == cDTBlob) && (json_val.is_object()))
            {
                //vogl_error_printf("Expected JSON object for name_value_map node key %s\n", key_str.get_ptr());
                //print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                //return false;

                const json_node &blob_node = *json_val.get_node_ptr();

                dynamic_string blob_id;
                if (!blob_node.get_value_as_string("blob_id", blob_id))
                {
                    vogl_error_printf("Failed parsing blob_file key in name_value_map node key %s\n", key_str.get_ptr());
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                    return false;
                }

                uint64_t blob_crc64 = 0;
                if (!get_uint64_from_json_node(blob_node, "crc64", blob_crc64))
                {
                    vogl_error_printf("Failed parsing crc64 key in name_value_map node key %s\n", key_str.get_ptr());
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                    return false;
                }

                uint64_t blob_size = 0;
                if (!get_uint64_from_json_node(blob_node, "size", blob_size))
                {
                    vogl_error_printf("Failed parsing size key in name_value_map node key %s\n", key_str.get_ptr());
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                    return false;
                }

                if (blob_size > cUINT32_MAX)
                {
                    vogl_error_printf("Blob size is too large (%" PRIu64 "), blob id %s key %s\n", blob_size, blob_id.get_ptr(), key_str.get_ptr());
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                    return false;
                }

                if (!pBlob_manager)
                {
                    vogl_error_printf("Encountered data blob in packet but no blob manager was specified, blob size %" PRIu64 " blob id %s key %s\n", blob_size, blob_id.get_ptr(), key_str.get_ptr());
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                    return false;
                }

                uint8_vec blob;
                if (!pBlob_manager->get(blob_id, blob))
                {
                    vogl_error_printf("Failed retrieving data blob, blob size %" PRIu64 " blob id %s key %s CRC64 0x%" PRIX64 "\n", blob_size, blob_id.get_ptr(), key_str.get_ptr(), blob_crc64);
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                    return false;
                }

                uint64_t actual_blob_size = blob.size();

                // TODO: We'll eventually support enormous blob files
                if (actual_blob_size > cUINT32_MAX)
                {
                    vogl_error_printf("Blob size is too large (%" PRIu64 "), blob id %s key %s\n", actual_blob_size, blob_id.get_ptr(), key_str.get_ptr());
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                    return false;
                }

                if (actual_blob_size != blob_size)
                {
                    vogl_warning_printf("Unexpected size of blob id %s (should be %" PRIu64 " bytes, but is %" PRIu64 " bytes), reading all of file and hoping for the best\n", blob_id.get_ptr(), blob_size, actual_blob_size);
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter, cMsgWarning);
                }

                uint64_t check_crc64 = calc_crc64(CRC64_INIT, blob.get_ptr(), blob.size());
                if (check_crc64 != blob_crc64)
                {
                    vogl_warning_printf("name_value_map blob file %s's CRC64 is bad (expected 0x%016" PRIX64 " got 0x%016" PRIX64 ")\n", blob_id.get_ptr(), blob_crc64, check_crc64);
                    print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter, cMsgWarning);
                }

                val.set_blob_take_ownership(blob);
            }
            else if (value_type == cDTJSONDoc)
            {
                *val.init_json_document() = json_val;
            }
            else
            {
                switch (json_val.get_type())
                {
                    case cJSONValueTypeBool:
                    case cJSONValueTypeInt:
                    {
                        val.set_int64(json_val.as_int64());
                        break;
                    }
                    case cJSONValueTypeDouble:
                    {
                        val.set_double(json_val.as_double());
                        break;
                    }
                    case cJSONValueTypeString:
                    {
                        const char *pStr = json_val.as_string_ptr();
                        if (!pStr || !val.parse(pStr))
                        {
                            vogl_error_printf("Failed parsing name_value_map value type for key %s\n", key_str.get_ptr());
                            print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                            return false;
                        }
                        break;
                    }
                    default:
                    {
                        // Just skip it.
                        vogl_error_printf("Unexpected name_value_map value type for key %s\n", key_str.get_ptr());
                        print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                        break;
                    }
                }
            }

            if (!force_value_to_type(val, value_type))
            {
                vogl_error_printf("Failed parsing value for name_value_map key %s\n", key_str.get_ptr());
                print_json_context(pDocument_filename, json_val, *pName_value_array, name_value_node_iter);
                return false;
            }
            m_key_value_map.insert(key, val);

            name_value_index++;

        } // name_value_node_iter
    }

    if (!fixup_manually_edited_params(gl_entrypoint_id, gl_func_name.get_ptr(), call_counter))
        return false;

    m_is_valid = true;

    VOGL_ASSERT(check());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::validate_value_conversion
// Returns true if the dest type is at least large enough to hold the parameter (or the same size in bytes for floats).
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::validate_value_conversion(uint32_t dest_type_size, uint32_t dest_type_loki_type_flags, int param_index) const
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT((dest_type_size >= 1) && (dest_type_size <= 8));

    const vogl_ctype_desc_t &param_ctype_desc = (param_index < 0) ? get_return_value_ctype_desc() : get_param_ctype_desc(param_index);
    uint64_t param_data = (param_index < 0) ? get_return_value_data() : get_param_data(param_index);
    uint32_t param_size = param_ctype_desc.m_size;

    if (param_ctype_desc.m_is_pointer)
    {
        // This func is not really intended to be used with pointer types, but try to do something.
        return (dest_type_size >= m_pCTypes->get_pointer_size());
    }

    if (param_ctype_desc.m_loki_type_flags & LOKI_TYPE_BITMASK(LOKI_IS_FLOAT))
    {
        return dest_type_size == static_cast<uint32_t>(param_ctype_desc.m_size);
    }

    // unsigned ints = unsigned char, unsigned short int,unsigned int, unsigned long int, unsigned long long int
    // signed ints = signed char, short int,int, long int, long long int
    // integral = bool, char, wchar_t, or signed/unsigned ints
    bool param_is_signed = (param_ctype_desc.m_loki_type_flags & (LOKI_TYPE_BITMASK(LOKI_IS_SIGNED_INT) | LOKI_TYPE_BITMASK(LOKI_IS_SIGNED_LONG))) != 0;
    bool param_is_unsigned = (param_ctype_desc.m_loki_type_flags & (LOKI_TYPE_BITMASK(LOKI_IS_UNSIGNED_INT) | LOKI_TYPE_BITMASK(LOKI_IS_UNSIGNED_LONG))) != 0;
    VOGL_NOTE_UNUSED(param_is_unsigned);
    bool param_is_integral = (param_ctype_desc.m_loki_type_flags & LOKI_TYPE_BITMASK(LOKI_IS_INTEGRAL)) != 0;

    bool dest_is_signed = (dest_type_loki_type_flags & (LOKI_TYPE_BITMASK(LOKI_IS_SIGNED_INT) | LOKI_TYPE_BITMASK(LOKI_IS_SIGNED_LONG))) != 0;
    bool dest_is_unsigned = (dest_type_loki_type_flags & (LOKI_TYPE_BITMASK(LOKI_IS_UNSIGNED_INT) | LOKI_TYPE_BITMASK(LOKI_IS_UNSIGNED_LONG))) != 0;
    VOGL_NOTE_UNUSED(dest_is_unsigned);
    bool dest_is_integral = (dest_type_loki_type_flags & LOKI_TYPE_BITMASK(LOKI_IS_INTEGRAL)) != 0;

    if (param_is_integral != dest_is_integral)
        return false;

    bigint128 dest_min_val, dest_max_val;
    bigint128::get_type_range(dest_type_size, dest_is_signed, dest_min_val, dest_max_val);

    // not endian safe
    bigint128 param_val(&param_data, param_size, param_is_signed);

    if (dest_is_signed)
    {
        return (param_val >= dest_min_val) && (param_val <= dest_max_val);
    }
    else
    {
        return param_val.unsigned_less_equal(dest_max_val);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::should_always_write_as_blob_file
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::should_always_write_as_blob_file(const char *pFunc_name)
{
    VOGL_FUNC_TRACER

    if ((strstr(pFunc_name, "ShaderSource") != NULL) ||
        (strstr(pFunc_name, "TexSubImage") != NULL) ||
        (strstr(pFunc_name, "TexImage") != NULL))
        return true;
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::ctype_to_json_string
// returns false if the returned string is a valid JSON numeric (otherwise it's a string)
//----------------------------------------------------------------------------------------------------------------------
void vogl_trace_packet::ctype_to_json_value(json_value &val, gl_entrypoint_id_t entrypoint_id, int param_index, uint64_t data, vogl_ctype_t ctype) const
{
    VOGL_FUNC_TRACER

    switch (ctype)
    {
        case VOGL_BOOL:
        case VOGL_GLBOOLEAN:
        {
            if (data == 0)
            {
                val.set_value(false);
                return;
            }
            else if (data == 1)
            {
                val.set_value(true);
                return;
            }

            break;
        }
        case VOGL_GLENUM:
        {
            const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_id];
            VOGL_NOTE_UNUSED(entrypoint_desc);
            VOGL_ASSERT((param_index == -1) || (static_cast<uint32_t>(param_index) < entrypoint_desc.m_num_params));

            const char *pName = get_gl_enums().find_name(data, entrypoint_id, param_index);

            if (pName)
            {
                VOGL_ASSERT(get_gl_enums().find_enum(pName) == data);

                val.set_value(pName);
                return;
            }

            break;
        }
        case VOGL_FLOAT:
        case VOGL_GLFLOAT:
        case VOGL_GLCLAMPF:
        {
            float flt_val = *reinterpret_cast<const float *>(&data);
            val.set_value(flt_val);

            dynamic_string val_as_str;
            val.serialize(val_as_str, false);

            json_value test_val;
            if ((!test_val.deserialize(val_as_str)) || (test_val.as_float() != flt_val))
                break;

            return;
        }
        case VOGL_GLDOUBLE:
        case VOGL_GLCLAMPD:
        {
            double dbl_val = *reinterpret_cast<const double *>(&data);
            val.set_value(dbl_val);

            dynamic_string val_as_str;
            val.serialize(val_as_str, false);

            json_value test_val;
            if ((!test_val.deserialize(val_as_str)) || (test_val.as_double() != dbl_val))
                break;

            return;
        }
        case VOGL_GLINT:
        case VOGL_INT:
        case VOGL_INT32T:
        case VOGL_GLSIZEI:
        case VOGL_GLFIXED:
        {
            val.set_value(static_cast<int32_t>(data));
            return;
        }
        case VOGL_GLSHORT:
        {
            val.set_value(static_cast<int16_t>(data));
            return;
        }
        case VOGL_GLBYTE:
        {
            val.set_value(static_cast<int8_t>(data));
            return;
        }
        case VOGL_GLINT64:
        case VOGL_GLINT64EXT:
        {
            val.set_value(static_cast<int64_t>(data));
            return;
        }
        default:
        {
            break;
        }
    }

    val.set_value(dynamic_string(cVarArg, "0x%" PRIX64, data).get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::json_serialize_param
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::json_serialize_param(
    const char *pFunc_name,
    json_node &gl_params_node, int param_index, const char *pName, const uint64_t param_data, const uint32_t param_size, vogl_ctype_t param_ctype,
    const void *pClient_mem, uint32_t client_mem_size, vogl_ctype_t client_mem_ctype,
    const json_serialize_params &params) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(param_size);
    VOGL_NOTE_UNUSED(client_mem_ctype);

    const vogl_ctype_desc_t &param_ctype_desc = (*m_pCTypes)[param_ctype];

    const bool has_client_memory = (pClient_mem != NULL);

    if ((param_ctype_desc.m_is_pointer) && (param_data) && (has_client_memory))
    {
        const vogl_ctype_t pointee_ctype = param_ctype_desc.m_pointee_ctype;
        const uint32_t pointee_size = (*m_pCTypes)[pointee_ctype].m_size;
        const bool pointee_is_ptr = (*m_pCTypes)[pointee_ctype].m_is_pointer;

        bool print_as_cstring = false;
        switch (param_ctype)
        {
            case VOGL_CONST_GLUBYTE_PTR:
            case VOGL_CONST_GLCHAR_PTR:
            case VOGL_GLCHARARB_PTR:
            case VOGL_CONST_GLBYTE_PTR:
            case VOGL_GLUBYTE_PTR:
            case VOGL_GLCHAR_PTR:
            case VOGL_CONST_GLCHARARB_PTR:
            case VOGL_LPCSTR:
            {
                if ((client_mem_size <= 8192) && (utils::is_buffer_printable(pClient_mem, client_mem_size, true, true)))
                {
                    print_as_cstring = true;
                    break;
                }
                break;
            }
            default:
                break;
        }

        if (print_as_cstring)
        {
            if ((client_mem_size >= 2) && (static_cast<const char *>(pClient_mem)[0] == '0') && (static_cast<const char *>(pClient_mem)[1] == 'x'))
                print_as_cstring = false;
            //else if (get_vogl_entrypoint_hashmap().find(static_cast<const char *>(pClient_mem)) != get_vogl_entrypoint_hashmap().end())
            //   print_as_cstring = false;
        }

        json_node &param_node = gl_params_node.add_object(pName);

        if (print_as_cstring)
        {
            const char *pStr = reinterpret_cast<const char *>(pClient_mem);

            // this was causing a crash and as far as I could tell it was a clang optimizer bug?
            //dynamic_string str(cVarArg, "%s", pStr);
            dynamic_string str(pStr);
            param_node.add_key_value("string", str);

            param_node.add_key_value("ptr", dynamic_string(cVarArg, "0x%016" PRIX64, param_data));
        }
        else
        {
            param_node.add_key_value("ptr", dynamic_string(cVarArg, "0x%016" PRIX64, param_data));
            param_node.add_key_value("mem_size", client_mem_size);

            uint64_t client_mem_crc64 = calc_crc64(CRC64_INIT, reinterpret_cast<const uint8_t *>(pClient_mem), client_mem_size);
            param_node.add_key_value("crc64", dynamic_string(cVarArg, "0x%016" PRIX64, client_mem_crc64));

            if ((pointee_size >= 1) && (pointee_size <= 8) && (math::is_power_of_2(pointee_size)) && (!(client_mem_size % pointee_size)))
            {
                json_node &param_data_values_node = param_node.add_array("values");

                const uint32_t client_mem_array_size = client_mem_size / pointee_size;

                for (uint32_t i = 0; i < client_mem_array_size; i++)
                {
                    const void *p = reinterpret_cast<const uint8_t *>(pClient_mem) + i * pointee_size;

                    uint64_t data = 0;
                    if (pointee_size == sizeof(uint8_t))
                        data = *reinterpret_cast<const uint8_t *>(p);
                    else if (pointee_size == sizeof(uint16_t))
                        data = *reinterpret_cast<const uint16_t *>(p);
                    else if (pointee_size == sizeof(uint32_t))
                        data = *reinterpret_cast<const uint32_t *>(p);
                    else if (pointee_size == sizeof(uint64_t))
                        data = *reinterpret_cast<const uint64_t *>(p);

                    json_value val;
                    ctype_to_json_value(val, static_cast<gl_entrypoint_id_t>(m_packet.m_entrypoint_id), param_index, data, pointee_ctype);

                    param_data_values_node.add_value(val);
                }
            }
            else
            {
                if ((!should_always_write_as_blob_file(pFunc_name)) && (client_mem_size < params.m_blob_file_size_threshold))
                {
                    dynamic_string buf;

                    buf.set_len(2 + client_mem_size * 2);
                    buf.set_char(0, '0');
                    buf.set_char(1, 'x');

                    for (uint32_t i = 0; i < client_mem_size; i++)
                    {
                        const uint8_t c = reinterpret_cast<const uint8_t *>(pClient_mem)[(client_mem_size - 1) - i];
                        buf.set_char(2 + i * 2 + 0, utils::to_hex((c >> 4) & 0xF));
                        buf.set_char(2 + i * 2 + 1, utils::to_hex(c & 0xF));
                    }

                    param_node.add_key_value("bytes", buf);
                }
                else
                {
                    dynamic_string id;

                    if (params.m_pBlob_manager)
                    {
                        //dynamic_string prefix(cVarArg, "%s_%s_param_%i", params.m_output_basename.get_ptr(), pFunc_name, param_index);
                        dynamic_string prefix(cVarArg, "%s_%s", pFunc_name, g_vogl_entrypoint_param_descs[m_packet.m_entrypoint_id][param_index].m_pName);

                        id = params.m_pBlob_manager->add_buf_compute_unique_id(pClient_mem, client_mem_size, prefix, "blob", &client_mem_crc64);
                        if (id.is_empty())
                        {
                            vogl_error_printf("Failed adding blob %s to blob manager\n", prefix.get_ptr());
                            return false;
                        }

                        vogl_message_printf("Wrote blob id %s\n", id.get_ptr());
                    }
                    else
                    {
                        id = "<blob_writing_disabled>";
                    }

                    param_node.add_key_value("blob_id", id);
                }
            }

            if (params.m_write_debug_info)
            {
                json_node &param_dbg_node = param_node.add_object("dbg");
                param_dbg_node.add_key_value("ctype", (*m_pCTypes)[param_ctype].m_pCType);
                param_dbg_node.add_key_value("pointee_ctype", (*m_pCTypes)[pointee_ctype].m_pCType);
                param_dbg_node.add_key_value("pointee_ctype_size", (*m_pCTypes)[pointee_ctype].m_size);
                param_dbg_node.add_key_value("pointee_ctype_is_opaque", (*m_pCTypes)[pointee_ctype].m_is_opaque_pointer);
                param_dbg_node.add_key_value("pointee_is_ptr", pointee_is_ptr);
                param_dbg_node.add_key_value("pointee_size", pointee_size);
            }
        }
    }
    else
    {
        json_value param_value;
        // This helper can return a hex string (beginning with 0x chars) for values that can't be losslessly converted to a JSON numeric
        ctype_to_json_value(param_value, static_cast<gl_entrypoint_id_t>(m_packet.m_entrypoint_id), param_index, param_data, param_ctype);
        gl_params_node.add_key_value(pName, param_value);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::convert_json_value_to_ctype_data
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::convert_json_value_to_ctype_data(uint64_t &data, const json_value &val, vogl_ctype_t ctype, const char *pName, int param_iter, gl_entrypoint_param_class_t param_class, bool permit_client_strings, const json_node &node, const char *pDocument_filename)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(param_class);

    const bool is_pointer = (*m_pCTypes)[ctype].m_is_pointer;

    // could either be a pointer to a string, or pointer value with no associated client memory, or a param value
    data = 0;

    double dbl_data = 0;
    uint32_t parsed_size = 0;
    VOGL_NOTE_UNUSED(parsed_size);
    bool parsed_double = false;
    bool parsed_hex = false;

    if (val.is_string())
    {
        dynamic_string str(val.as_string());

        if ((str.get_len() >= 3) && (str[0] == '0') && (str[1] == 'x'))
        {
            // hex parameter data - should be <= sizeof(uint64_t)
            // could be a pointer to unspecified client memory, or a non-ptr param value that couldn't be serialized to a regular json type

            const char *pBuf = str.get_ptr();
            if (!string_ptr_to_uint64(pBuf, data))
            {
                vogl_error_printf("Failed parsing hex numeric value of parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node);
                return false;
            }
            parsed_size = sizeof(uint64_t);
            parsed_hex = true;
        }
        else if (is_pointer)
        {
            if (!permit_client_strings)
            {
                vogl_error_printf("Unexpected string for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node);
                return false;
            }

            // must be a zero terminated string originally from client memory
            data = 0xFFFFFFFF;
            parsed_size = sizeof(uint32_t);

            if (!is_pointer)
            {
                vogl_error_printf("Encountered string for non-pointer parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node);
                return false;
            }

            client_memory_desc_t &mem_desc = m_client_memory_descs[param_iter];
            mem_desc.m_vec_ofs = m_client_memory.size();
            mem_desc.m_data_size = str.get_len() + 1;
            mem_desc.m_pointee_ctype = (*m_pCTypes)[ctype].m_pointee_ctype;

            m_client_memory.append(reinterpret_cast<const uint8_t *>(str.get_ptr()), str.get_len() + 1);
        }
        else
        {
            uint64_t gl_enum = get_gl_enums().find_enum(str);
            if (gl_enum == gl_enums::cUnknownEnum)
            {
                vogl_error_printf("Unexpected string for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node);
                return false;
            }

            // TODO - Fix this so it doesn't downcast to uint32_t (can it safely return uint64_t?)
            VOGL_ASSERT(gl_enum <= cUINT32_MAX);

            // a GL enum
            data = static_cast<uint32_t>(gl_enum);
            parsed_size = sizeof(uint32_t);
        }
    }
    else
    {
        // parameter data
        if (val.is_bool())
        {
            bool b = val.as_bool();
            memcpy(&data, &b, 1);
            parsed_size = 1;
        }
        else if (val.is_double())
        {
            dbl_data = val.as_double();
            memcpy(&data, &dbl_data, sizeof(dbl_data));
            parsed_size = sizeof(dbl_data);
            parsed_double = true;
        }
        else if (val.is_int())
        {
            int64_t v = val.as_int64();
            memcpy(&data, &v, sizeof(data));
            parsed_size = sizeof(int64_t);
        }
        else
        {
            vogl_error_printf("Invalid JSON node type for parameter %s\n", pName);
            print_json_context(pDocument_filename, val, node);
            return false;
        }
    }

    int64_t idata = static_cast<int64_t>(data);

    switch (ctype)
    {
        case VOGL_FLOAT:
        case VOGL_GLFLOAT:
        case VOGL_GLCLAMPF:
        {
            float v = 0;
            if (parsed_double)
                v = static_cast<float>(dbl_data);
            else if (parsed_hex)
                v = *reinterpret_cast<const float *>(&data);
            else
            {
                v = static_cast<float>(idata);
                vogl_debug_printf("Parameter %s should be a float, but a non-float value was specified in the JSON file\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgDebug);
            }

            data = 0;
            memcpy(&data, &v, sizeof(v));
            break;
        }
        case VOGL_GLDOUBLE:
        case VOGL_GLCLAMPD:
        {
            double v = 0;
            if (parsed_double)
                v = dbl_data;
            else if (parsed_hex)
                v = *reinterpret_cast<const double *>(&data);
            else
            {
                v = static_cast<double>(idata);
                vogl_debug_printf("Parameter %s should be a double, but a non-double value was specified in the JSON file\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgDebug);
            }
            memcpy(&data, &v, sizeof(v));
            break;
        }
        default:
        {
            if (parsed_double)
            {
                vogl_warning_printf("Float point value specified for non-float parameter %s, casting to integer\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgWarning);
                data = idata = static_cast<int64_t>(dbl_data);
            }
            break;
        }
    } // switch (ctype)

    switch (ctype)
    {
        case VOGL_BOOL:
        case VOGL_GLBOOLEAN:
        {
            //if ((data != 0) && (data != 1))
            //   vogl_warning_printf("Expected bool value for parameter %s\n", pName);
            break;
        }
        case VOGL_GLBITFIELD:
        case VOGL_GLUINT:
        case VOGL_UNSIGNED_INT:
        case VOGL_GLENUM:
        case VOGL_GLHANDLEARB:
        {
            if (data > cUINT32_MAX)
            {
                vogl_warning_printf("Value out of range for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgWarning);
            }
            break;
        }
        case VOGL_GLINT:
        case VOGL_INT:
        case VOGL_INT32T:
        case VOGL_GLFIXED:
        case VOGL_GLSIZEI:
        {
            if ((idata < cINT32_MIN) || (idata > cINT32_MAX))
            {
                vogl_warning_printf("Value out of range for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgWarning);
            }
            break;
        }
        case VOGL_GLBYTE:
        case VOGL_GLCHARARB:
        {
            if ((idata < -cINT8_MAX) || (idata > cINT8_MAX))
            {
                vogl_warning_printf("Value out of range for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgWarning);
            }
            break;
        }
        case VOGL_GLCHAR:
        case VOGL_GLUBYTE:
        {
            if (data > cUINT8_MAX)
            {
                vogl_warning_printf("Value out of range for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgWarning);
            }
            break;
        }
        case VOGL_GLHALFNV:
        case VOGL_GLUSHORT:
        {
            if (data > cUINT16_MAX)
            {
                vogl_warning_printf("Value out of range for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgWarning);
            }
            break;
        }
        case VOGL_GLSHORT:
        {
            if ((idata < cINT16_MIN) || (idata > cINT16_MAX))
            {
                vogl_warning_printf("Value out of range for parameter %s\n", pName);
                print_json_context(pDocument_filename, val, node, cMsgWarning);
            }
            break;
        }
        default:
            break;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::json_deserialize_param
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::json_deserialize_param(
    uint64_t cur_call_counter, const char *pFunc_name,
    const json_node &params_node, uint32_t param_iter, const char *pParam_name, gl_entrypoint_param_class_t param_class, const vogl_ctype_desc_t &param_ctype_desc,
    const char *pDocument_filename, const vogl_blob_manager *pBlob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(cur_call_counter);

    int param_index = params_node.find_key(pParam_name);
    if (param_index < 0)
    {
        // HACK HACK
        bool fixed = false;
        if (strncmp(pParam_name, "_near", 5) == 0)
        {
            vogl_warning_printf("Failed finding function parameter \"_near\", attempting to use \"near\" instead\n");
            param_index = params_node.find_key("near");
            fixed = (param_index >= 0);
        }
        else if (strncmp(pParam_name, "_far", 4) == 0)
        {
            vogl_warning_printf("Failed finding function parameter \"_far\", attempting to use \"far\" instead\n");
            param_index = params_node.find_key("far");
            fixed = (param_index >= 0);
        }

        if (!fixed)
        {
            vogl_error_printf("Failed finding function parameter \"%s\"\n", pParam_name);
            print_json_context(pDocument_filename, params_node);
            return false;
        }
    }

    const json_value &val = params_node.get_value(param_index);
    if ((val.is_null()) || (val.is_array()))
    {
        vogl_error_printf("Invalid JSON node type for parameter %s\n", pParam_name);
        print_json_context(pDocument_filename, params_node);
        return false;
    }

    if (val.is_object())
    {
        // must be pointer to client memory of some sort
        if (!param_ctype_desc.m_is_pointer)
        {
            vogl_error_printf("A memory object was specified for parameter %s, but parameter is not a pointer to client memory\n", pParam_name);
            print_json_context(pDocument_filename, val, params_node);
            return false;
        }

        vogl_ctype_t pointee_ctype = param_ctype_desc.m_pointee_ctype;
        VOGL_ASSERT(pointee_ctype != VOGL_INVALID_CTYPE);

        uint32_t pointee_ctype_size = (*m_pCTypes)[pointee_ctype].m_size;

        const json_node *pParam_obj = val.get_node_ptr();

        uint64_t ptr = 0;
        if (!get_uint64_from_json_node(*pParam_obj, "ptr", ptr, 0))
        {
            vogl_warning_printf("Failed parsing ptr field of parameter %s (the pointer is only for debugging, not replay, so this is not a hard failure)\n", pParam_name);
            print_json_context(pDocument_filename, *pParam_obj, cMsgWarning);
        }

        if (pParam_obj->has_key("string"))
        {
            const char *pStr = pParam_obj->find_value("string").as_string_ptr();
            uint32_t str_len = vogl_strlen(pStr);

            client_memory_desc_t &mem_desc = m_client_memory_descs[param_iter];
            mem_desc.m_vec_ofs = m_client_memory.size();
            mem_desc.m_data_size = str_len + 1;
            mem_desc.m_pointee_ctype = pointee_ctype;

            m_client_memory.append(reinterpret_cast<const uint8_t *>(pStr), str_len + 1);

            m_param_data[param_iter] = ptr;

            VOGL_ASSERT(param_ctype_desc.m_size <= cUINT8_MAX);
            m_param_size[param_iter] = param_ctype_desc.m_size;
            m_param_ctype[param_iter] = param_ctype_desc.m_ctype;
        }
        else
        {
            uint64_t mem_size = 0;
            if (!get_uint64_from_json_node(*pParam_obj, "mem_size", mem_size, 0))
            {
                vogl_error_printf("Failed parsing mem_size field of parameter %s\n", pParam_name);
                print_json_context(pDocument_filename, *pParam_obj);
                return false;
            }

            if (mem_size > 0x7FFF0000)
            {
                vogl_error_printf("Client memory blob size is too large for parameter %s\n", pParam_name);
                print_json_context(pDocument_filename, *pParam_obj);
                return false;
            }

            uint64_t blob_crc64 = 0;
            bool has_blob_crc64 = get_uint64_from_json_node(*pParam_obj, "crc64", blob_crc64, 0);

            vogl::vector<uint8_t> blob(static_cast<uint32_t>(mem_size));

            if (pParam_obj->find_child("values"))
            {
                if (pointee_ctype_size <= 0)
                {
                    vogl_error_printf("Can't specify a values array for void* parameter %s\n", pParam_name);
                    print_json_context(pDocument_filename, *pParam_obj);
                    return false;
                }

                const json_node *pValues = pParam_obj->find_child("values");

                if (!pValues->is_array())
                {
                    vogl_error_printf("Expected array for values field of parameter %s\n", pParam_name);
                    print_json_context(pDocument_filename, *pValues);
                    return false;
                }

                uint32_t num_vals = pValues->size();
                if (num_vals != (mem_size / pointee_ctype_size))
                {
                    vogl_error_printf("Array size is invalid for parameter %s\n", pParam_name);
                    print_json_context(pDocument_filename, *pValues);
                    return false;
                }

                if ((mem_size % pointee_ctype_size) != 0)
                {
                    vogl_error_printf("Mem size is invalid for parameter %s\n", pParam_name);
                    print_json_context(pDocument_filename, *pValues);
                    return false;
                }

                for (uint32_t val_iter = 0; val_iter < num_vals; val_iter++)
                {
                    const json_value &val2 = pValues->get_value(val_iter);

                    uint64_t param_data;
                    if (!convert_json_value_to_ctype_data(param_data, val2, pointee_ctype, "values", -1, VOGL_VALUE_PARAM, false, *pValues, pDocument_filename))
                        return false;

                    memcpy(blob.get_ptr() + val_iter * pointee_ctype_size, &param_data, pointee_ctype_size);
                }
            }
            else if (pParam_obj->has_key("blob_id"))
            {
                dynamic_string blob_id;
                pParam_obj->get_value_as_string("blob_id", blob_id);

                if (!pBlob_manager)
                {
                    vogl_error_printf("No blob manager specified, blob id %s\n", blob_id.get_ptr());
                    print_json_context(pDocument_filename, *pParam_obj);
                    return false;
                }

                uint8_vec data_blob;
                if (!pBlob_manager->get(blob_id, data_blob))
                {
                    vogl_error_printf("Failed retrieving data blob, blob size %" PRIu64 " blob id %s CRC64 0x%" PRIX64 "\n", mem_size, blob_id.get_ptr(), blob_crc64);
                    print_json_context(pDocument_filename, *pParam_obj);
                    return false;
                }

                // TODO: We'll support uint64_t blob sizes eventually
                uint64_t file_size = data_blob.size();
                if (file_size > cUINT32_MAX)
                {
                    vogl_error_printf("Blob file %s is too large (%" PRIu64 ")\n", blob_id.get_ptr(), file_size);
                    print_json_context(pDocument_filename, *pParam_obj);
                    return false;
                }

                blob.swap(data_blob);

                if (file_size != mem_size)
                {
                    vogl_warning_printf("Unexpected size of blob file %s (should be %" PRIu64 " bytes, but is %" PRIu64 " bytes), reading all of file and hoping for the best\n", blob_id.get_ptr(), mem_size, file_size);
                    print_json_context(pDocument_filename, *pParam_obj, cMsgWarning);
                }
            }
            else if (pParam_obj->has_key("bytes"))
            {
                const json_value &bytes_val = pParam_obj->find_value("bytes");
                const char *pByte_str = bytes_val.as_string_ptr();
                if (!pByte_str)
                {
                    vogl_error_printf("Expected bytes value string for parameter %s\n", pFunc_name);
                    print_json_context(pDocument_filename, *pParam_obj);
                    return false;
                }

                uint32_t bytes_len = vogl_strlen(pByte_str);
                if ((bytes_len != 2 + 2 * mem_size) || (pByte_str[0] != '0') || (pByte_str[1] != 'x'))
                {
                    vogl_error_printf("Can't parse bytes value string for parameter %s (either size is bad, or string doesn't start with 0x)\n", pFunc_name);
                    print_json_context(pDocument_filename, *pParam_obj);
                    return false;
                }

                for (uint32_t i = 0; i < mem_size; i++)
                {
                    char c0 = pByte_str[2 + i * 2 + 0];
                    char c1 = pByte_str[2 + i * 2 + 1];

                    int val_c0 = utils::from_hex(c0);
                    int val_c1 = utils::from_hex(c1);

                    if ((val_c0 < 0) || (val_c1 < 0))
                    {
                        vogl_error_printf("Non-hex char in bytes value at byte index %u for parameter %s\n", i, pFunc_name);
                        print_json_context(pDocument_filename, *pParam_obj);
                        return false;
                    }

                    blob[(static_cast<uint32_t>(mem_size) - 1) - i] = (val_c0 << 4) | (val_c1);
                }
            }
            else
            {
                vogl_error_printf("Failed parsing array parameter %s\n", pParam_name);
                print_json_context(pDocument_filename, *pParam_obj);
                return false;
            }

            if (has_blob_crc64)
            {
                uint64_t check_crc64 = calc_crc64(CRC64_INIT, blob.get_ptr(), static_cast<size_t>(mem_size));
                if (check_crc64 != blob_crc64)
                {
                    vogl_warning_printf("Blob CRC64 check failed for parameter %s (expected 0x%016" PRIX64 " got 0x%016" PRIX64 ")\n", pParam_name, blob_crc64, check_crc64);
                    print_json_context(pDocument_filename, *pParam_obj, cMsgWarning);
                }
            }

            m_param_data[param_iter] = ptr;

            VOGL_ASSERT(param_ctype_desc.m_size <= cUINT8_MAX);
            m_param_size[param_iter] = param_ctype_desc.m_size;
            m_param_ctype[param_iter] = param_ctype_desc.m_ctype;

            client_memory_desc_t &mem_desc = m_client_memory_descs[param_iter];
            mem_desc.m_vec_ofs = m_client_memory.size();
            mem_desc.m_data_size = blob.size();
            mem_desc.m_pointee_ctype = pointee_ctype;

            m_client_memory.append(blob.get_ptr(), blob.size());
        }
    }
    else
    {
        uint64_t param_data;
        if (!convert_json_value_to_ctype_data(param_data, val, param_ctype_desc.m_ctype, pParam_name, param_iter, param_class, true, params_node, pDocument_filename))
            return false;

        m_param_data[param_iter] = 0;
        memcpy(&m_param_data[param_iter], &param_data, param_ctype_desc.m_size);

        VOGL_ASSERT(param_ctype_desc.m_size <= cUINT8_MAX);
        m_param_size[param_iter] = param_ctype_desc.m_size;
        m_param_ctype[param_iter] = param_ctype_desc.m_ctype;

    } // val.is_object()

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::get_uint64_from_json_node
// TODO: This is now outdated, we can retrieve uint64_t's directly using members in the json_node class
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::get_uint64_from_json_node(const json_node &node, const char *pKey, uint64_t &val, uint64_t def)
{
    VOGL_FUNC_TRACER

    int key_index = node.find_key(pKey);
    if (key_index < 0)
    {
        val = def;
        return false;
    }

    const json_value &json_val = node.get_value(key_index);

    if (json_val.is_string())
    {
        const char *pBuf = json_val.as_string_ptr();
        if (!string_ptr_to_uint64(pBuf, val))
        {
            val = def;
            return false;
        }
    }
    else
    {
        int64_t ival;
        if (!json_val.get_numeric(ival, static_cast<int64_t>(def)))
        {
            val = def;
            return false;
        }
        VOGL_ASSERT(ival >= 0);
        val = static_cast<uint64_t>(ival);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::force_value_to_type
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::force_value_to_type(value &val, value_data_type type)
{
    VOGL_FUNC_TRACER

    switch (type)
    {
        case cDTBool:
        {
            val.set_bool(val.get_bool());
            break;
        }
        case cDTInt8:
        {
            val.set_int8(val.get_int8());
            break;
        }
        case cDTUInt8:
        {
            val.set_uint8(val.get_uint8());
            break;
        }
        case cDTInt16:
        {
            val.set_int16(val.get_int16());
            break;
        }
        case cDTUInt16:
        {
            val.set_uint16(val.get_uint16());
            break;
        }
        case cDTInt:
        {
            val.set_int(val.get_int());
            break;
        }
        case cDTUInt:
        {
            val.set_uint(val.get_uint());
            break;
        }
        case cDTInt64:
        {
            val.set_int64(val.get_int64());
            break;
        }
        case cDTUInt64:
        {
            val.set_uint64(val.get_uint64());
            break;
        }
        case cDTFloat:
        {
            val.set_float(val.get_float());
            break;
        }
        case cDTDouble:
        {
            val.set_double(val.get_double());
            break;
        }
        case cDTVoidPtr:
        {
            val.set_void_ptr((void *)val.get_uint64());
            break;
        }
        case cDTString:
        {
            if (val.get_data_type() != cDTString)
                return false;
            break;
        }
        case cDTStringHash:
        {
            val.set_string_hash(val.get_string_hash());
            break;
        }
        case cDTBlob:
        {
            if (val.get_data_type() != cDTBlob)
                return false;
            break;
        }
        case cDTJSONDoc:
        {
            if (val.get_data_type() != cDTJSONDoc)
                return false;
            break;
        }
        default:
            return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::fixup_manually_edited_params
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::fixup_manually_edited_params(gl_entrypoint_id_t gl_entrypoint_id, const char *pGL_func_name, uint64_t cur_call_counter)
{
    VOGL_FUNC_TRACER

    if ((gl_entrypoint_id == VOGL_ENTRYPOINT_glShaderSource) || (gl_entrypoint_id == VOGL_ENTRYPOINT_glShaderSourceARB))
    {
        //int count = get_param_data(1);
        GLsizei count = get_param_value<GLsizei>(1);
        if (count)
        {
            int32_t *pLengths = get_param_client_memory<int32_t>(3);
            if ((pLengths) && (get_param_client_memory_data_size(3) == count * sizeof(int32_t)))
            {
                for (GLsizei i = 0; i < count; i++)
                {
                    key_value_map::const_iterator it = m_key_value_map.find(i);
                    if (it == m_key_value_map.end())
                    {
                        vogl_error_printf("GL func %s call counter %" PRIu64 ": Failed finding shader source blob string in GL key value map\n", pGL_func_name, cur_call_counter);
                        return false;
                    }

                    const uint8_vec *pBlob = it->second.get_blob();
                    if (!pBlob)
                    {
                        vogl_error_printf("GL func %s call counter %" PRIu64 ": Failed finding shader source blob string in GL key value map\n", pGL_func_name, cur_call_counter);
                        return false;
                    }

                    uint32_t l = pLengths[i];

                    if (pBlob->size() != l)
                    {
                        vogl_warning_printf("GL func %s call counter %" PRIu64 ": Shader source code line %u: blob size is %u bytes, but length field is %u bytes: Fixing up length field to match blob's size\n",
                                           pGL_func_name, cur_call_counter, static_cast<uint32_t>(i), pBlob->size(), l);
                        pLengths[i] = pBlob->size();
                    }
                }
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::pretty_print_param
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::pretty_print(dynamic_string &str, bool type_info) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
    {
        str.empty();
        return false;
    }

    dynamic_string return_val;
    if (!pretty_print_return_value(return_val, type_info))
    {
        str.empty();
        return false;
    }

    str.format("%s %s(", return_val.get_ptr(), get_entrypoint_desc().m_pName);

    dynamic_string param_val;
    for (uint32_t i = 0; i < m_total_params; i++)
    {
        if (!pretty_print_param(param_val, i, type_info))
        {
            str.empty();
            return false;
        }

        str += param_val;

        if (i != (m_total_params - 1))
            str += ", ";
    }

    str += ")";

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::pretty_print_param
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::pretty_print_param(dynamic_string &str, uint32_t param_index, bool type_info) const
{
    VOGL_FUNC_TRACER

    str.empty();

    if (!m_is_valid)
        return false;

    const bool is_return_param = has_return_value() && (param_index == m_total_params);

    VOGL_ASSERT((param_index < m_total_params) || is_return_param);

    uint64_t val_data = m_param_data[param_index];
    //uint32_t val_size = m_param_size[param_index];

    const vogl_ctype_desc_t &ctype_desc = trace_ctypes()[m_param_ctype[param_index]];

    bool handled = false;

    const void *pClient_mem = NULL;
    uint32_t client_mem_size = 0;

    if (ctype_desc.m_is_pointer)
    {
        if (type_info)
            str.format("(%s) ", ctype_desc.m_pCType);

        if (m_client_memory_descs[param_index].m_vec_ofs >= 0)
        {
            pClient_mem = &m_client_memory[m_client_memory_descs[param_index].m_vec_ofs];
            client_mem_size = m_client_memory_descs[param_index].m_data_size;
        }

        if ((pClient_mem) && (client_mem_size))
        {
            bool print_as_cstring = false;
            switch (ctype_desc.m_ctype)
            {
                case VOGL_CONST_GLUBYTE_PTR: // TODO: This could false positive!
                case VOGL_CONST_GLCHAR_PTR:
                case VOGL_GLCHARARB_PTR:
                case VOGL_GLCHAR_PTR:
                case VOGL_CONST_GLCHARARB_PTR:
                case VOGL_LPCSTR:
                {
                    if ((client_mem_size <= 65536) && (utils::is_buffer_printable(pClient_mem, client_mem_size, true, true)))
                    {
                        print_as_cstring = true;
                        break;
                    }
                    break;
                }
                default:
                    break;
            }

            if (print_as_cstring)
            {
                const char *pStr = reinterpret_cast<const char *>(pClient_mem);
                uint32_t strlen = vogl_strlen(pStr);

                const uint32_t MAX_STR_LEN = 64;

                uint32_t chars_to_print = math::minimum<uint32_t>(MAX_STR_LEN, strlen);

                if (chars_to_print == strlen)
                    str.format_append("[\"%s\"]", pStr);
                else
                {
                    str.append("[");

                    for (uint32_t i = 0; i < chars_to_print; i++)
                        str.append_char(pStr[i]);

                    str.append("...]");
                }

                handled = true;
            }
        }
    }
    else
    {
        handled = pretty_print_param_val(str, ctype_desc, val_data, get_entrypoint_id(), is_return_param ? -1 : param_index);
    }

    if (!handled)
    {
        if (ctype_desc.m_is_pointer)
        {
            const vogl_ctype_t pointee_ctype = ctype_desc.m_pointee_ctype;
            const vogl_ctype_desc_t &pointee_ctype_desc = trace_ctypes()[pointee_ctype];
            const uint32_t pointee_size = (*m_pCTypes)[pointee_ctype].m_size;
            //const bool pointee_is_ptr = (*m_pCTypes)[pointee_ctype].m_is_pointer;

            if ((!val_data) && (!client_mem_size))
                str.format_append("NULL");
            else if ((pointee_ctype == VOGL_INVALID_CTYPE) && (!client_mem_size))
            {
                str.format_append("ptr=0x%" PRIX64, val_data);
            }
            else
            {
                dynamic_string shortened_ctype(pointee_ctype_desc.m_pName);
                if (shortened_ctype.begins_with("VOGL_"))
                    shortened_ctype.right(5);

                if ((pointee_size > 1) && ((client_mem_size % pointee_size) == 0))
                    str.format_append("ptr=0x%" PRIX64 " size=%u elements=%u pointee_type=%s", val_data, client_mem_size, client_mem_size / pointee_size, shortened_ctype.get_ptr());
                else
                    str.format_append("ptr=0x%" PRIX64 " size=%u pointee_type=%s", val_data, client_mem_size, shortened_ctype.get_ptr());

                uint32_t bytes_to_dump = math::minimum<uint32_t>(64, client_mem_size);
                if ((pClient_mem) && (bytes_to_dump))
                {
                    str.append(" [");

                    if ((pointee_size == sizeof(uint64_t)) && ((client_mem_size & 7) == 0))
                    {
                        for (uint32_t i = 0; i < bytes_to_dump / sizeof(uint64_t); i++)
                        {
                            if (i)
                                str.format_append(" ");

                            uint64_t array_val_data = reinterpret_cast<const uint64_t *>(pClient_mem)[i];
                            if (!pretty_print_param_val(str, pointee_ctype_desc, array_val_data, get_entrypoint_id(), is_return_param ? -1 : param_index))
                                str.format_append("0x%" PRIX64, array_val_data);
                        }
                    }
                    else if ((pointee_size == sizeof(uint32_t)) && ((client_mem_size & 3) == 0))
                    {
                        for (uint32_t i = 0; i < bytes_to_dump / sizeof(uint32_t); i++)
                        {
                            if (i)
                                str.format_append(" ");

                            uint64_t array_val_data = reinterpret_cast<const uint32_t *>(pClient_mem)[i];
                            if (!pretty_print_param_val(str, pointee_ctype_desc, array_val_data, get_entrypoint_id(), is_return_param ? -1 : param_index))
                                str.format_append("0x%" PRIX64, array_val_data);
                        }
                    }
                    else if ((pointee_size == sizeof(uint16_t)) && ((client_mem_size & 1) == 0))
                    {
                        for (uint32_t i = 0; i < bytes_to_dump / sizeof(uint16_t); i++)
                        {
                            if (i)
                                str.format_append(" ");

                            uint64_t array_val_data = reinterpret_cast<const uint16_t *>(pClient_mem)[i];

                            if (!pretty_print_param_val(str, pointee_ctype_desc, array_val_data, get_entrypoint_id(), is_return_param ? -1 : param_index))
                                str.format_append("0x%" PRIX64, array_val_data);
                        }
                    }
                    else
                    {
                        bytes_to_dump = math::minimum<uint32_t>(16, client_mem_size);

                        for (uint32_t i = 0; i < bytes_to_dump; i++)
                        {
                            if (i)
                                str.format_append(" ");

                            uint32_t array_val_data = reinterpret_cast<const uint8_t *>(pClient_mem)[i];

                            handled = false;
                            if (pointee_size == sizeof(uint8_t))
                                handled = pretty_print_param_val(str, pointee_ctype_desc, array_val_data, get_entrypoint_id(), is_return_param ? -1 : param_index);

                            if (!handled)
                                str.format_append("%02X", array_val_data);
                        }
                    }

                    if (bytes_to_dump < client_mem_size)
                        str.append("...");

                    str.append("]");
                }
            }
        }
        else
        {
            str.format_append("0x%" PRIX64, val_data);
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_packet::pretty_print_return_value
//----------------------------------------------------------------------------------------------------------------------
bool vogl_trace_packet::pretty_print_return_value(dynamic_string &str, bool type_info) const
{
    VOGL_FUNC_TRACER

    str.empty();

    if (!m_is_valid)
        return false;

    if (!has_return_value())
    {
        str = "void";
        return true;
    }

    pretty_print_param(str, m_total_params, type_info);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_does_packet_refer_to_program
// This conservatively determines if a trace packet refers to a GL program or ARB program object (there may be some
// false positives in here because ARB program and shader objects live in the same namespace).
//----------------------------------------------------------------------------------------------------------------------
bool vogl_does_packet_refer_to_program(const vogl_trace_packet &gl_packet, GLuint &program)
{
    VOGL_FUNC_TRACER

    program = 0;

    gl_entrypoint_id_t entrypoint_id = gl_packet.get_entrypoint_id();
    const gl_entrypoint_desc_t &entrypoint_desc = gl_packet.get_entrypoint_desc();

    if ((entrypoint_desc.m_return_namespace == VOGL_NAMESPACE_PROGRAMS) || (entrypoint_id == VOGL_ENTRYPOINT_glCreateProgramObjectARB))
    {
        program = gl_packet.get_return_value<GLuint>();
        return true;
    }

    for (uint32_t i = 0; i < entrypoint_desc.m_num_params; i++)
    {
        vogl_namespace_t param_namespace = g_vogl_entrypoint_param_descs[entrypoint_id][i].m_namespace;

        if (param_namespace == VOGL_NAMESPACE_PROGRAMS)
        {
            // TODO: Make sure this never can be a ptr to an array of handles
            VOGL_VERIFY(!gl_packet.get_param_ctype_desc(i).m_is_pointer);
            program = gl_packet.get_param_value<GLuint>(i);
            return true;
        }

        // Special case the GL_ARB_shader_objects/GL_ARB_vertex_shader GLhandleARB, which alias programs.
        // We want to reject handles that are clearly shader objects, and accept everything else.
        if ((!i) && (param_namespace == VOGL_NAMESPACE_GLHANDLEARB))
        {
            bool accept = true;

            switch (entrypoint_id)
            {
                case VOGL_ENTRYPOINT_glShaderSourceARB:
                case VOGL_ENTRYPOINT_glCompileShaderARB:
                {
                    accept = false;
                    break;
                }
                default:
                    break;
            }

            if (accept)
            {
                // TODO: Make sure this never can be a ptr to an array of handles
                VOGL_VERIFY(!gl_packet.get_param_ctype_desc(i).m_is_pointer);
                program = gl_packet.get_param_value<GLuint>(i);
                return true;
            }
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_write_glInternalTraceCommandRAD
//----------------------------------------------------------------------------------------------------------------------
bool vogl_write_glInternalTraceCommandRAD(data_stream &stream, const vogl_ctypes *pCTypes, GLuint cmd, GLuint size, const GLubyte *data)
{
    VOGL_FUNC_TRACER

    vogl_trace_packet packet(pCTypes);

    packet.begin_construction(VOGL_ENTRYPOINT_glInternalTraceCommandRAD, 0, 0, 0, utils::RDTSC());

    uint64_t cur_rdtsc = utils::RDTSC();
    packet.set_gl_begin_rdtsc(cur_rdtsc);
    packet.set_gl_end_rdtsc(cur_rdtsc + 1);

    packet.set_param(0, VOGL_GLUINT, &cmd, sizeof(cmd));
    packet.set_param(1, VOGL_GLUINT, &size, sizeof(size));

    vogl_trace_ptr_value ptr_val = reinterpret_cast<vogl_trace_ptr_value>(data);
    packet.set_param(2, VOGL_CONST_GLUBYTE_PTR, &ptr_val, packet.get_ctypes()->get_pointer_size());

    switch (cmd)
    {
        case cITCRDemarcation:
        {
            break;
        }
        case cITCRKeyValueMap:
        {
            if ((size == sizeof(key_value_map)) && (data))
            {
                // Directly jam in the key value map, so it properly serializes as readable JSON.
                packet.get_key_value_map() = *reinterpret_cast<const key_value_map *>(data);
            }
            else
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }
            break;
        }
        default:
        {
            vogl_error_printf("Unknown trace command type %u\n", cmd);
            VOGL_ASSERT_ALWAYS;
            return false;
        }
    }

    packet.end_construction(utils::RDTSC());

    return packet.serialize(stream);
}
