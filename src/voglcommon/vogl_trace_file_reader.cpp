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

// File: vogl_trace_file_reader.cpp
#include "vogl_trace_file_reader.h"
#include "vogl_console.h"
#include "vogl_file_utils.h"

vogl_trace_file_reader::trace_file_reader_status_t vogl_trace_file_reader::read_frame_packets(uint32_t frame_index, uint32_t num_frames, vogl_trace_packet_array &packets, uint32_t &actual_frames_read)
{
    VOGL_FUNC_TRACER

    actual_frames_read = 0;

    if (!is_opened())
    {
        vogl_error_printf("Trace file is not open\n");

        VOGL_ASSERT_ALWAYS;

        return cFailed;
    }

    if (!num_frames)
    {
        actual_frames_read = 0;
        return cOK;
    }

    vogl_scoped_location_saver saved_loc(*this);

    if (!seek_to_frame(frame_index))
    {
        vogl_error_printf("Failed seeking to frame %u\n", frame_index);
        return cFailed;
    }

    uint32_t total_frames_read = 0;

    packets.reserve(packets.size() + num_frames * 1000);

    trace_file_reader_status_t status = cOK;
    for (;;)
    {
        status = read_next_packet();
        if (status == cFailed)
        {
            vogl_error_printf("Failed reading from trace file\n");
            break;
        }

        packets.push_back(get_packet_buf());

        if (is_eof_packet())
            break;

        if (is_swap_buffers_packet())
        {
            if (++total_frames_read == num_frames)
                break;
        }
    }

    actual_frames_read = total_frames_read;

    return cOK;
}

void vogl_trace_file_reader::create_eof_packet()
{
    VOGL_FUNC_TRACER

    vogl_trace_stream_packet_base eof_packet;
    eof_packet.init(cTSPTEOF, sizeof(vogl_trace_stream_packet_base));
    eof_packet.finalize();
    m_packet_buf.resize(0);
    m_packet_buf.append(reinterpret_cast<uint8_t *>(&eof_packet), sizeof(eof_packet));
}

bool vogl_trace_file_reader::init_loose_file_blob_manager(const char *pTrace_filename, const char *pLoose_file_path)
{
    VOGL_FUNC_TRACER

    dynamic_string loose_file_path(".");

    if ((pLoose_file_path) && vogl_strlen(pLoose_file_path))
        loose_file_path = pLoose_file_path;
    else if ((pTrace_filename) && (vogl_strlen(pTrace_filename)))
    {
        dynamic_string fname;
        if (!file_utils::split_path(pTrace_filename, loose_file_path, fname))
        {
            vogl_error_printf("Failed splitting trace filename \"%s\", assuming \".\" as the loose file path\n", pTrace_filename);
            loose_file_path = ".";
        }
    }

    if (!m_loose_file_blob_manager.init(cBMFReadable, loose_file_path.get_ptr()))
    {
        close();
        return false;
    }

    return true;
}

vogl_binary_trace_file_reader::vogl_binary_trace_file_reader()
    : vogl_trace_file_reader(),
      m_trace_file_size(0),
      m_cur_frame_index(0),
      m_max_frame_index(-1),
      m_found_frame_file_offsets_packet(0)
{
    VOGL_FUNC_TRACER

    m_frame_file_offsets.reserve(4096);
}

vogl_binary_trace_file_reader::~vogl_binary_trace_file_reader()
{
    VOGL_FUNC_TRACER

    close();
}

bool vogl_binary_trace_file_reader::read_frame_file_offsets()
{
    VOGL_FUNC_TRACER

    m_frame_file_offsets.clear();
    m_max_frame_index = -1;
    m_found_frame_file_offsets_packet = false;

    uint8_vec frame_offsets_data;
    if (!m_archive_blob_manager.is_initialized() || !m_archive_blob_manager.get(VOGL_TRACE_ARCHIVE_FRAME_FILE_OFFSETS_FILENAME, frame_offsets_data))
    {
        vogl_debug_printf("Couldn't find trace frame file offset file in trace archive, seeking will be slow in this trace file\n");
        return false;
    }

    if (frame_offsets_data.size() & (sizeof(uint64_t) - 1))
    {
        vogl_error_printf("Trace frame file offset file in trace archive is invalid, seeking will be slow in this trace file\n");
        return false;
    }

    uint32_t total_offsets = frame_offsets_data.size() / sizeof(uint64_t);

    m_frame_file_offsets.resize(total_offsets);
    memcpy(m_frame_file_offsets.get_ptr(), frame_offsets_data.get_ptr(), m_frame_file_offsets.size_in_bytes());

    vogl_debug_printf("Frame file offsets packet is OK, found %u total frame offsets\n", m_frame_file_offsets.size());

    m_max_frame_index = m_frame_file_offsets.size() - 1;

    m_found_frame_file_offsets_packet = true;
    return true;
}

