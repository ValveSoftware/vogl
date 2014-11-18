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

// File: vogl_replay_tool.cpp
#include "vogl_common.h"
#include "vogl_gl_replayer.h"
#include "vogl_mergesort.h"
#include "vogl_unique_ptr.h"

#define vogl_progress_printf(...) vogl::console::printf(VOGL_FUNCTION_INFO_CSTR, cMsgPrint | cMsgFlagNoLog, __VA_ARGS__)

static command_line_param_desc g_command_line_param_descs_info[] =
{
    // info specific
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
};

//----------------------------------------------------------------------------------------------------------------------
// struct histo_entry
//----------------------------------------------------------------------------------------------------------------------
typedef vogl::map<dynamic_string> dynamic_string_set;
typedef vogl::map<dynamic_string, uint64_t> dynamic_string_hash_map;
typedef vogl::map<uint32_t> uint_map;

struct histo_entry
{
    histo_entry()
    {
    }
    histo_entry(dynamic_string_hash_map::const_iterator it, double val)
        : m_it(it), m_value(val)
    {
    }

    dynamic_string_hash_map::const_iterator m_it;
    double m_value;

    bool operator<(const histo_entry &rhs) const
    {
        return m_value > rhs.m_value;
    }
};

//----------------------------------------------------------------------------------------------------------------------
// dump_histogram
//----------------------------------------------------------------------------------------------------------------------
#define SAFE_FLOAT_DIV(n, d) (d ? ((double)(n) / (d)) : 0)

