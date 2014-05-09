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
// File: vogl_trace_file_writer.h
//----------------------------------------------------------------------------------------------------------------------
#ifndef VOGL_TRACE_FILE_WRITER_H
#define VOGL_TRACE_FILE_WRITER_H

#include "vogl_common.h"
#include "vogl_trace_stream_types.h"
#include "vogl_trace_packet.h"
#include "vogl_cfile_stream.h"
#include "vogl_dynamic_stream.h"
#include "vogl_json.h"
#include "vogl_unique_ptr.h"

//----------------------------------------------------------------------------------------------------------------------
// vogl_trace_file_writer
//----------------------------------------------------------------------------------------------------------------------
class vogl_trace_file_writer
{
public:
    vogl_trace_file_writer(const vogl_ctypes *pCTypes);
    ~vogl_trace_file_writer();

    inline bool is_opened() const
    {
        return m_stream.is_opened();
    }
    inline const dynamic_string &get_filename() const
    {
        return m_filename;
    }

    inline data_stream &get_stream()
    {
        return m_stream;
    }

    inline vogl_archive_blob_manager *get_trace_archive()
    {
        return m_pTrace_archive.get();
    }

    // pTrace_archive may be NULL. Takes ownership of pTrace_archive.
    // TODO: Get rid of the demarcation packet, etc. Make the initial sequence of packets more explicit.
    bool open(const char *pFilename, vogl_archive_blob_manager *pTrace_archive = NULL, bool delete_archive = true, bool write_demarcation_packet = true, uint32_t pointer_sizes = sizeof(void *));

    inline uint64_t get_cur_gl_call_counter()
    {
        return m_gl_call_counter;
    }

    inline uint64_t get_next_gl_call_counter()
    {
        VOGL_FUNC_TRACER

        atomic64_t ctr;
        for (;;)
        {
            atomic64_t cur_ctr = m_gl_call_counter;
            ctr = atomic_compare_exchange64(&m_gl_call_counter, cur_ctr + 1, cur_ctr);
            if (ctr == cur_ctr)
                break;
        }
        return ctr;
    }

    inline bool write_packet(const vogl_trace_packet &packet)
    {
        VOGL_FUNC_TRACER

        if (!m_stream.is_opened())
            return false;

        if (!packet.serialize(m_stream))
            return false;

        if (vogl_is_swap_buffers_entrypoint(packet.get_entrypoint_id()))
            m_frame_file_offsets.push_back(m_stream.get_ofs());

        return true;
    }

    inline bool write_packet(const void *pPacket, uint32_t packet_size, bool is_swap)
    {
        VOGL_FUNC_TRACER

        if (!m_stream.is_opened())
            return false;

        if (m_stream.write(pPacket, packet_size) != packet_size)
            return false;

        if (is_swap)
            m_frame_file_offsets.push_back(m_stream.get_ofs());

        return true;
    }

    inline bool flush()
    {
        VOGL_FUNC_TRACER

        return m_stream.flush();
    }

    bool close();

private:
    atomic64_t m_gl_call_counter;

    const vogl_ctypes *m_pCTypes;
    dynamic_string m_filename;
    cfile_stream m_stream;

    vogl_unique_ptr<vogl_archive_blob_manager> m_pTrace_archive;
    bool m_delete_archive;

    vogl_trace_stream_start_of_file_packet m_sof_packet;

    vogl::vector<uint64_t> m_frame_file_offsets;

    void write_ctypes_packet();

    void write_entrypoints_packet();

    bool write_eof_packet();

    bool write_frame_file_offsets_to_archive();

    void close_archive(const char *pArchive_filename);
};

#endif // VOGL_TRACE_FILE_WRITER_H