bool vogl_binary_trace_file_reader::open(const char *pFilename, const char *pLoose_file_path)
{
    VOGL_FUNC_TRACER

    close();

    if (!init_loose_file_blob_manager(pFilename, pLoose_file_path))
        return false;

    if (!m_trace_stream.open(pFilename, cDataStreamReadable | cDataStreamSeekable, true))
    {
        close();
        return false;
    }

    m_trace_file_size = m_trace_stream.get_size();

    if (m_trace_stream.read(&m_sof_packet, sizeof(m_sof_packet)) != sizeof(m_sof_packet))
    {
        close();
        return false;
    }

    if (!m_sof_packet.full_validation(sizeof(m_sof_packet)))
    {
        vogl_error_printf("Trace file failed validation!\n");
        if (m_sof_packet.m_version < static_cast<uint16_t>(VOGL_TRACE_FILE_MINIMUM_COMPATIBLE_VERSION))
        {
            vogl_error_printf("Trace file version is not supported, found version 0x%04X, expected at least version 0x%04X!\n", m_sof_packet.m_version, VOGL_TRACE_FILE_MINIMUM_COMPATIBLE_VERSION);
        }
        close();
        return false;
    }

    if (m_sof_packet.m_version > static_cast<uint16_t>(VOGL_TRACE_FILE_VERSION))
    {
        // TODO: Make this an error? Backwards compat?
        vogl_warning_printf("Trace file version is 0x%04X, expected version 0x%04X, this may not work at all!\n", m_sof_packet.m_version, VOGL_TRACE_FILE_VERSION);
    }

    if (m_sof_packet.m_archive_size)
    {
        if (!m_archive_blob_manager.init_file(cBMFReadable | cBMFOpenExisting, pFilename, m_sof_packet.m_archive_offset, m_sof_packet.m_archive_size))
        {
            vogl_error_printf("Failed reading in-trace archive!\n");
            return false;
        }
    }

    m_packet_buf.reserve(512 * 1024);

    m_trace_stream.seek(m_sof_packet.m_first_packet_offset, false);

    if (!read_frame_file_offsets())
    {
        // Keep this in sync with the offset pushed in vogl_init_tracefile()!
        m_frame_file_offsets.push_back(get_cur_file_ofs());
    }

    return true;
}

bool vogl_binary_trace_file_reader::is_opened()
{
    VOGL_FUNC_TRACER

    return m_trace_stream.is_opened();
}

const char *vogl_binary_trace_file_reader::get_filename()
{
    VOGL_FUNC_TRACER

    return m_trace_stream.get_name().get_ptr();
}

void vogl_binary_trace_file_reader::close()
{
    VOGL_FUNC_TRACER

    vogl_trace_file_reader::close();

    m_trace_stream.close();
    m_trace_file_size = 0;

    m_cur_frame_index = 0;
    m_max_frame_index = -1;
    m_frame_file_offsets.resize(0);

    m_saved_location_stack.clear();

    m_found_frame_file_offsets_packet = false;
}

bool vogl_binary_trace_file_reader::is_at_eof()
{
    VOGL_FUNC_TRACER

    return (m_trace_stream.get_remaining() < sizeof(vogl_trace_stream_packet_base));
}

bool vogl_binary_trace_file_reader::seek_to_frame(uint32_t frame_index)
{
    VOGL_FUNC_TRACER

    if (!is_opened())
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if (frame_index >= m_frame_file_offsets.size())
    {
        if ((m_max_frame_index >= 0) && (frame_index > m_max_frame_index))
        {
            vogl_warning_printf("Can't seek to frame %u, max valid frame index is %u\n", frame_index, static_cast<uint32_t>(m_max_frame_index));
            return false;
        }

        vogl_warning_printf("Seeking forward in binary trace from frame %u to frame %u, this could take a while to build the index\n", m_cur_frame_index, frame_index);

        if (m_frame_file_offsets.size())
        {
            seek(m_frame_file_offsets.back());
            m_cur_frame_index = m_frame_file_offsets.size() - 1;
        }

        do
        {
            trace_file_reader_status_t status = read_next_packet();
            if (status == cFailed)
            {
                vogl_error_printf("Failed reading next packet\n");
                return false;
            }

            if ((status == cEOF) || (is_eof_packet()))
                break;

        } while (frame_index >= m_frame_file_offsets.size());

        if (frame_index >= m_frame_file_offsets.size())
        {
            if (g_command_line_params().get_value_as_bool("verbose"))
                vogl_debug_printf("Failed seeking forward in binary trace to frame %u, cur frame is now %u\n", frame_index, m_cur_frame_index);
            return false;
        }
    }

    seek(m_frame_file_offsets[frame_index]);
    m_cur_frame_index = frame_index;
    return true;
}