static void dump_histogram(const char *pMsg, const dynamic_string_hash_map &map, uint64_t total_gl_entrypoint_packets, uint64_t total_swaps)
{
    VOGL_FUNC_TRACER

    vogl::vector<histo_entry> histo;
    for (dynamic_string_hash_map::const_iterator it = map.begin(); it != map.end(); ++it)
        histo.push_back(histo_entry(it, static_cast<double>(it->second)));
    mergesort(histo);
    vogl_printf("\n----------------------\n%s: %u\n", pMsg, map.size());

    for (uint32_t i = 0; i < histo.size(); i++)
    {
        dynamic_string_hash_map::const_iterator it = histo[i].m_it;
        vogl_printf("%s: Total calls: %" PRIu64 " %3.1f%%, Avg calls per frame: %f\n", it->first.get_ptr(), it->second, SAFE_FLOAT_DIV(it->second * 100.0f, total_gl_entrypoint_packets), SAFE_FLOAT_DIV(it->second, total_swaps));
    }
}
//----------------------------------------------------------------------------------------------------------------------
// tool_info_mode
//----------------------------------------------------------------------------------------------------------------------
bool tool_info_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    if (desc)
    {
        desc->append(g_command_line_param_descs_info, VOGL_ARRAY_SIZE(g_command_line_param_descs_info));
        return true;
    }

    dynamic_string input_base_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_base_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input JSON/blob trace files!\n");
        return false;
    }

    dynamic_string actual_input_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(
                vogl_open_trace_file(input_base_filename, actual_input_filename,
                                     g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
        return false;

    vogl_printf("Scanning trace file %s\n", actual_input_filename.get_ptr());

    const vogl_trace_stream_start_of_file_packet &sof_packet = pTrace_reader->get_sof_packet();

    if (pTrace_reader->get_type() == cBINARY_TRACE_FILE_READER)
    {
        uint64_t file_size = dynamic_cast<vogl_binary_trace_file_reader *>(pTrace_reader.get())->get_stream().get_size();
        vogl_printf("Total file size: %s\n", uint64_to_string_with_commas(file_size).get_ptr());
    }

    vogl_printf("SOF packet size: %" PRIu64 " bytes\n", sof_packet.m_size);
    vogl_printf("Version: 0x%04X\n", sof_packet.m_version);
    vogl_printf("UUID: 0x%08x 0x%08x 0x%08x 0x%08x\n", sof_packet.m_uuid[0], sof_packet.m_uuid[1], sof_packet.m_uuid[2], sof_packet.m_uuid[3]);
    vogl_printf("First packet offset: %" PRIu64 "\n", sof_packet.m_first_packet_offset);
    vogl_printf("Trace pointer size: %u\n", sof_packet.m_pointer_sizes);
    vogl_printf("Trace archive size: %" PRIu64 " offset: %" PRIu64 "\n", sof_packet.m_archive_size, sof_packet.m_archive_offset);
    vogl_printf("Can quickly seek forward: %u\nMax frame index: %" PRIu64 "\n", pTrace_reader->can_quickly_seek_forward(), pTrace_reader->get_max_frame_index());

    if (!pTrace_reader->get_archive_blob_manager().is_initialized())
    {
        vogl_warning_printf("This trace does not have a trace archive!\n");
    }
    else
    {
        vogl_printf("----------------------\n");
        vogl::vector<dynamic_string> archive_files(pTrace_reader->get_archive_blob_manager().enumerate());
        vogl_printf("Total trace archive files: %u\n", archive_files.size());
        for (uint32_t i = 0; i < archive_files.size(); i++)
            vogl_printf("\"%s\"\n", archive_files[i].get_ptr());
        vogl_printf("----------------------\n");
    }

    uint32_t min_packet_size = cUINT32_MAX;
    uint32_t max_packet_size = 0;
    uint64_t total_packets = 1; // 1, not 0, to account for the SOF packet
    uint64_t total_packet_bytes = 0;
    uint64_t total_swaps = 0;
    uint64_t total_make_currents = 0;
    uint64_t total_internal_trace_commands = 0;
    uint64_t total_non_gl_entrypoint_packets = 1; // 1, not 0, to account for the SOF packet
    uint64_t total_gl_entrypoint_packets = 0;
    uint64_t total_gl_commands = 0;
    uint64_t total_glx_commands = 0;
    uint64_t total_cgl_commands = 0;
    uint64_t total_wgl_commands = 0;
    uint64_t total_unknown_commands = 0;
    uint64_t total_draws = 0;
    bool found_eof_packet = false;

    uint64_t min_packets_per_frame = cUINT64_MAX;
    uint64_t max_packets_per_frame = 0;
    uint64_t max_frame_make_currents = 0;
    uint64_t min_frame_make_currents = cUINT64_MAX;

    uint64_t min_frame_draws = cUINT64_MAX;
    uint64_t max_frame_draws = 0;
    uint64_t cur_frame_draws = 0;

    uint64_t cur_frame_packet_count = 0;
    uint64_t total_frame_make_currents = 0;

    uint64_t num_non_whitelisted_funcs = 0;
    dynamic_string_set non_whitelisted_funcs_called;

    uint64_t total_programs_linked = 0;
    uint64_t total_program_binary_calls = 0;
    uint_map unique_programs_used;
    uint_map unique_program_pipelines_used;

    uint32_t total_gl_state_snapshots = 0;

    uint32_t total_display_list_calls = false;
    uint32_t total_gl_get_errors = 0;

    uint32_t total_context_creates = 0;
    uint32_t total_context_destroys = 0;

    dynamic_string_hash_map all_apis_called, category_histogram, version_histogram, profile_histogram, deprecated_histogram;

    vogl_ctypes trace_gl_ctypes(pTrace_reader->get_sof_packet().m_pointer_sizes);

    vogl_trace_packet trace_packet(&trace_gl_ctypes);

    for (;;)
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogl_error_printf("Failed reading from trace file!\n");
            goto done;
        }

        if (read_status == vogl_trace_file_reader::cEOF)
        {
            vogl_printf("At trace file EOF on swap %" PRIu64 "\n", total_swaps);
            break;
        }

        const vogl::vector<uint8_t> &packet_buf = pTrace_reader->get_packet_buf();
        VOGL_NOTE_UNUSED(packet_buf);

        uint32_t packet_size = pTrace_reader->get_packet_size();

        min_packet_size = math::minimum<uint32_t>(min_packet_size, packet_size);
        max_packet_size = math::maximum<uint32_t>(max_packet_size, packet_size);
        total_packets++;
        total_packet_bytes += packet_size;

        cur_frame_packet_count++;

        const vogl_trace_stream_packet_base &base_packet = pTrace_reader->get_base_packet();
        VOGL_NOTE_UNUSED(base_packet);
        const vogl_trace_gl_entrypoint_packet *pGL_packet = NULL;

        if (pTrace_reader->get_packet_type() == cTSPTGLEntrypoint)
        {
            if (!trace_packet.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
            {
                vogl_error_printf("Failed parsing GL entrypoint packet\n");
                goto done;
            }

            pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
            gl_entrypoint_id_t entrypoint_id = static_cast<gl_entrypoint_id_t>(pGL_packet->m_entrypoint_id);

            const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_id];

            all_apis_called[entrypoint_desc.m_pName]++;
            category_histogram[entrypoint_desc.m_pCategory]++;
            version_histogram[entrypoint_desc.m_pVersion]++;
            profile_histogram[entrypoint_desc.m_pProfile]++;
            deprecated_histogram[entrypoint_desc.m_pDeprecated]++;

            if (!entrypoint_desc.m_is_whitelisted)
            {
                num_non_whitelisted_funcs++;
                non_whitelisted_funcs_called.insert(entrypoint_desc.m_pName);
            }

            if (!strcmp(entrypoint_desc.m_pAPI_prefix, "GLX"))
                total_glx_commands++;
            else if (!strcmp(entrypoint_desc.m_pAPI_prefix, "CGL"))
                total_cgl_commands++;
            else if (!strcmp(entrypoint_desc.m_pAPI_prefix, "WGL"))
                total_wgl_commands++;
            else if (!strcmp(entrypoint_desc.m_pAPI_prefix, "GL"))
                total_gl_commands++;
            else
                total_unknown_commands++;

            total_gl_entrypoint_packets++;

            if (vogl_is_swap_buffers_entrypoint(entrypoint_id))
            {
                total_swaps++;
                if ((total_swaps & 255) == 255)
                    vogl_progress_printf("Frame %" PRIu64 "\n", total_swaps);

                min_packets_per_frame = math::minimum(min_packets_per_frame, cur_frame_packet_count);
                max_packets_per_frame = math::maximum(max_packets_per_frame, cur_frame_packet_count);

                min_frame_draws = math::minimum(min_frame_draws, cur_frame_draws);
                max_frame_draws = math::maximum(max_frame_draws, cur_frame_draws);

                max_frame_make_currents = math::maximum(max_frame_make_currents, total_frame_make_currents);
                min_frame_make_currents = math::minimum(min_frame_make_currents, total_frame_make_currents);

                cur_frame_packet_count = 0;
                total_frame_make_currents = 0;
                cur_frame_draws = 0;
            }
            else if (vogl_is_draw_entrypoint(entrypoint_id))
            {
                total_draws++;
                cur_frame_draws++;
            }
            else if (vogl_is_make_current_entrypoint(entrypoint_id))
            {
                total_make_currents++;
                total_frame_make_currents++;
            }

            switch (entrypoint_id)
            {
                case VOGL_ENTRYPOINT_glInternalTraceCommandRAD:
                {
                    total_internal_trace_commands++;

                    GLuint cmd = trace_packet.get_param_value<GLuint>(0);
                    GLuint size = trace_packet.get_param_value<GLuint>(1);
                    VOGL_NOTE_UNUSED(size);

                    if (cmd == cITCRKeyValueMap)
                    {
                        key_value_map &kvm = trace_packet.get_key_value_map();

                        dynamic_string cmd_type(kvm.get_string("command_type"));
                        if (cmd_type == "state_snapshot")
                        {
                            total_gl_state_snapshots++;
                        }
                    }

                    break;
                }
                case VOGL_ENTRYPOINT_glProgramBinary:
                {
                    total_program_binary_calls++;
                    break;
                }
                case VOGL_ENTRYPOINT_glLinkProgram:
                case VOGL_ENTRYPOINT_glLinkProgramARB:
                {
                    total_programs_linked++;
                    break;
                }
                case VOGL_ENTRYPOINT_glUseProgram:
                case VOGL_ENTRYPOINT_glUseProgramObjectARB:
                {
                    GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                    unique_programs_used.insert(trace_handle);

                    break;
                }
                case VOGL_ENTRYPOINT_glUseProgramStages:
                {
                    GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                    unique_program_pipelines_used.insert(trace_handle);

                    break;
                }
                case VOGL_ENTRYPOINT_glGenLists:
                case VOGL_ENTRYPOINT_glDeleteLists:
                case VOGL_ENTRYPOINT_glXUseXFont:
                case VOGL_ENTRYPOINT_glCallList:
                case VOGL_ENTRYPOINT_glCallLists:
                case VOGL_ENTRYPOINT_glListBase:
                {
                    total_display_list_calls++;
                    break;
                }
                case VOGL_ENTRYPOINT_glGetError:
                {
                    total_gl_get_errors++;
                    break;
                }
                case VOGL_ENTRYPOINT_glXCreateContext:
                case VOGL_ENTRYPOINT_glXCreateContextAttribsARB:
                {
                    total_context_creates++;
                    break;
                }
                case VOGL_ENTRYPOINT_glXDestroyContext:
                {
                    total_context_destroys++;
                    break;
                }
                default:
                    break;
            }
        }
        else
        {
            total_non_gl_entrypoint_packets++;
        }

        if (pTrace_reader->get_packet_type() == cTSPTEOF)
        {
            found_eof_packet = true;
            vogl_printf("Found trace file EOF packet on swap %" PRIu64 "\n", total_swaps);
            break;
        }
    }

