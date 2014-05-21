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

// File: replay_tool_find.cpp
#include "vogl_common.h"
#include "vogl_gl_replayer.h"
#include "vogl_bigint128.h"
#include "vogl_regex.h"

static command_line_param_desc g_command_line_param_descs_find[] =
{
    // find specific
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
    { "find_func", 1, false, "Find: Limit the find to only the specified function name POSIX regex pattern" },
    { "find_param", 1, false, "Find: The parameter value to find, hex, decimal integers, or GL enum strings OK" },
    { "find_namespace", 1, false, "Find: Limits --find_param to only parameters using the specified handle namespace: invalid, GLhandleARB, GLframebuffer, GLtexture, GLrenderbuffer, GLquery, GLsampler, GLprogramARB, GLprogram, GLarray, GLlist, GLlocation, GLlocationARB, GLfence, GLsync, GLpipeline, GLshader, GLbuffer, GLfeedback, GLarrayAPPLE, GLfragmentShaderATI" },
    { "find_param_name", 1, false, "Find: Limits the find to only params with the specified name (specify \"return\" to limit search to only return values)" },
    { "find_frame_low", 1, false, "Find: Limit the find to frames beginning at the specified frame index" },
    { "find_frame_high", 1, false, "Find: Limit the find to frames up to and including the specified frame index" },
    { "find_call_low", 1, false, "Find: Limit the find to GL calls beginning at the specified call index" },
    { "find_call_high", 1, false, "Find: Limit the find to GL calls up to and including the specified call index" },
};