int64_t vogl_binary_trace_file_reader::get_max_frame_index()
{
    VOGL_FUNC_TRACER

    if (m_max_frame_index < 0)
    {
        push_location();

        seek_to_frame(0xFFFFFFFF);

        pop_location();
    }

    if (m_max_frame_index >= 0)
        return m_max_frame_index;
    else
        return -1;
}

vogl_trace_file_reader::trace_file_reader_status_t vogl_binary_trace_file_reader::read_next_packet()
{
    VOGL_FUNC_TRACER

    m_packet_buf.resize(sizeof(vogl_trace_stream_packet_base));

    {
        vogl_trace_stream_packet_base &packet_base = *reinterpret_cast<vogl_trace_stream_packet_base *>(m_packet_buf.get_ptr());
        uint32_t bytes_actually_read = m_trace_stream.read(&packet_base, sizeof(packet_base));
        if (bytes_actually_read != sizeof(packet_base))
        {
            // Jam in a fake EOF packet in case the caller doesn't get the message that something is wrong
            create_eof_packet();

            // The could happen if the file was truncated, or the last packet didn't get entirely written.
            if (bytes_actually_read)
                return cFailed;

            if (m_max_frame_index < 0)
                m_max_frame_index = m_cur_frame_index;
            else
                VOGL_ASSERT(m_max_frame_index == m_cur_frame_index);

            return cEOF;
        }

        if ((!packet_base.basic_validation()) || (packet_base.m_size >= 0x7FFFFFFFULL))
        {
            vogl_error_printf("Bad trace file - packet failed basic validation tests!\n");

            create_eof_packet();

            return cFailed;
        }

        m_packet_buf.resize(packet_base.m_size);
    }

    vogl_trace_stream_packet_base &packet_base = *reinterpret_cast<vogl_trace_stream_packet_base *>(m_packet_buf.get_ptr());

    uint32_t num_bytes_remaining = packet_base.m_size - sizeof(vogl_trace_stream_packet_base);
    if (num_bytes_remaining)
    {
        uint32_t actual_bytes_read = m_trace_stream.read(m_packet_buf.get_ptr() + sizeof(vogl_trace_stream_packet_base), num_bytes_remaining);
        if (actual_bytes_read != num_bytes_remaining)
        {
            vogl_error_printf("Failed reading variable size trace packet data (wanted %u bytes, got %u bytes), trace file is probably corrupted/invalid\n", num_bytes_remaining, actual_bytes_read);
            return cFailed;
        }
    }

    if (!packet_base.check_crc(packet_base.m_size))
    {
        vogl_error_printf("Bad trace file - packet CRC32 is bad!\n");

        create_eof_packet();

        return cFailed;
    }

    if (is_eof_packet())
    {
        if (m_max_frame_index < 0)
            m_max_frame_index = m_cur_frame_index;
        else
            VOGL_ASSERT(m_max_frame_index == m_cur_frame_index);
    }
    else if (is_swap_buffers_packet())
    {
        m_cur_frame_index++;

        if (m_cur_frame_index >= m_frame_file_offsets.size())
        {
            m_frame_file_offsets.resize(math::maximum(m_frame_file_offsets.size(), m_cur_frame_index + 1));
            m_frame_file_offsets[m_cur_frame_index] = get_cur_file_ofs();
        }
        else
        {
            VOGL_ASSERT(m_frame_file_offsets[m_cur_frame_index] == get_cur_file_ofs());
        }
    }

    return cOK;
}

bool vogl_binary_trace_file_reader::push_location()
{
    VOGL_FUNC_TRACER

    if (!m_trace_stream.is_opened())
        return false;

    saved_location *p = m_saved_location_stack.enlarge(1);
    p->m_cur_frame_index = m_cur_frame_index;
    p->m_cur_ofs = m_trace_stream.get_ofs();

    return true;
}