done:
    vogl_printf("\n");

#define PRINT_UINT_VAR(x) vogl_printf("%s: %u\n", dynamic_string(#x).replace("_", " ").get_ptr(), x);
#define PRINT_UINT64_VAR(x) vogl_printf("%s: %" PRIu64 "\n", dynamic_string(#x).replace("_", " ").get_ptr(), x);
#define PRINT_FLOAT(x, f) vogl_printf("%s: %f\n", dynamic_string(#x).replace("_", " ").get_ptr(), f);

    PRINT_UINT64_VAR(num_non_whitelisted_funcs);
    PRINT_UINT_VAR(total_gl_state_snapshots);

    PRINT_UINT64_VAR(total_swaps);

    PRINT_UINT64_VAR(total_make_currents);
    PRINT_FLOAT(avg_make_currents_per_frame, SAFE_FLOAT_DIV(total_make_currents, total_swaps));
    PRINT_UINT64_VAR(max_frame_make_currents);
    PRINT_UINT64_VAR(min_frame_make_currents);

    PRINT_UINT64_VAR(total_draws);
    PRINT_FLOAT(avg_draws_per_frame, SAFE_FLOAT_DIV(total_draws, total_swaps));
    PRINT_UINT64_VAR(min_frame_draws);
    PRINT_UINT64_VAR(max_frame_draws);

    PRINT_UINT_VAR(min_packet_size);
    PRINT_UINT_VAR(max_packet_size);
    PRINT_UINT64_VAR(total_packets);
    PRINT_UINT64_VAR(total_packet_bytes);
    PRINT_FLOAT(avg_packet_bytes_per_frame, SAFE_FLOAT_DIV(total_packet_bytes, total_swaps));
    PRINT_FLOAT(avg_packet_size, SAFE_FLOAT_DIV(total_packet_bytes, total_packets));
    PRINT_UINT64_VAR(min_packets_per_frame);
    PRINT_UINT64_VAR(max_packets_per_frame);
    PRINT_FLOAT(avg_packets_per_frame, SAFE_FLOAT_DIV(total_packets, total_swaps));

    PRINT_UINT64_VAR(total_internal_trace_commands);
    PRINT_UINT64_VAR(total_non_gl_entrypoint_packets);
    PRINT_UINT64_VAR(total_gl_entrypoint_packets);
    PRINT_UINT64_VAR(total_gl_commands);
    PRINT_UINT64_VAR(total_glx_commands);
    PRINT_UINT64_VAR(total_cgl_commands);
    PRINT_UINT64_VAR(total_wgl_commands);
    PRINT_UINT64_VAR(total_unknown_commands);

    PRINT_UINT_VAR(found_eof_packet);

    PRINT_UINT_VAR(total_display_list_calls);
    printf("Avg display lists calls per frame: %f\n", SAFE_FLOAT_DIV(total_display_list_calls, total_swaps));

    PRINT_UINT_VAR(total_gl_get_errors);
    printf("Avg glGetError calls per frame: %f\n", SAFE_FLOAT_DIV(total_gl_get_errors, total_swaps));

    PRINT_UINT_VAR(total_context_creates);
    PRINT_UINT_VAR(total_context_destroys);

