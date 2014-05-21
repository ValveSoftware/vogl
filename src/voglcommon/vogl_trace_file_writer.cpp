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
// File: vogl_trace_file_writer.cpp
//----------------------------------------------------------------------------------------------------------------------
#include "vogl_trace_file_writer.h"
#include "vogl_console.h"
#include "vogl_file_utils.h"
#include "vogl_uuid.h"

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_file_writer
//----------------------------------------------------------------------------------------------------------------------
vogl_trace_file_writer::vogl_trace_file_writer(const vogl_ctypes *pCTypes)
    : m_gl_call_counter(0),
      m_pCTypes(pCTypes),
      m_pTrace_archive(NULL),
      m_delete_archive(false)
{
    VOGL_FUNC_TRACER
}

vogl_trace_file_writer::~vogl_trace_file_writer()
{
    VOGL_FUNC_TRACER

    close();
}

// pTrace_archive may be NULL. Takes ownership of pTrace_archive.
// TODO: Get rid of the demarcation packet, etc. Make the initial sequence of packets more explicit.
bool vogl_trace_file_writer::open(const char *pFilename, vogl_archive_blob_manager *pTrace_archive, bool delete_archive, bool write_demarcation_packet, uint32_t pointer_sizes)
{
    VOGL_FUNC_TRACER

    close();

    if (!pFilename)
        return false;

    m_filename = pFilename;
    if (!m_stream.open(pFilename, cDataStreamWritable | cDataStreamSeekable, false))
    {
        vogl_error_printf("Failed opening trace file \"%s\"\n", pFilename);
        return false;
    }

    vogl_verbose_printf("Prepping trace file \"%s\"\n", pFilename);

    m_sof_packet.init();
    m_sof_packet.m_pointer_sizes = pointer_sizes;
    m_sof_packet.m_first_packet_offset = sizeof(m_sof_packet);

    md5_hash h(gen_uuid());
    VOGL_ASSUME(sizeof(h) == sizeof(m_sof_packet.m_uuid));
    memcpy(&m_sof_packet.m_uuid, &h, sizeof(h));

    m_sof_packet.finalize();
    VOGL_VERIFY(m_sof_packet.full_validation(sizeof(m_sof_packet)));

    if (m_stream.write(&m_sof_packet, sizeof(m_sof_packet)) != sizeof(m_sof_packet))
    {
        vogl_error_printf("Failed writing to trace file \"%s\"\n", pFilename);
        return false;
    }

    if (pTrace_archive)
    {
        m_pTrace_archive.reset(pTrace_archive);
        m_delete_archive = delete_archive;
    }
    else
    {
        m_pTrace_archive.reset(vogl_new(vogl_archive_blob_manager));
        m_delete_archive = true;

        if (!m_pTrace_archive->init_file_temp(cBMFReadWrite))
        {
            vogl_error_printf("Failed opening temp archive!\n");

            m_pTrace_archive.reset();

            return false;
        }
    }

    // TODO: The trace reader records the first offset right after SOF, I would like to do this after the demarcation packet.
    m_frame_file_offsets.reserve(10000);
    m_frame_file_offsets.resize(0);
    m_frame_file_offsets.push_back(m_stream.get_ofs());

    write_ctypes_packet();

    write_entrypoints_packet();

    if (write_demarcation_packet)
    {
        vogl_write_glInternalTraceCommandRAD(m_stream, m_pCTypes, cITCRDemarcation, 0, NULL);
    }

    vogl_verbose_printf("Finished opening trace file \"%s\"\n", pFilename);

    return true;
}