bool vogl_binary_trace_file_reader::pop_location()
{
    VOGL_FUNC_TRACER

    if ((!m_trace_stream.is_opened()) || (m_saved_location_stack.is_empty()))
        return false;

    saved_location &loc = m_saved_location_stack.back();

    bool success = true;

    if (!m_trace_stream.seek(loc.m_cur_ofs, false))
        success = false;
    else
        m_cur_frame_index = loc.m_cur_frame_index;

    m_saved_location_stack.pop_back();

    return success;
}

//-----------------------------------------------------------------------------
// vogl_json_trace_file_reader::vogl_json_trace_file_reader
//-----------------------------------------------------------------------------
vogl_json_trace_file_reader::vogl_json_trace_file_reader()
    : vogl_trace_file_reader(),
      m_filename_exists(false),
      m_filename_is_in_multiframe_form(false),
      m_cur_frame_index(0), m_max_frame_index(0), m_cur_packet_node_index(0), m_packet_node_size(0), m_doc_eof_key_value(false),
      m_at_eof(false),
      m_trace_packet(&m_trace_ctypes)
{
    VOGL_FUNC_TRACER
}

vogl_json_trace_file_reader::~vogl_json_trace_file_reader()
{
    VOGL_FUNC_TRACER

    close();
}

bool vogl_json_trace_file_reader::open(const char *pFilename, const char *pLoose_file_path)
{
    VOGL_FUNC_TRACER

    close();

    if (!init_loose_file_blob_manager(pFilename, pLoose_file_path))
        return false;

    m_filename = pFilename;
    m_filename_is_in_multiframe_form = vogl_is_multiframe_json_trace_filename(pFilename);

    if (!file_utils::split_path(pFilename, &m_drive, &m_dir, &m_fname, &m_ext))
    {
        vogl_error_printf("Failed splitting input filename: \"%s\"\n", pFilename);
        close();
        return false;
    }

    m_filename_exists = file_utils::does_file_exist(pFilename);
    if ((!m_filename_exists) && (m_filename_is_in_multiframe_form))
    {
        vogl_error_printf("Could not open JSON trace file: \"%s\"\n", pFilename);
        close();
        return false;
    }

    if ((m_filename_exists) && (!m_filename_is_in_multiframe_form))
    {
        // Assume they just want to play one frame.
        m_cur_frame_filename = m_filename;
        m_base_filename = m_filename;
        m_max_frame_index = 0;
    }
    else
    {
        uint32_t i;
        for (i = 0; i < 99999999; i++)
        {
            dynamic_string trial_base_name(m_fname);
            if (m_filename_is_in_multiframe_form)
                trial_base_name.shorten(7);

            if (!i)
                file_utils::combine_path(m_base_filename, m_drive.get_ptr(), m_dir.get_ptr(), trial_base_name.get_ptr());

            dynamic_string trial_name(cVarArg, "%s_%06u", trial_base_name.get_ptr(), i);

            dynamic_string trial_filename;
            file_utils::combine_path_and_extension(trial_filename, m_drive.get_ptr(), m_dir.get_ptr(), trial_name.get_ptr(), m_ext.get_ptr());

            if (!file_utils::does_file_exist(trial_filename.get_ptr()))
                break;

            if (!i)
                m_cur_frame_filename = trial_filename;
        }

        if (!i)
        {
            vogl_error_printf("Could not open JSON trace file \"%s\"\n", pFilename);
            close();
            return false;
        }

        m_max_frame_index = i - 1;
    }

    if (!open_first_document())
    {
        close();
        return false;
    }

    vogl_message_printf("Opened JSON trace file \"%s\", frame range [%u - %u]\n", m_cur_frame_filename.get_ptr(), m_cur_frame_index, m_max_frame_index);

    return true;
}

bool vogl_json_trace_file_reader::is_opened()
{
    VOGL_FUNC_TRACER

    return m_pPackets_array != NULL;
}

const char *vogl_json_trace_file_reader::get_filename()
{
    VOGL_FUNC_TRACER

    return m_filename.get_ptr();
}

