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

// File: replay_tool_dump.cpp
#include "vogl_common.h"
#include "vogl_gl_replayer.h"

#include "vogl_colorized_console.h"
#include "vogl_file_utils.h"

#ifdef VOGL_REMOTING
#include "vogl_remote.h"
#endif // VOGL_REMOTING

#include "libtelemetry.h"

static command_line_param_desc g_command_line_param_descs_dump[] =
{
    // dump specific
    { "verify", 0, false, "Dump: Fully round-trip verify all JSON objects vs. the original packet's" },
    { "write_debug_info", 0, false, "Dump: Write extra debug info to output JSON trace files" },
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
    { "debug", 0, false, "Enable verbose debug information" },
};

//----------------------------------------------------------------------------------------------------------------------
// tool_dump_mode
//----------------------------------------------------------------------------------------------------------------------
bool tool_dump_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    if (desc)
    {
        desc->append(g_command_line_param_descs_dump, VOGL_ARRAY_SIZE(g_command_line_param_descs_dump));
        return true;
    }

    dynamic_string input_trace_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_trace_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input binary trace file!\n");
        return false;
    }

    dynamic_string output_base_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_base_filename.is_empty())
    {
        vogl_error_printf("Must specify base filename of output JSON/blob files!\n");
        return false;
    }

    vogl_loose_file_blob_manager output_file_blob_manager;

    dynamic_string output_trace_path(file_utils::get_pathname(output_base_filename.get_ptr()));
    vogl_debug_printf("Output trace path: %s\n", output_trace_path.get_ptr());
    output_file_blob_manager.init(cBMFReadWrite, output_trace_path.get_ptr());

    file_utils::create_directories(output_trace_path, false);

    dynamic_string actual_input_trace_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(
            vogl_open_trace_file(input_trace_filename, actual_input_trace_filename,
            g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
    {
        vogl_error_printf("Failed opening input trace file \"%s\"\n", input_trace_filename.get_ptr());
        return false;
    }

    const bool full_verification = g_command_line_params().get_value_as_bool("verify");

    vogl_ctypes trace_gl_ctypes;
    trace_gl_ctypes.init(pTrace_reader->get_sof_packet().m_pointer_sizes);

    dynamic_string archive_name;
    if (pTrace_reader->get_archive_blob_manager().is_initialized())
    {
        dynamic_string archive_filename(output_base_filename.get_ptr());
        archive_filename += "_trace_archive.zip";

        archive_name = file_utils::get_filename(archive_filename.get_ptr());

        vogl_message_printf("Writing trace archive \"%s\", size %" PRIu64 " bytes\n", archive_filename.get_ptr(), pTrace_reader->get_archive_blob_manager().get_archive_size());

        cfile_stream archive_stream;
        if (!archive_stream.open(archive_filename.get_ptr(), cDataStreamWritable | cDataStreamSeekable))
        {
            vogl_error_printf("Failed opening output trace archive \"%s\"!\n", archive_filename.get_ptr());
            return false;
        }

        if (!pTrace_reader->get_archive_blob_manager().write_archive_to_stream(archive_stream))
        {
            vogl_error_printf("Failed writing to output trace archive \"%s\"!\n", archive_filename.get_ptr());
            return false;
        }

        if (!archive_stream.close())
        {
            vogl_error_printf("Failed writing to output trace archive \"%s\"!\n", archive_filename.get_ptr());
            return false;
        }
    }

    vogl_trace_packet gl_packet_cracker(&trace_gl_ctypes);

    uint32_t cur_file_index = 0;
    uint32_t cur_frame_index = 0;
    uint64_t cur_packet_index = 0;
    VOGL_NOTE_UNUSED(cur_packet_index);
    json_document cur_doc;

    {
        json_node &meta_node = cur_doc.get_root()->add_object("meta");
        meta_node.add_key_value("cur_frame", cur_frame_index);

        json_node &sof_node = cur_doc.get_root()->add_object("sof");
        sof_node.add_key_value("pointer_sizes", pTrace_reader->get_sof_packet().m_pointer_sizes);
        sof_node.add_key_value("version", to_hex_string(pTrace_reader->get_sof_packet().m_version));
        if (!archive_name.is_empty())
            sof_node.add_key_value("archive_filename", archive_name);

        json_node &uuid_array = sof_node.add_array("uuid");
        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(pTrace_reader->get_sof_packet().m_uuid); i++)
            uuid_array.add_value(pTrace_reader->get_sof_packet().m_uuid[i]);
    }

    // TODO: Automatically dump binary snapshot file to text?
    // Right now we can't afford to do it at trace time, it takes too much memory.

    json_node *pPacket_array = &cur_doc.get_root()->add_array("packets");

    bool flush_current_document = false;

    bool status = true;

    for (;;)
    {
        uint64_t cur_packet_ofs = (pTrace_reader->get_type() == cBINARY_TRACE_FILE_READER) ? static_cast<vogl_binary_trace_file_reader *>(pTrace_reader.get())->get_cur_file_ofs() : 0;

        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if (read_status == vogl_trace_file_reader::cEOF)
        {
            vogl_message_printf("At trace file EOF\n");
            break;
        }
        else if (read_status != vogl_trace_file_reader::cOK)
        {
            vogl_error_printf("Failed reading from trace file, or file size was too small\n");

            status = false;
            break;
        }

        if (pTrace_reader->get_packet_type() != cTSPTGLEntrypoint)
        {
            if (pTrace_reader->get_packet_type() == cTSPTSOF)
            {
                vogl_error_printf("Encountered redundant SOF packet!\n");
                status = false;
            }

            json_node *pMeta_node = cur_doc.get_root()->find_child_object("meta");
            if (pMeta_node)
                pMeta_node->add_key_value("eof", (pTrace_reader->get_packet_type() == cTSPTEOF) ? 1 : 2);

            break;
        }

        if (flush_current_document)
        {
            flush_current_document = false;

            dynamic_string output_filename(cVarArg, "%s_%06u.json", output_base_filename.get_ptr(), cur_file_index);
            vogl_message_printf("Writing file: \"%s\"\n", output_filename.get_ptr());

            if (!cur_doc.serialize_to_file(output_filename.get_ptr(), true))
            {
                vogl_error_printf("Failed serializing JSON document to file %s\n", output_filename.get_ptr());

                status = false;
                break;
            }

            if (cur_doc.get_root()->check_for_duplicate_keys())
                vogl_warning_printf("JSON document %s has nodes with duplicate keys, this document may not be readable by some JSON parsers\n", output_filename.get_ptr());

            cur_file_index++;

            pPacket_array = NULL;
            cur_doc.clear();

            json_node &note_node = cur_doc.get_root()->add_object("meta");
            note_node.add_key_value("cur_frame", cur_frame_index);

            json_node &uuid_array = note_node.add_array("uuid");
            for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(pTrace_reader->get_sof_packet().m_uuid); i++)
                uuid_array.add_value(pTrace_reader->get_sof_packet().m_uuid[i]);

            pPacket_array = &cur_doc.get_root()->add_array("packets");
        }

        const vogl_trace_gl_entrypoint_packet &gl_packet = pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
        const char *pFunc_name = g_vogl_entrypoint_descs[gl_packet.m_entrypoint_id].m_pName;
        VOGL_NOTE_UNUSED(pFunc_name);

        if (g_command_line_params().get_value_as_bool("debug"))
        {
            vogl_debug_printf("Trace packet: File offset: %" PRIu64 ", Total size %u, Param size: %u, Client mem size %u, Name value size %u, call %" PRIu64 ", ID: %s (%u), Thread ID: 0x%" PRIX64 ", Trace Context: 0x%" PRIX64 "\n",
                             cur_packet_ofs,
                             gl_packet.m_size,
                             gl_packet.m_param_size,
                             gl_packet.m_client_memory_size,
                             gl_packet.m_name_value_map_size,
                             gl_packet.m_call_counter,
                             g_vogl_entrypoint_descs[gl_packet.m_entrypoint_id].m_pName,
                             gl_packet.m_entrypoint_id,
                             gl_packet.m_thread_id,
                             gl_packet.m_context_handle);
        }

        if (!gl_packet_cracker.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), true))
        {
            vogl_error_printf("Failed deserializing GL entrypoint packet. Trying to continue parsing the file, this may die!\n");

            //status = false;
            //break;
            continue;
        }

        json_node &new_node = pPacket_array->add_object();

        vogl_trace_packet::json_serialize_params serialize_params;
        serialize_params.m_output_basename = file_utils::get_filename(output_base_filename.get_ptr());
        serialize_params.m_pBlob_manager = &output_file_blob_manager;
        serialize_params.m_cur_frame = cur_file_index;
        serialize_params.m_write_debug_info = g_command_line_params().get_value_as_bool("write_debug_info");
        if (!gl_packet_cracker.json_serialize(new_node, serialize_params))
        {
            vogl_error_printf("JSON serialization failed!\n");

            status = false;
            break;
        }

        if (full_verification)
        {
#if 0
            if (!strcmp(pFunc_name, "glClearColor"))
            {
                VOGL_BREAKPOINT
            }
#endif

            vogl::vector<char> new_node_as_text;
            new_node.serialize(new_node_as_text, true, 0);

#if 0
            if (new_node_as_text.size())
            {
                printf("%s\n", new_node_as_text.get_ptr());
            }
#endif

            json_document round_tripped_node;
            if (!round_tripped_node.deserialize(new_node_as_text.get_ptr()) || !round_tripped_node.get_root())
            {
                vogl_error_printf("Failed verifying serialized JSON data (step 1)!\n");

                status = false;
                break;
            }
            else
            {
                vogl_trace_packet temp_cracker(&trace_gl_ctypes);
                bool success = temp_cracker.json_deserialize(*round_tripped_node.get_root(), "<memory>", &output_file_blob_manager);
                if (!success)
                {
                    vogl_error_printf("Failed verifying serialized JSON data (step 2)!\n");

                    status = false;
                    break;
                }
                else
                {
                    success = gl_packet_cracker.compare(temp_cracker, false);
                    if (!success)
                    {
                        vogl_error_printf("Failed verifying serialized JSON data (step 3)!\n");

                        status = false;
                        break;
                    }
                    else
                    {
                        dynamic_stream dyn_stream;

                        success = temp_cracker.serialize(dyn_stream);
                        if (!success)
                        {
                            vogl_error_printf("Failed verifying serialized JSON data (step 4)!\n");

                            status = false;
                            break;
                        }
                        else
                        {
                            vogl_trace_packet temp_cracker2(&trace_gl_ctypes);
                            success = temp_cracker2.deserialize(static_cast<const uint8_t *>(dyn_stream.get_ptr()), static_cast<uint32_t>(dyn_stream.get_size()), true);
                            if (!success)
                            {
                                vogl_error_printf("Failed verifying serialized JSON data (step 5)!\n");

                                status = false;
                                break;
                            }
                            success = gl_packet_cracker.compare(temp_cracker2, true);
                            if (!success)
                            {
                                vogl_error_printf("Failed verifying serialized JSON data (step 6)!\n");

                                status = false;
                                break;
                            }
                            else
                            {
                                uint64_t binary_serialized_size = dyn_stream.get_size();
                                if (binary_serialized_size != pTrace_reader->get_packet_buf().size())
                                {
                                    vogl_error_printf("Round-tripped binary serialized size differs from original packet's' size (step 7)!\n");

                                    status = false;
                                    break;
                                }
                                else
                                {
#if 0
									// This is excessive- the key value map fields may be binary serialized in different orders
									// TODO: maybe fix the key value map class so it serializes in a stable order (independent of hash table construction)?
									const uint8_t *p = static_cast<const uint8_t *>(dyn_stream.get_ptr());
									const uint8_t *q = static_cast<const uint8_t *>(pTrace_reader->get_packet_buf().get_ptr());
									if (memcmp(p, q, binary_serialized_size) != 0)
									{
										file_utils::write_buf_to_file("p.bin", p, binary_serialized_size);
										file_utils::write_buf_to_file("q.bin", q, binary_serialized_size);
										vogl_error_printf("Round-tripped binary serialized data differs from original packet's data (step 7)!\n");

										status = false;
										break;
									}
#endif
                                }
                            }
                        }
                    }
                }
            }
        }

        if (vogl_is_swap_buffers_entrypoint(static_cast<gl_entrypoint_id_t>(gl_packet.m_entrypoint_id)))
        {
            flush_current_document = true;
            cur_frame_index++;
        }

        if (cur_doc.get_root()->size() >= 1 * 1000 * 1000)
        {
            // TODO: Support replaying dumps like this, or fix the code to serialize the text as it goes.
            vogl_error_printf("Haven't encountered a SwapBuffers() call in over 1000000 GL calls, dumping current in-memory JSON document to disk to avoid running out of memory. This JSON dump may not be replayable, but writing it anyway.\n");
            flush_current_document = true;
        }
    }

    json_node *pMeta_node = cur_doc.get_root()->find_child_object("meta");

    if (pMeta_node)
    {
        if (!pMeta_node->has_key("eof"))
            pMeta_node->add_key_value("eof", 2);

        dynamic_string output_filename(cVarArg, "%s_%06u.json", output_base_filename.get_ptr(), cur_file_index);
        vogl_message_printf("Writing file: \"%s\"\n", output_filename.get_ptr());

        if (!cur_doc.serialize_to_file(output_filename.get_ptr(), true))
        {
            vogl_error_printf("Failed serializing JSON document to file %s\n", output_filename.get_ptr());

            status = false;
        }

        if (cur_doc.get_root()->check_for_duplicate_keys())
            vogl_warning_printf("JSON document %s has nodes with duplicate keys, this document may not be readable by some JSON parsers\n", output_filename.get_ptr());

        cur_file_index++;
    }

    if (!status)
        vogl_error_printf("Failed dumping binary trace to JSON files starting with filename prefix \"%s\" (but wrote as much as possible)\n", output_base_filename.get_ptr());
    else
        vogl_message_printf("Successfully dumped binary trace to %u JSON file(s) starting with filename prefix \"%s\"\n", cur_file_index, output_base_filename.get_ptr());

    return status;
}