#undef PRINT_UINT
#undef PRINT_UINT64_VAR
#undef PRINT_FLOAT

    vogl_printf("----------------------\n%s: %" PRIu64 "\n", "Total calls to glLinkProgram/glLinkProgramARB", total_programs_linked);
    vogl_printf("%s: %" PRIu64 "\n", "Total calls to glProgramBinary", total_program_binary_calls);
    vogl_printf("%s: %u\n", "Total unique program handles passed to glUseProgram/glUseProgramObjectARB", unique_programs_used.size());
    vogl_printf("%s: %u\n", "Total unique program pipeline handles passed to glUseProgramStages", unique_program_pipelines_used.size());

    dump_histogram("API histogram", all_apis_called, total_gl_entrypoint_packets, total_swaps);
    dump_histogram("API Category histogram", category_histogram, total_gl_entrypoint_packets, total_swaps);
    dump_histogram("API Version histogram", version_histogram, total_gl_entrypoint_packets, total_swaps);
//dump_histogram("API Profile histogram", profile_histogram, total_gl_entrypoint_packets, total_swaps);
//dump_histogram("API Deprecated histogram", deprecated_histogram, total_gl_entrypoint_packets, total_swaps);

#undef DUMP_HISTOGRAM

    if (non_whitelisted_funcs_called.size())
    {
        vogl_warning_printf("\n----------------------\n");
        vogl_warning_printf("Number of non-whitelisted functions actually called: %u\n", non_whitelisted_funcs_called.size());
        for (dynamic_string_set::const_iterator it = non_whitelisted_funcs_called.begin(); it != non_whitelisted_funcs_called.end(); ++it)
            vogl_warning_printf("%s\n", it->first.get_ptr());
        vogl_warning_printf("\n----------------------\n");
    }

    return true;
}
#undef SAFE_FLOAT_DIV

