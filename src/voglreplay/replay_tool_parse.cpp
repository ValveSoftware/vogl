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
#include "vogl_trace_file_writer.h"

#include "vogl_file_utils.h"

static command_line_param_desc g_command_line_param_descs_parse[] =
{
    // parse specific
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
};

//----------------------------------------------------------------------------------------------------------------------
// tool_parse_mode
//----------------------------------------------------------------------------------------------------------------------
bool tool_parse_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    if (desc)
    {
        desc->append(g_command_line_param_descs_parse, VOGL_ARRAY_SIZE(g_command_line_param_descs_parse));
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

    dynamic_string output_trace_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_trace_filename.is_empty())
    {
        vogl_error_printf("Must specify full filename of output binary trace file!\n");
        return false;
    }

    file_utils::create_directories(output_trace_filename, true);

    if (file_utils::add_default_extension(output_trace_filename, ".bin"))
        vogl_message_printf("Trim output filename doesn't have an extension, appending \".bin\" to the filename\n");

    // TODO: Refactor this, I think we're going to write the actual ctypes and entrpoint descs, etc. into the trace archive next.
    vogl_ctypes trace_ctypes;
    trace_ctypes.init(pTrace_reader->get_sof_packet().m_pointer_sizes);

    vogl_trace_file_writer trace_writer(&trace_ctypes);
    if (!trace_writer.open(output_trace_filename.get_ptr(), NULL, true, false, pTrace_reader->get_sof_packet().m_pointer_sizes))
    {
        vogl_error_printf("Unable to create file \"%s\"!\n", output_trace_filename.get_ptr());
        return false;
    }

    if (pTrace_reader->get_archive_blob_manager().is_initialized())
    {
        dynamic_string_array blob_files(pTrace_reader->get_archive_blob_manager().enumerate());
        for (uint32_t i = 0; i < blob_files.size(); i++)
        {
            if (blob_files[i] == VOGL_TRACE_ARCHIVE_FRAME_FILE_OFFSETS_FILENAME)
                continue;

            vogl_message_printf("Adding blob file %s to output trace archive\n", blob_files[i].get_ptr());

            uint8_vec blob_data;
            if (pTrace_reader->get_archive_blob_manager().get(blob_files[i], blob_data))
            {
                if (trace_writer.get_trace_archive()->add_buf_using_id(blob_data.get_ptr(), blob_data.size(), blob_files[i]).is_empty())
                {
                    vogl_error_printf("Failed writing blob data %s to output trace archive!\n", blob_files[i].get_ptr());
                    return false;
                }
            }
            else
            {
                vogl_error_printf("Failed reading blob data %s from trace archive!\n", blob_files[i].get_ptr());
                return false;
            }
        }
    }

    for (;;)
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogl_error_printf("Failed reading from trace file\n");
            goto failed;
        }

        if ((read_status == vogl_trace_file_reader::cEOF) || (pTrace_reader->is_eof_packet()))
        {
            vogl_message_printf("At trace file EOF\n");
            break;
        }

        const vogl::vector<uint8_t> &packet_buf = pTrace_reader->get_packet_buf();

        if (!trace_writer.write_packet(packet_buf.get_ptr(), packet_buf.size(), pTrace_reader->is_swap_buffers_packet()))
        {
            vogl_error_printf("Failed writing to output trace file \"%s\"\n", output_trace_filename.get_ptr());
            goto failed;
        }
    }

    if (!trace_writer.close())
    {
        vogl_error_printf("Failed closing output trace file \"%s\"\n", output_trace_filename.get_ptr());
        goto failed;
    }

    vogl_message_printf("Successfully wrote binary trace file \"%s\"\n", output_trace_filename.get_ptr());

    return true;

failed:
    trace_writer.close();

    vogl_warning_printf("Processing failed, output trace file \"%s\" may be invalid!\n", output_trace_filename.get_ptr());

    return false;
}
