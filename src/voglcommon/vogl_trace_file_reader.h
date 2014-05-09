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

// File: vogl_trace_file_reader.h
#ifndef VOGL_TRACE_FILE_READER_H
#define VOGL_TRACE_FILE_READER_H

#include "vogl_common.h"
#include "vogl_trace_stream_types.h"
#include "vogl_trace_packet.h"
#include "vogl_cfile_stream.h"
#include "vogl_dynamic_stream.h"
#include "vogl_json.h"

//----------------------------------------------------------------------------------------------------------------------
// class vogl_trace_packet_array
//----------------------------------------------------------------------------------------------------------------------
typedef vogl::vector<uint8_vec> vogl_packet_buf_vec;

class vogl_trace_packet_array
{
public:
    vogl_trace_packet_array()
    {
    }

    void clear()
    {
        m_packets.clear();
    }

    void resize(uint32_t new_size)
    {
        m_packets.resize(new_size);
    }
    void reserve(uint32_t new_capacity)
    {
        m_packets.reserve(new_capacity);
    }

    uint32_t size() const
    {
        return m_packets.size();
    }
    bool is_empty() const
    {
        return m_packets.is_empty();
    }

    void push_back(const uint8_vec &packet)
    {
        m_packets.push_back(packet);
    }

    void insert(uint32_t index, const uint8_vec &packet)
    {
        m_packets.insert(index, packet);
    }

    void erase(uint32_t index)
    {
        m_packets.erase(index);
    }

    void swap(vogl_trace_packet_array &other)
    {
        m_packets.swap(other.m_packets);
    }

    const vogl_packet_buf_vec &get_vec() const
    {
        return m_packets;
    }
    vogl_packet_buf_vec &get_vec()
    {
        return m_packets;
    }

    const uint8_vec &get_packet_buf(uint32_t index) const
    {
        return m_packets[index];
    }
    uint8_vec &get_packet_buf(uint32_t index)
    {
        return m_packets[index];
    }

    template <typename T>
    inline const T &get_packet(uint32_t index) const
    {
        VOGL_ASSERT(m_packets[index].size() >= sizeof(T));
        return *reinterpret_cast<const T *>(m_packets[index].get_ptr());
    }

    inline const vogl_trace_stream_packet_base &get_base_packet(uint32_t index) const
    {
        return get_packet<vogl_trace_stream_packet_base>(index);
    }

    inline vogl_trace_stream_packet_types_t get_packet_type(uint32_t index) const
    {
        return static_cast<vogl_trace_stream_packet_types_t>(get_base_packet(index).m_type);
    }
    inline uint32_t get_packet_size(uint32_t index) const
    {
        return m_packets[index].size();
    }

    inline bool is_eof_packet(uint32_t index) const
    {
        VOGL_FUNC_TRACER

        return (get_packet_type(index) == cTSPTEOF);
    }

    inline bool is_swap_buffers_packet(uint32_t index) const
    {
        VOGL_FUNC_TRACER

        if (get_packet_type(index) == cTSPTGLEntrypoint)
        {
            const vogl_trace_gl_entrypoint_packet &gl_packet = get_packet<vogl_trace_gl_entrypoint_packet>(index);
            return vogl_is_swap_buffers_entrypoint(static_cast<gl_entrypoint_id_t>(gl_packet.m_entrypoint_id));
        }
        return false;
    }

private:
    vogl_packet_buf_vec m_packets;
};

//----------------------------------------------------------------------------------------------------------------------
// enum trace_file_reader_type_t
//----------------------------------------------------------------------------------------------------------------------
enum vogl_trace_file_reader_type_t
{
    cINVALID_TRACE_FILE_READER,
    cBINARY_TRACE_FILE_READER,
    cJSON_TRACE_FILE_READER,
    cTOTAL_TRACE_FILE_READERS
};

//----------------------------------------------------------------------------------------------------------------------
// class trace_file_reader
//----------------------------------------------------------------------------------------------------------------------
class vogl_trace_file_reader
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_trace_file_reader);