bool vogl_json_trace_file_reader::open_first_document()
{
    VOGL_FUNC_TRACER

    if (!read_document(m_cur_frame_filename))
        return false;

    const json_node *pRoot_node = m_cur_doc.get_root();
    if (!pRoot_node)
    {
        vogl_error_printf("JSON file must be an object: \"%s\"\n", m_cur_frame_filename.get_ptr());
        return false;
    }

    const json_node *pSOF_node = pRoot_node->find_child_object("sof");
    if (!pSOF_node)
    {
        vogl_error_printf("Failed finding SOF (start of file) packet in JSON file \"%s\"\n", m_cur_frame_filename.get_ptr());
        return false;
    }

    int meta_pointer_sizes = pSOF_node->value_as_int32("pointer_sizes", -1);
    if ((meta_pointer_sizes != sizeof(uint32_t)) && (meta_pointer_sizes != sizeof(uint64_t)))
    {
        vogl_error_printf("Invalid meta pointer sizes field in JSON file \"%s\"\n", m_cur_frame_filename.get_ptr());
        return false;
    }

    dynamic_string archive_filename = pSOF_node->value_as_string("archive_filename");
    if (!archive_filename.is_empty())
    {
        dynamic_string full_archive_filename(archive_filename);

        // Determine if the archive path is absolute or not
        if (!full_archive_filename.contains('/') && !full_archive_filename.contains('\\'))
        {
            file_utils::combine_path(full_archive_filename, m_drive.get_ptr(), m_dir.get_ptr(), archive_filename.get_ptr());
        }

        if (!m_archive_blob_manager.init_file(cBMFReadable | cBMFOpenExisting, full_archive_filename.get_ptr()))
        {
            vogl_error_printf("JSON trace relies on archive \"%s\", which cannot be opened! Will try to read anyway, but later operations may fail if loose files cannot be found.\n", archive_filename.get_ptr());
            // Don't immediately exit in case they have manually deleted the archive and want everything to read from loose files.
            //return false;
        }
    }

    uint64_t trace_version = pSOF_node->value_as_uint64("version");

    m_sof_packet.init();
    m_sof_packet.m_pointer_sizes = meta_pointer_sizes;
    m_sof_packet.m_version = safe_int_cast<uint16_t>(trace_version);

    if (m_archive_blob_manager.is_initialized())
    {
        file_utils::get_file_size(m_archive_blob_manager.get_archive_filename().get_ptr(), m_sof_packet.m_archive_size);
    }

    const json_node *pUUID_array = pSOF_node->find_child_array("uuid");
    if (pUUID_array)
    {
        for (uint32_t i = 0; i < math::minimum<uint32_t>(pUUID_array->size(), vogl_trace_stream_start_of_file_packet::cUUIDSize); i++)
            m_sof_packet.m_uuid[i] = pUUID_array->value_as_uint32(i);
    }

    // Makes no sense to finalize it, it's not complete or valid.
    //m_sof_packet.finalize();

    m_trace_ctypes.change_pointer_sizes(meta_pointer_sizes);

    return true;
}

bool vogl_json_trace_file_reader::read_document(const dynamic_string &filename)
{
    VOGL_FUNC_TRACER

    m_cur_frame_filename = filename;

    m_cur_packet_node_index = 0;
    m_packet_node_size = 0;
    m_doc_eof_key_value = 0;
    m_pPackets_array = NULL;
    m_cur_doc.clear();

    bool deserialize_status = false;

    // HACK HACK: to work around another app writing to the file as we try to read it in -endless mode
    const uint32_t cMaxRetries = 5;
    for (uint32_t tries = 0; tries < cMaxRetries; tries++)
    {
        if (!m_trace_stream.open(m_cur_frame_filename.get_ptr(), cDataStreamReadable, true))
        {
            vogl_error_printf("Could not open JSON trace file \"%s\"\n", m_cur_frame_filename.get_ptr());
            return false;
        }

        deserialize_status = m_cur_doc.deserialize_file(m_cur_frame_filename.get_ptr());

        m_trace_stream.close();

        if (deserialize_status)
            break;

        // Sleep a while in case another app is writing to the file
        vogl_sleep(500);
    }

    if (!deserialize_status)
    {
        if (m_cur_doc.get_error_msg().has_content())
            vogl_error_printf("Failed deserializing JSON file \"%s\"!\nError: %s Line: %u\n", m_cur_frame_filename.get_ptr(), m_cur_doc.get_error_msg().get_ptr(), m_cur_doc.get_error_line());
        else
            vogl_error_printf("Failed deserializing JSON file \"%s\"!\n", m_cur_frame_filename.get_ptr());

        m_cur_doc.clear();
        return false;
    }

    vogl_message_printf("Processing JSON file \"%s\"\n", m_cur_frame_filename.get_ptr());

    const json_node *pRoot_node = m_cur_doc.get_root();
    if (!pRoot_node)
    {
        vogl_error_printf("Couldn't find root node in JSON file \"%s\"\n", m_cur_frame_filename.get_ptr());
        m_cur_doc.clear();
        return false;
    }

    const json_node *pMeta_node = pRoot_node->find_child("meta");
    if (!pMeta_node)
    {
        vogl_error_printf("Couldn't find meta node in JSON file \"%s\"\n", m_cur_frame_filename.get_ptr());
        m_cur_doc.clear();
        return false;
    }

    int64_t meta_frame_index = pMeta_node->value_as_int64("cur_frame", -1);
    if (meta_frame_index != m_cur_frame_index)
    {
        vogl_error_printf("Invalid meta frame index in JSON file \"%s\" (expected %lli, got %lli)\n", m_cur_frame_filename.get_ptr(), static_cast<long long int>(m_cur_frame_index), static_cast<long long int>(meta_frame_index));
        m_cur_doc.clear();
        return false;
    }

    m_doc_eof_key_value = pMeta_node->value_as_int("eof", 0);

    m_pPackets_array = pRoot_node->find_child_array("packets");
    if (!m_pPackets_array)
    {
        vogl_error_printf("Couldn't find packets node in JSON file \"%s\"\n", m_cur_frame_filename.get_ptr());
        m_cur_doc.clear();
        return false;
    }

    const json_node *pUUID_array = pMeta_node->find_child_array("uuid");
    if (pUUID_array)
    {
        uint32_t uuid[vogl_trace_stream_start_of_file_packet::cUUIDSize];
        utils::zero_object(uuid);

        for (uint32_t i = 0; i < math::minimum<uint32_t>(pUUID_array->size(), vogl_trace_stream_start_of_file_packet::cUUIDSize); i++)
            uuid[i] = pUUID_array->value_as_uint32(i);

        if (memcmp(uuid, m_sof_packet.m_uuid, sizeof(uuid)) != 0)
        {
            // Print the UUID's the way they would appear in the json for easier searching
            vogl_warning_printf("Document UUID (%u %u %u %u) in file %s differs from the UUID in the first frame's SOF packet (%u %u %u %u)\n",
                               uuid[0], uuid[1], uuid[2], uuid[3],
                               filename.get_ptr(),
                               m_sof_packet.m_uuid[0], m_sof_packet.m_uuid[1], m_sof_packet.m_uuid[2], m_sof_packet.m_uuid[3]);
        }
    }

    m_packet_node_size = m_pPackets_array->size();

    return true;
}