bool vogl_trace_file_writer::close()
{
    VOGL_FUNC_TRACER

    vogl_debug_printf("vogl_trace_file_writer close\n");

    if (!m_stream.is_opened())
        return false;

    vogl_verbose_printf("Flushing trace file %s (this could take some time), %u total frame file offsets\n", m_filename.get_ptr(), m_frame_file_offsets.size());

    bool success = true;

    dynamic_string trace_archive_filename;

    if (!write_eof_packet())
    {
        vogl_error_printf("Failed writing to trace file \"%s\"\n", m_filename.get_ptr());
        success = false;
    }
    else if (m_pTrace_archive.get())
    {
        trace_archive_filename = m_pTrace_archive->get_archive_filename();

        if ((!write_frame_file_offsets_to_archive()) || !m_pTrace_archive->deinit())
        {
            vogl_error_printf("Failed closing trace archive \"%s\"!\n", trace_archive_filename.get_ptr());
            success = false;
        }
        else
        {
            if (!file_utils::get_file_size(trace_archive_filename.get_ptr(), m_sof_packet.m_archive_size))
            {
                vogl_error_printf("Failed determining file size of archive file \"%s\"\n", trace_archive_filename.get_ptr());
                success = false;
            }
            else if (m_sof_packet.m_archive_size)
            {
                m_sof_packet.m_archive_offset = m_stream.get_size();

                vogl_verbose_printf("Copying %" PRIu64 " archive bytes into output trace file\n", m_sof_packet.m_archive_size);

                if (!m_stream.write_file_data(trace_archive_filename.get_ptr()))
                {
                    vogl_error_printf("Failed copying source archive \"%s\" into trace file!\n", trace_archive_filename.get_ptr());
                    success = false;
                }

                m_sof_packet.m_archive_size = m_stream.get_size() - m_sof_packet.m_archive_offset;
            }
        }

        if (success)
        {
            m_sof_packet.finalize();
            VOGL_VERIFY(m_sof_packet.full_validation(sizeof(m_sof_packet)));

            if (!m_stream.seek(0, false) || (m_stream.write(&m_sof_packet, sizeof(m_sof_packet)) != sizeof(m_sof_packet)))
            {
                vogl_error_printf("Failed writing to trace file \"%s\"\n", m_filename.get_ptr());
                success = false;
            }
        }
    }

    close_archive(trace_archive_filename.get_ptr());

    uint64_t total_trace_file_size = m_stream.get_size();

    if (!m_stream.close())
    {
        vogl_error_printf("Failed writing to or closing trace file!\n");
        success = false;
    }

    dynamic_string full_trace_filename(m_filename);
    file_utils::full_path(full_trace_filename);

    if (success)
        vogl_message_printf("Successfully closed trace file %s, total file size: %s\n", full_trace_filename.get_ptr(), uint64_to_string_with_commas(total_trace_file_size).get_ptr());
    else
        vogl_error_printf("Failed closing trace file %s! (Trace will probably not be valid.)\n", full_trace_filename.get_ptr());

    return success;
}

void vogl_trace_file_writer::write_ctypes_packet()
{
    VOGL_FUNC_TRACER

    key_value_map typemap_key_values;
    typemap_key_values.insert("command_type", "ctypes");
    typemap_key_values.insert("num_ctypes", VOGL_NUM_CTYPES);
    for (uint32_t ctype_iter = 0; ctype_iter < VOGL_NUM_CTYPES; ctype_iter++)
    {
        const vogl_ctype_desc_t &desc = (*m_pCTypes)[static_cast<vogl_ctype_t>(ctype_iter)];

        uint32_t base_index = ctype_iter << 8;
        typemap_key_values.insert(base_index++, desc.m_pName);
        typemap_key_values.insert(base_index++, desc.m_pCType);
        typemap_key_values.insert(base_index++, desc.m_size);
        typemap_key_values.insert(base_index++, desc.m_loki_type_flags);
        typemap_key_values.insert(base_index++, desc.m_is_pointer);
        typemap_key_values.insert(base_index++, desc.m_is_opaque_pointer);
        typemap_key_values.insert(base_index++, desc.m_is_pointer_diff);
        typemap_key_values.insert(base_index++, desc.m_is_opaque_type);
    }
    vogl_write_glInternalTraceCommandRAD(m_stream, m_pCTypes, cITCRKeyValueMap, sizeof(typemap_key_values), reinterpret_cast<const GLubyte *>(&typemap_key_values));
}

void vogl_trace_file_writer::write_entrypoints_packet()
{
    VOGL_FUNC_TRACER

    key_value_map entrypoint_key_values;
    entrypoint_key_values.insert("command_type", "entrypoints");
    entrypoint_key_values.insert("num_entrypoints", VOGL_NUM_ENTRYPOINTS);
    for (uint32_t func_iter = 0; func_iter < VOGL_NUM_ENTRYPOINTS; func_iter++)
    {
        const gl_entrypoint_desc_t &desc = g_vogl_entrypoint_descs[func_iter];
        entrypoint_key_values.insert(func_iter, desc.m_pName);
    }

    vogl_write_glInternalTraceCommandRAD(m_stream, m_pCTypes, cITCRKeyValueMap, sizeof(entrypoint_key_values), reinterpret_cast<const GLubyte *>(&entrypoint_key_values));
}

bool vogl_trace_file_writer::write_eof_packet()
{
    VOGL_FUNC_TRACER

    vogl_trace_stream_packet_base eof_packet;
    eof_packet.init(cTSPTEOF, sizeof(vogl_trace_stream_packet_base));
    eof_packet.finalize();
    return m_stream.write(&eof_packet, sizeof(eof_packet)) != 0;
}

bool vogl_trace_file_writer::write_frame_file_offsets_to_archive()
{
    VOGL_FUNC_TRACER

    if (!m_pTrace_archive.get())
        return false;

    if (m_frame_file_offsets.is_empty())
        return true;

    return m_pTrace_archive->add_buf_using_id(m_frame_file_offsets.get_ptr(), m_frame_file_offsets.size_in_bytes(), VOGL_TRACE_ARCHIVE_FRAME_FILE_OFFSETS_FILENAME).has_content();
}

void vogl_trace_file_writer::close_archive(const char *pArchive_filename)
{
    VOGL_FUNC_TRACER

    if (m_pTrace_archive.get())
    {
        m_pTrace_archive.reset();

        if (m_delete_archive && pArchive_filename)
        {
            file_utils::delete_file(pArchive_filename);
        }
    }
    m_delete_archive = false;
}