public:
    vogl_trace_file_reader()
    {
        VOGL_FUNC_TRACER

        utils::zero_object(m_sof_packet);

        m_multi_blob_manager.init(cBMFReadable | cBMFOpenExisting);

        m_multi_blob_manager.add_blob_manager(&m_loose_file_blob_manager);
        m_multi_blob_manager.add_blob_manager(&m_archive_blob_manager);
    }

    virtual ~vogl_trace_file_reader()
    {
    }

    virtual bool open(const char *pFilename, const char *pLoose_file_path) = 0;

    virtual bool is_opened() = 0;
    virtual const char *get_filename() = 0;

    virtual void close()
    {
        VOGL_FUNC_TRACER

        utils::zero_object(m_sof_packet);
        m_packet_buf.clear();
        m_loose_file_blob_manager.deinit();
        m_archive_blob_manager.deinit();
    }

    virtual vogl_trace_file_reader_type_t get_type() const = 0;

    virtual const vogl_trace_stream_start_of_file_packet &get_sof_packet() const
    {
        VOGL_FUNC_TRACER return m_sof_packet;
    }

    virtual bool is_at_eof() = 0;

    virtual bool can_quickly_seek_forward() const = 0;

    virtual uint32_t get_cur_frame() const = 0;

    // Allows random seeking, but it can be very slow to seek forward in binary traces.
    virtual bool seek_to_frame(uint32_t frame_index) = 0;

    virtual int64_t get_max_frame_index() = 0;

    virtual bool push_location() = 0;
    virtual bool pop_location() = 0;

    enum trace_file_reader_status_t
    {
        cFailed = -1,
        cOK,
        cEOF
    };

    // Notes: Be careful handling cEOF/eof packets.
    // The last packet MAY be an EOF packet, or not if the trace terminated before it could be flushed.
    // So you need to check for both the cEOF return status and if the packet is EOF.
    // Be sure to test with both JSON and binary readers because with JSON files you'll always just get a cEOF return
    // status at EOF (and never see an actual EOF packet), while with binary you can get both types of behavior.
    // Also, for binary traces: just because you get an EOF packet doesn't necessarily mean that cEOF will be returned next (the file could somehow be corrupted).
    virtual trace_file_reader_status_t read_next_packet() = 0;

    virtual trace_file_reader_status_t read_frame_packets(uint32_t frame_index, uint32_t num_frames, vogl_trace_packet_array &packets, uint32_t &actual_frames_read);

    // packet helpers

    const uint8_vec &get_packet_buf() const
    {
        return m_packet_buf;
    }

    template <typename T>
    inline const T &get_packet() const
    {
        VOGL_ASSERT(m_packet_buf.size() >= sizeof(T));
        return *reinterpret_cast<const T *>(m_packet_buf.get_ptr());
    }

    inline const vogl_trace_stream_packet_base &get_base_packet() const
    {
        return get_packet<vogl_trace_stream_packet_base>();
    }

    inline vogl_trace_stream_packet_types_t get_packet_type() const
    {
        return static_cast<vogl_trace_stream_packet_types_t>(get_base_packet().m_type);
    }
    inline uint32_t get_packet_size() const
    {
        return m_packet_buf.size();
    }

    inline bool is_eof_packet() const
    {
        VOGL_FUNC_TRACER

        return (get_packet_type() == cTSPTEOF);
    }

    inline bool is_swap_buffers_packet() const
    {
        VOGL_FUNC_TRACER

        if (get_packet_type() == cTSPTGLEntrypoint)
        {
            const vogl_trace_gl_entrypoint_packet &gl_packet = get_packet<vogl_trace_gl_entrypoint_packet>();
            return vogl_is_swap_buffers_entrypoint(static_cast<gl_entrypoint_id_t>(gl_packet.m_entrypoint_id));
        }
        return false;
    }

    const vogl_multi_blob_manager &get_multi_blob_manager() const
    {
        return m_multi_blob_manager;
    }
    vogl_multi_blob_manager &get_multi_blob_manager()
    {
        return m_multi_blob_manager;
    }

    const vogl_loose_file_blob_manager &get_loose_file_blob_manager() const
    {
        return m_loose_file_blob_manager;
    }
    vogl_loose_file_blob_manager &get_loose_file_blob_manager()
    {
        return m_loose_file_blob_manager;
    }

    const vogl_archive_blob_manager &get_archive_blob_manager() const
    {
        return m_archive_blob_manager;
    }
    vogl_archive_blob_manager &get_archive_blob_manager()
    {
        return m_archive_blob_manager;
    }

protected:
    vogl_trace_stream_start_of_file_packet m_sof_packet;

    uint8_vec m_packet_buf;

    vogl_loose_file_blob_manager m_loose_file_blob_manager;
    vogl_archive_blob_manager m_archive_blob_manager;
    vogl_multi_blob_manager m_multi_blob_manager;

    void create_eof_packet();
    bool init_loose_file_blob_manager(const char *pTrace_filename, const char *pLoose_file_path);
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_scoped_location_saver
//----------------------------------------------------------------------------------------------------------------------
class vogl_scoped_location_saver
{
    vogl_trace_file_reader &m_reader;

public:
    vogl_scoped_location_saver(vogl_trace_file_reader &reader)
        : m_reader(reader)
    {
        m_reader.push_location();
    }
    ~vogl_scoped_location_saver()
    {
        m_reader.pop_location();
    }
};

//----------------------------------------------------------------------------------------------------------------------
// class binary_trace_file_reader
//----------------------------------------------------------------------------------------------------------------------
class vogl_binary_trace_file_reader : public vogl_trace_file_reader
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_binary_trace_file_reader);