void vogl_json_trace_file_reader::close()
{
    VOGL_FUNC_TRACER

    vogl_trace_file_reader::close();

    m_filename.clear();
    m_base_filename.clear();
    m_filename_exists = false;
    m_filename_is_in_multiframe_form = false;

    m_drive.clear();
    m_dir.clear();
    m_fname.clear();
    m_ext.clear();

    m_cur_frame_filename.clear();

    m_trace_stream.close();

    m_cur_frame_index = 0;
    m_max_frame_index = 0;

    m_cur_doc.clear();
    m_pPackets_array = NULL;
    m_cur_packet_node_index = 0;
    m_packet_node_size = 0;
    m_doc_eof_key_value = 0;

    m_at_eof = false;

    m_saved_location_stack.clear();
}

bool vogl_json_trace_file_reader::is_at_eof()
{
    VOGL_FUNC_TRACER

    if (!m_cur_doc.get_root())
        return true;

    return (m_at_eof) ||
           (m_cur_frame_index > m_max_frame_index) ||
           ((m_doc_eof_key_value > 0) && (m_cur_packet_node_index >= m_packet_node_size));
}

dynamic_string vogl_json_trace_file_reader::compose_frame_filename()
{
    VOGL_FUNC_TRACER

    if ((m_filename_exists) && (!m_filename_is_in_multiframe_form))
        return m_filename;

    dynamic_string trial_base_name(m_fname);
    if (m_filename_is_in_multiframe_form)
        trial_base_name.shorten(7);

    dynamic_string trial_name(cVarArg, "%s_%06u", trial_base_name.get_ptr(), m_cur_frame_index);

    dynamic_string trial_filename;
    file_utils::combine_path_and_extension(trial_filename, m_drive.get_ptr(), m_dir.get_ptr(), trial_name.get_ptr(), m_ext.get_ptr());

    return trial_filename;
}

bool vogl_json_trace_file_reader::seek_to_frame(uint32_t frame_index)
{
    VOGL_FUNC_TRACER

    // Special case: It's OK to seek to the very beginning of the frame just beyond the max valid JSON frame, to emulate how the binary read works.
    if (frame_index == (m_max_frame_index + 1))
    {
        m_cur_frame_index = m_max_frame_index;

        if (!read_document(compose_frame_filename()))
            return false;

        m_cur_packet_node_index = m_packet_node_size;

        m_at_eof = true;

        return true;
    }
    else if (frame_index > m_max_frame_index)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    m_cur_frame_index = frame_index;

    if (!read_document(compose_frame_filename()))
        return false;

    m_at_eof = false;

    return true;
}