//----------------------------------------------------------------------------------------------------------------------
// param_value_matches
//----------------------------------------------------------------------------------------------------------------------
static bool param_value_matches(const bigint128 &value_to_find, vogl_namespace_t find_namespace, uint64_t value_data, vogl_ctype_t value_ctype, vogl_namespace_t value_namespace)
{
    if (find_namespace != VOGL_NAMESPACE_UNKNOWN)
    {
        if (find_namespace != value_namespace)
            return false;
    }

    bigint128 value(0);

    switch (value_ctype)
    {
        case VOGL_BOOL:
        case VOGL_GLBOOLEAN:
        {
            value = value_data;
            break;
        }
        case VOGL_GLENUM:
        {
            value = value_data;
            break;
        }
        case VOGL_FLOAT:
        case VOGL_GLFLOAT:
        case VOGL_GLCLAMPF:
        {
            // Not supporting float/double finds for now
            return false;
        }
        case VOGL_GLDOUBLE:
        case VOGL_GLCLAMPD:
        {
            // Not supporting float/double finds for now
            return false;
        }
        case VOGL_GLINT:
        case VOGL_INT:
        case VOGL_INT32T:
        case VOGL_GLSIZEI:
        case VOGL_GLFIXED:
        {
            value = static_cast<int32_t>(value_data);
            break;
        }
        case VOGL_GLSHORT:
        {
            value = static_cast<int16_t>(value_data);
            break;
        }
        case VOGL_GLBYTE:
        {
            value = static_cast<int8_t>(value_data);
            break;
        }
        case VOGL_GLINT64:
        case VOGL_GLINT64EXT:
        {
            value = static_cast<int64_t>(value_data);
            break;
        }
        default:
        {
            value = value_data;
            break;
        }
    }

    if (value == value_to_find)
        return true;

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// print_match
//----------------------------------------------------------------------------------------------------------------------
static void print_match(const vogl_trace_packet &trace_packet, int param_index, int array_element_index, uint64_t total_swaps)
{
    json_document doc;
    vogl_trace_packet::json_serialize_params params;
    trace_packet.json_serialize(*doc.get_root(), params);

    dynamic_string packet_as_json;
    doc.serialize(packet_as_json);

    if (param_index == -2)
        vogl_printf("----- Function match, frame %" PRIu64 ":\n", total_swaps);
    else if (param_index < 0)
        vogl_printf("----- Return value match, frame %" PRIu64 ":\n", total_swaps);
    else if (array_element_index >= 0)
        vogl_printf("----- Parameter %i element %i match, frame %" PRIu64 ":\n", param_index, array_element_index, total_swaps);
    else
        vogl_printf("----- Parameter %i match, frame %" PRIu64 ":\n", param_index, total_swaps);

    vogl_printf("%s\n", packet_as_json.get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// tool_find_mode
//----------------------------------------------------------------------------------------------------------------------
bool tool_find_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    if (desc)
    {
        desc->append(g_command_line_param_descs_find, VOGL_ARRAY_SIZE(g_command_line_param_descs_find));
        return true;
    }

    dynamic_string input_base_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_base_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input JSON/blob trace files!\n");
        return false;
    }

    dynamic_string actual_input_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(input_base_filename,
        actual_input_filename,
        g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
        return false;

    bigint128 value_to_find(static_cast<uint64_t>(gl_enums::cUnknownEnum));
    bool has_find_param = g_command_line_params().has_key("find_param");
    if (has_find_param)
    {
        dynamic_string find_value_str(g_command_line_params().get_value_as_string("find_param"));
        if ((find_value_str.has_content()) && (vogl_isalpha(find_value_str[0])))
        {
            value_to_find = get_gl_enums().find_enum(find_value_str);
        }

        if (value_to_find == static_cast<uint64_t>(gl_enums::cUnknownEnum))
        {
            bool find_param_u64_valid = false;
            value_to_find = g_command_line_params().get_value_as_uint64("find_param", 0, 0, 0, cUINT64_MAX, 0, &find_param_u64_valid);

            if (!find_param_u64_valid)
            {
                bool find_param_i64_valid = false;
                value_to_find = g_command_line_params().get_value_as_int64("find_param", 0, 0, 0, cINT64_MAX, 0, &find_param_i64_valid);

                if (!find_param_i64_valid)
                {
                    vogl_error_printf("Failed parsing \"-find_param X\" command line option!\n");
                    return false;
                }
            }
        }
    }

    dynamic_string find_namespace_str(g_command_line_params().get_value_as_string_or_empty("find_namespace"));
    vogl_namespace_t find_namespace = VOGL_NAMESPACE_UNKNOWN;
    if (find_namespace_str.has_content())
    {
        find_namespace = vogl_find_namespace_from_gl_type(find_namespace_str.get_ptr());
        if ((find_namespace == VOGL_NAMESPACE_INVALID) && (find_namespace_str != "invalid"))
        {
            vogl_error_printf("Invalid namespace: \"%s\"\n", find_namespace_str.get_ptr());
            return false;
        }
    }

    dynamic_string find_param_name(g_command_line_params().get_value_as_string_or_empty("find_param_name"));

    dynamic_string find_func_pattern(g_command_line_params().get_value_as_string_or_empty("find_func"));
    regexp func_regex;
    if (find_func_pattern.has_content())
    {
        if (!func_regex.init(find_func_pattern.get_ptr(), REGEX_IGNORE_CASE))
        {
            vogl_error_printf("Invalid func regex: \"%s\"\n", find_func_pattern.get_ptr());
            return false;
        }
    }

    int64_t find_frame_low = g_command_line_params().get_value_as_int64("find_frame_low", 0, -1);
    int64_t find_frame_high = g_command_line_params().get_value_as_int64("find_frame_high", 0, -1);
    int64_t find_call_low = g_command_line_params().get_value_as_int64("find_call_low", 0, -1);
    int64_t find_call_high = g_command_line_params().get_value_as_int64("find_call_high", 0, -1);

    vogl_printf("Scanning trace file %s\n", actual_input_filename.get_ptr());

    vogl_ctypes trace_gl_ctypes(pTrace_reader->get_sof_packet().m_pointer_sizes);
    vogl_trace_packet trace_packet(&trace_gl_ctypes);

    uint64_t total_matches = 0;
    uint64_t total_swaps = 0;

    for (;;)
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogl_error_printf("Failed reading from trace file!\n");
            goto done;
        }

        if (read_status == vogl_trace_file_reader::cEOF)
            break;

        const vogl::vector<uint8_t> &packet_buf = pTrace_reader->get_packet_buf();
        VOGL_NOTE_UNUSED(packet_buf);

        const vogl_trace_stream_packet_base &base_packet = pTrace_reader->get_base_packet();
        VOGL_NOTE_UNUSED(base_packet);

        const vogl_trace_gl_entrypoint_packet *pGL_packet = NULL;
        VOGL_NOTE_UNUSED(pGL_packet);

        if (pTrace_reader->get_packet_type() == cTSPTEOF)
            break;
        else if (pTrace_reader->get_packet_type() != cTSPTGLEntrypoint)
            continue;

        if (!trace_packet.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
        {
            vogl_error_printf("Failed parsing GL entrypoint packet\n");
            goto done;
        }

        pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();

        if (find_frame_low >= 0)
        {
            if (total_swaps < static_cast<uint64_t>(find_frame_low))
                goto skip;
        }
        if (find_frame_high >= 0)
        {
            if (total_swaps > static_cast<uint64_t>(find_frame_high))
                break;
        }

        if (find_call_low >= 0)
        {
            if (trace_packet.get_call_counter() < static_cast<uint64_t>(find_call_low))
                goto skip;
        }
        if (find_call_high >= 0)
        {
            if (trace_packet.get_call_counter() > static_cast<uint64_t>(find_call_high))
                break;
        }

        if (func_regex.is_initialized())
        {
            if (!func_regex.full_match(trace_packet.get_entrypoint_desc().m_pName))
                goto skip;
        }

        if (!has_find_param)
        {
            print_match(trace_packet, -2, -1, total_swaps);
            total_matches++;
        }
        else
        {
            if (trace_packet.has_return_value())
            {
                if ((find_param_name.is_empty()) || (find_param_name == "return"))
                {
                    if (param_value_matches(value_to_find, find_namespace, trace_packet.get_return_value_data(), trace_packet.get_return_value_ctype(), trace_packet.get_return_value_namespace()))
                    {
                        print_match(trace_packet, -1, -1, total_swaps);
                        total_matches++;
                    }
                }
            }

            for (uint32_t i = 0; i < trace_packet.total_params(); i++)
            {
                if ((find_param_name.has_content()) && (find_param_name != trace_packet.get_param_desc(i).m_pName))
                    continue;

                const vogl_ctype_desc_t &param_ctype_desc = trace_packet.get_param_ctype_desc(i);

                if (param_ctype_desc.m_is_pointer)
                {
                    if ((!param_ctype_desc.m_is_opaque_pointer) && (param_ctype_desc.m_pointee_ctype != VOGL_VOID) && (trace_packet.has_param_client_memory(i)))
                    {
                        const vogl_client_memory_array array(trace_packet.get_param_client_memory_array(i));

                        for (uint32_t j = 0; j < array.size(); j++)
                        {
                            if (param_value_matches(value_to_find, find_namespace, array.get_element<uint64_t>(j), array.get_element_ctype(), trace_packet.get_param_namespace(i)))
                            {
                                print_match(trace_packet, i, j, total_swaps);
                                total_matches++;
                            }
                        }
                    }
                }
                else if (param_value_matches(value_to_find, find_namespace, trace_packet.get_param_data(i), trace_packet.get_param_ctype(i), trace_packet.get_param_namespace(i)))
                {
                    print_match(trace_packet, i, -1, total_swaps);
                    total_matches++;
                }
            }
        }

    skip:
        if (vogl_is_swap_buffers_entrypoint(trace_packet.get_entrypoint_id()))
            total_swaps++;
    }

done:
    vogl_printf("Total matches found: %" PRIu64 "\n", total_matches);

    return true;
}