public:
    vogl_binary_trace_file_reader();

    const data_stream &get_stream() const
    {
        return m_trace_stream;
    }
    data_stream &get_stream()
    {
        return m_trace_stream;
    }

    virtual ~vogl_binary_trace_file_reader();

    virtual bool open(const char *pFilename, const char *pLoose_file_path);

    virtual bool is_opened();
    virtual const char *get_filename();

    virtual void close();

    virtual vogl_trace_file_reader_type_t get_type() const
    {
        return cBINARY_TRACE_FILE_READER;
    }

    virtual bool is_at_eof();

    virtual bool can_quickly_seek_forward() const
    {
        return m_found_frame_file_offsets_packet;
    }

    virtual uint32_t get_cur_frame() const
    {
        return m_cur_frame_index;
    }

    virtual bool seek_to_frame(uint32_t frame_index);

    virtual int64_t get_max_frame_index();

    virtual bool push_location();
    virtual bool pop_location();

    uint64_t get_trace_file_size() const
    {
        return m_trace_file_size;
    }
    inline uint64_t get_cur_file_ofs()
    {
        return m_trace_stream.get_ofs();
    }
    inline bool seek(uint64_t new_ofs)
    {
        return m_trace_stream.seek(new_ofs, false);
    }

    virtual trace_file_reader_status_t read_next_packet();

private:
    cfile_stream m_trace_stream;
    uint64_t m_trace_file_size;

    uint32_t m_cur_frame_index;
    int64_t m_max_frame_index;
    vogl::vector<uint64_t> m_frame_file_offsets;

    struct saved_location
    {
        uint32_t m_cur_frame_index;
        uint64_t m_cur_ofs;
    };

    vogl::vector<saved_location> m_saved_location_stack;

    bool read_frame_file_offsets();

    bool m_found_frame_file_offsets_packet;
};

//----------------------------------------------------------------------------------------------------------------------
// class json_trace_file_reader
//----------------------------------------------------------------------------------------------------------------------
class vogl_json_trace_file_reader : public vogl_trace_file_reader
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_json_trace_file_reader);

public:
    vogl_json_trace_file_reader();

    virtual ~vogl_json_trace_file_reader();

    virtual bool open(const char *pFilename, const char *pLoose_file_path);

    virtual bool is_opened();
    virtual const char *get_filename();

    virtual void close();

    virtual vogl_trace_file_reader_type_t get_type() const
    {
        return cJSON_TRACE_FILE_READER;
    }

    virtual bool is_at_eof();

    virtual bool can_quickly_seek_forward() const
    {
        return true;
    }

    virtual uint32_t get_cur_frame() const
    {
        return m_cur_frame_index;
    }

    virtual int64_t get_max_frame_index()
    {
        return m_max_frame_index;
    }

    virtual bool seek_to_frame(uint32_t frame_index);

    virtual trace_file_reader_status_t read_next_packet();

    virtual bool push_location();
    virtual bool pop_location();

private:
    dynamic_string m_filename;
    dynamic_string m_base_filename;
    bool m_filename_exists;
    bool m_filename_is_in_multiframe_form;

    dynamic_string m_drive;
    dynamic_string m_dir;
    dynamic_string m_fname;
    dynamic_string m_ext;

    dynamic_string m_cur_frame_filename;

    cfile_stream m_trace_stream;

    uint32_t m_cur_frame_index;
    uint32_t m_max_frame_index;

    json_document m_cur_doc;
    const json_node *m_pPackets_array;
    uint32_t m_cur_packet_node_index;
    uint32_t m_packet_node_size;
    int m_doc_eof_key_value;

    bool m_at_eof;

    vogl_ctypes m_trace_ctypes;
    vogl_trace_packet m_trace_packet;
    dynamic_stream m_dyn_stream;

    struct saved_location
    {
        dynamic_string m_cur_frame_filename;
        uint32_t m_cur_frame_index;
        uint32_t m_cur_packet_node_index;
        bool m_at_eof;
    };

    vogl::vector<saved_location> m_saved_location_stack;

    dynamic_string compose_frame_filename();
    bool read_document(const dynamic_string &filename);
    bool open_first_document();
};

bool vogl_is_multiframe_json_trace_filename(const char *pFilename);
vogl_trace_file_reader_type_t vogl_determine_trace_file_type(const dynamic_string &orig_filename, dynamic_string &filename_to_use);
vogl_trace_file_reader *vogl_create_trace_file_reader(vogl_trace_file_reader_type_t trace_type);
vogl_trace_file_reader *vogl_open_trace_file(dynamic_string &orig_filename, dynamic_string &actual_filename, const char *pLoose_file_path);

#endif // VOGL_TRACE_FILE_READER_H