vogl_trace_file_reader::trace_file_reader_status_t vogl_json_trace_file_reader::read_next_packet()
{
    VOGL_FUNC_TRACER

    if (!m_pPackets_array)
    {
        VOGL_ASSERT_ALWAYS;
        return cFailed;
    }

    if ((m_at_eof) || (m_cur_frame_index > m_max_frame_index))
    {
        create_eof_packet();
        return cEOF;
    }

    for (;;)
    {
        while (m_cur_packet_node_index >= m_packet_node_size)
        {
            if (m_doc_eof_key_value > 0)
            {
                create_eof_packet();
                m_at_eof = true;
                return cEOF;
            }

            if (m_cur_frame_index <= m_max_frame_index)
                m_cur_frame_index++;

            if (m_cur_frame_index > m_max_frame_index)
            {
                vogl_warning_printf("Last JSON document %u did not have a non-zero eof meta key, forcing EOF\n", m_max_frame_index);

                create_eof_packet();
                m_at_eof = true;
                return cEOF;
            }

            if (!read_document(compose_frame_filename()))
                return cFailed;
        }

        const json_node *pGL_node = m_pPackets_array->get_value_as_object(m_cur_packet_node_index);
        if (!pGL_node)
        {
            vogl_warning_printf("Ignoring invalid JSON key %s, file \"%s\"\n", m_pPackets_array->get_path_to_item(m_cur_packet_node_index).get_ptr(), m_cur_frame_filename.get_ptr());
            m_cur_packet_node_index++;
            continue;
        }

        bool success = m_trace_packet.json_deserialize(*pGL_node, m_cur_frame_filename.get_ptr(), &m_multi_blob_manager);
        if (!success)
        {
            if (m_cur_doc.get_error_msg().has_content())
                vogl_error_printf("Failed deserializing JSON file \"%s\"!\nError: %s Line: %u\n", m_cur_frame_filename.get_ptr(), m_cur_doc.get_error_msg().get_ptr(), m_cur_doc.get_error_line());
            else
                vogl_error_printf("Failed deserializing JSON file \"%s\"!\n", m_cur_frame_filename.get_ptr());
            return cFailed;
        }

        m_dyn_stream.reset();
        m_dyn_stream.open();

        if (!m_trace_packet.serialize(m_dyn_stream))
        {
            vogl_error_printf("Failed serializing binary trace packet data while processing JSON file \"%s\"!\n", m_cur_frame_filename.get_ptr());
            return cFailed;
        }

        m_dyn_stream.get_buf().swap(m_packet_buf);

        m_cur_packet_node_index++;

        break;
    }

    return m_at_eof ? cEOF : cOK;
}

bool vogl_is_multiframe_json_trace_filename(const char *pFilename)
{
    VOGL_FUNC_TRACER

    dynamic_string drive, dir, fname, ext;
    if (!file_utils::split_path(pFilename, &drive, &dir, &fname, &ext))
        return false;

    if ((fname.size() >= 7) && (fname.get_char_at_end(6) == '_'))
    {
        for (int i = 0; i < 6; i++)
            if (!vogl_isdigit(fname.get_char_at_end(i)))
                return false;
        return true;
    }

    return false;
}

bool vogl_json_trace_file_reader::push_location()
{
    VOGL_FUNC_TRACER

    if (!m_pPackets_array)
        return false;

    saved_location *p = m_saved_location_stack.enlarge(1);
    p->m_cur_frame_filename = m_cur_frame_filename;
    p->m_cur_frame_index = m_cur_frame_index;
    p->m_cur_packet_node_index = m_cur_packet_node_index;
    p->m_at_eof = m_at_eof;

    return true;
}

bool vogl_json_trace_file_reader::pop_location()
{
    VOGL_FUNC_TRACER

    if ((!m_pPackets_array) || (m_saved_location_stack.is_empty()))
        return false;

    saved_location &loc = m_saved_location_stack.back();

    bool success = true;

    if ((loc.m_cur_frame_filename != m_cur_frame_filename) || (loc.m_cur_frame_index != m_cur_frame_index))
    {
        m_cur_frame_index = loc.m_cur_frame_index;

        if (!read_document(loc.m_cur_frame_filename))
            success = false;
    }

    if (success)
    {
        m_cur_frame_index = loc.m_cur_frame_index;

        m_at_eof = loc.m_at_eof;

        if (loc.m_cur_packet_node_index > m_pPackets_array->size())
            success = false;
        else
            m_cur_packet_node_index = loc.m_cur_packet_node_index;
    }

    m_saved_location_stack.pop_back();

    return success;
}

vogl_trace_file_reader_type_t vogl_determine_trace_file_type(const dynamic_string &orig_filename, dynamic_string &filename_to_use)
{
    VOGL_FUNC_TRACER

    dynamic_string trace_extension(orig_filename);
    file_utils::get_extension(trace_extension);

    vogl_trace_file_reader_type_t trace_reader_type = cINVALID_TRACE_FILE_READER;

    if (trace_extension == "bin")
    {
        trace_reader_type = cBINARY_TRACE_FILE_READER;
        filename_to_use = orig_filename;
    }
    else if (trace_extension == "json")
    {
        trace_reader_type = cJSON_TRACE_FILE_READER;
        filename_to_use = orig_filename;
    }
    else if (file_utils::does_file_exist(orig_filename.get_ptr()))
    {
        FILE *pFile = vogl_fopen(orig_filename.get_ptr(), "rb");
        if (pFile)
        {
            int first_char = fgetc(pFile);
            if (first_char == '{')
            {
                trace_reader_type = cJSON_TRACE_FILE_READER;
                filename_to_use = orig_filename;
            }
            else if (first_char != EOF)
            {
                vogl_trace_stream_packet_base first_packet;
                utils::zero_object(first_packet);

                *reinterpret_cast<char *>(&first_packet) = static_cast<char>(first_char);

                if (fread(reinterpret_cast<char *>(&first_packet) + 1, sizeof(first_packet) - 1, 1, pFile) == 1)
                {
                    if ((first_packet.m_prefix == vogl_trace_stream_packet_base::cTracePacketPrefix) && (first_packet.m_type == cTSPTSOF))
                    {
                        trace_reader_type = cBINARY_TRACE_FILE_READER;
                        filename_to_use = orig_filename;
                    }
                }
            }

            vogl_fclose(pFile);
        }
    }

    if (trace_reader_type == cINVALID_TRACE_FILE_READER)
    {
        dynamic_string trial_filename(cVarArg, "%s.bin", orig_filename.get_ptr());
        if (file_utils::does_file_exist(trial_filename.get_ptr()))
        {
            trace_reader_type = cBINARY_TRACE_FILE_READER;
            filename_to_use = trial_filename;
        }
        else
        {
            trial_filename.format("%s.json", orig_filename.get_ptr());
            if (file_utils::does_file_exist(trial_filename.get_ptr()))
            {
                trace_reader_type = cJSON_TRACE_FILE_READER;
                filename_to_use = trial_filename;
            }
            else
            {
                trial_filename.format("%s_000000.json", orig_filename.get_ptr());
                if (file_utils::does_file_exist(trial_filename.get_ptr()))
                {
                    trace_reader_type = cJSON_TRACE_FILE_READER;
                    filename_to_use = trial_filename;
                }
            }
        }
    }

    return trace_reader_type;
}

vogl_trace_file_reader *vogl_create_trace_file_reader(vogl_trace_file_reader_type_t trace_type)
{
    VOGL_FUNC_TRACER

    switch (trace_type)
    {
        case cBINARY_TRACE_FILE_READER:
            return vogl_new(vogl_binary_trace_file_reader);
            break;
        case cJSON_TRACE_FILE_READER:
            return vogl_new(vogl_json_trace_file_reader);
            break;
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }

    return NULL;
}

vogl_trace_file_reader *vogl_open_trace_file(dynamic_string &orig_filename, dynamic_string &actual_filename, const char *pLoose_file_path)
{
    VOGL_FUNC_TRACER

    vogl_trace_file_reader_type_t trace_type = vogl_determine_trace_file_type(orig_filename, actual_filename);
    if (trace_type == cINVALID_TRACE_FILE_READER)
    {
        vogl_error_printf("Unable to determine file type of trace file \"%s\"\n", orig_filename.get_ptr());
        return NULL;
    }

    vogl_trace_file_reader *pTrace_reader = vogl_create_trace_file_reader(trace_type);
    if (!pTrace_reader)
    {
        vogl_error_printf("Unable to determine file type of trace file \"%s\"\n", orig_filename.get_ptr());
        return NULL;
    }

    if (!pTrace_reader->open(actual_filename.get_ptr(), pLoose_file_path))
    {
        vogl_error_printf("Failed opening trace file \"%s\"\n", orig_filename.get_ptr());
        vogl_delete(pTrace_reader);
        return NULL;
    }

    return pTrace_reader;
}
