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

// File: vogl_trace_stream_types.h
#ifndef VOGL_TRACE_STREAM_TYPES_H
#define VOGL_TRACE_STREAM_TYPES_H
#include "vogl_miniz.h"
#include "vogl_port.h"

#define VOGL_TRACE_FILE_VERSION 0x0107
#define VOGL_TRACE_FILE_MINIMUM_COMPATIBLE_VERSION 0x0107

#define VOGL_TRACE_LINK_PROGRAM_UNIFORM_DESC_KEY_OFS 0xF0000

#pragma pack(push, 1)
enum vogl_trace_stream_packet_types_t
{
    cTSPTSOF = 1,
    cTSPTGLEntrypoint = 3,
    cTSPTEOF = 4,
    cTSPTTotalTypes
};

struct vogl_trace_stream_packet_base
{
    enum
    {
        cTracePacketPrefix = 0xD1C71602
    };
    uint32_t m_prefix;
    uint32_t m_size; // total size, including extra data, needed to get to next packet_base
    uint32_t m_crc;  // CRC32 of all packet data following this member

    enum
    {
        cMinimumPossibleRnd = 1
    };
    uint16_t m_rnd; // a random number, cannot be 0, must immediately follow m_crc!

    uint64_t m_packet_begin_rdtsc;
    uint64_t m_gl_begin_rdtsc;
    uint64_t m_gl_end_rdtsc;
    uint64_t m_packet_end_rdtsc;

    uint8_t m_type;     // trace_packet_types_t
    uint16_t m_inv_rnd; // not of m_rnd, for verification (and to purpusely increase the packet entropy - this is only for devel)

    inline void init(uint8_t type, uint32_t size)
    {
        memset(this, 0, sizeof(*this));

        m_prefix = cTracePacketPrefix;
        m_type = type;
        m_size = size;
        m_rnd = cMinimumPossibleRnd;
    }

    // Sets the rnd field using get_random()
    inline void init_rnd()
    {
        uint32_t rnd16 = static_cast<uint16_t>(plat_rand());
        m_rnd = static_cast<uint16_t>(math::maximum<uint32_t>(vogl_trace_stream_packet_base::cMinimumPossibleRnd, rnd16));
        VOGL_ASSERT(m_rnd);
    }

    // Assumes the *entire* packet is sequential in memory (i.e. m_size bytes starting at this)
    inline void finalize()
    {
        m_inv_rnd = ~m_rnd;

        VOGL_ASSERT(m_size >= sizeof(vogl_trace_stream_packet_base));

        m_crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const uint8_t *>(this) + (uint32_t)VOGL_OFFSETOF(vogl_trace_stream_packet_base, m_rnd), m_size - (uint32_t)VOGL_OFFSETOF(vogl_trace_stream_packet_base, m_rnd));
    }

    inline bool basic_validation() const
    {
        if (m_prefix != cTracePacketPrefix)
            return false;

        if (m_type >= cTSPTTotalTypes)
            return false;

        if (m_size < sizeof(vogl_trace_stream_packet_base))
            return false;

        VOGL_ASSUME(sizeof(m_rnd) == sizeof(uint16_t));
        uint32_t inv_rnd = (~m_rnd) & 0xFFFFU;

        if ((m_rnd < static_cast<uint32_t>(cMinimumPossibleRnd)) || (inv_rnd != static_cast<uint32_t>(m_inv_rnd)))
            return false;

        return true;
    }

    // Assumes the *entire* packet is sequential in memory (i.e. m_size bytes starting at this)
    inline bool check_crc(uint32_t actual_buf_size) const
    {
        if (m_size < sizeof(vogl_trace_stream_packet_base))
            return false;

        if (m_size > actual_buf_size)
            return false;

        uint32_t check_crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const uint8_t *>(this) + (uint32_t)VOGL_OFFSETOF(vogl_trace_stream_packet_base, m_rnd), m_size - (uint32_t)VOGL_OFFSETOF(vogl_trace_stream_packet_base, m_rnd));
        if (check_crc != m_crc)
            return false;

        return true;
    }

    // Assumes the *entire* packet is sequential in memory (i.e. m_size bytes starting at this)
    inline bool full_validation(uint32_t actual_buf_size) const
    {
        if (!basic_validation())
            return false;

        return check_crc(actual_buf_size);
    }
};

struct vogl_trace_stream_start_of_file_packet
{
    enum
    {
        cSOFTracePacketPrefix = 0xD1C71601
    };
    uint32_t m_prefix;
    uint64_t m_size; // The size of this packet (NOT including the archive data)
    uint32_t m_crc;  // CRC32 of all packet data following this member

    uint16_t m_version; // must immediately follow m_crc!
    uint8_t m_pointer_sizes;
    uint8_t m_unused;

    enum
    {
        cUUIDSize = 4
    };
    uint32_t m_uuid[cUUIDSize];

    uint64_t m_first_packet_offset;

    // File offset and size of all ZIP archive data immediately following this packet, or 0 if there is no archive data.
    uint64_t m_archive_offset;
    uint64_t m_archive_size;

    inline void init()
    {
        memset(this, 0, sizeof(*this));
        m_size = sizeof(*this);
        m_pointer_sizes = sizeof(void *);
        m_version = VOGL_TRACE_FILE_VERSION;
    }

    uint32_t compute_crc() const
    {
        return (uint32_t)mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const uint8_t *>(this) + (uint32_t)VOGL_OFFSETOF(vogl_trace_stream_start_of_file_packet, m_version), sizeof(*this) - (uint32_t)VOGL_OFFSETOF(vogl_trace_stream_start_of_file_packet, m_version));
    }

    inline void finalize()
    {
        m_prefix = cSOFTracePacketPrefix;
        m_crc = compute_crc();
    }

    inline bool basic_validation() const
    {
        if (m_prefix != cSOFTracePacketPrefix)
            return false;

        if (m_size != sizeof(vogl_trace_stream_start_of_file_packet))
            return false;

        if (m_version < VOGL_TRACE_FILE_MINIMUM_COMPATIBLE_VERSION)
            return false;

        return true;
    }

    inline bool check_crc(uint32_t actual_buf_size) const
    {
        if (m_size != sizeof(vogl_trace_stream_start_of_file_packet))
            return false;
        if (actual_buf_size < sizeof(vogl_trace_stream_start_of_file_packet))
            return false;

        uint32_t check_crc = compute_crc();
        if (check_crc != m_crc)
            return false;

        return true;
    }

    inline bool full_validation(uint32_t actual_buf_size) const
    {
        if (!basic_validation())
            return false;
        return check_crc(actual_buf_size);
    }
};

#define VOGL_RETURN_PARAM_INDEX 255

// GL entrypoint packets contain a fixed size struct vogl_trace_gl_entrypoint_packet, immediately
// followed by three variable sized data blobs:
//
// struct vogl_trace_gl_entrypoint_packet contains the base data in vogl_trace_stream_packet_base (total
// size of packet, crc of entire packet, various RDTSC's), and stuff like the entrypoint ID/context
// handle/kernel thread ID/call counter/etc.
//
// Immediately following this struct are up to three variable length blobs of data: The parameter
// data, any referenced client memory data, and an optional name value map.
struct vogl_trace_gl_entrypoint_packet : vogl_trace_stream_packet_base
{
    uint16_t m_entrypoint_id;

    uint64_t m_context_handle;
    uint64_t m_call_counter;
    uint64_t m_thread_id;

    uint16_t m_param_size;
    uint32_t m_client_memory_size;

    uint32_t m_backtrace_hash_index;

    // The code that handles serialization is written assuming most packets will *not* use a name value map.
    // Maps are intended to be rare events that are only included on expensive GL/GLX calls.
    uint32_t m_name_value_map_size;

    inline void init()
    {
        vogl_trace_stream_packet_base::init(cTSPTGLEntrypoint, sizeof(*this));
    }
};
#pragma pack(pop)

enum vogl_internal_trace_command_rad_cmd_types_t
{
    cITCRDemarcation = 0,
    cITCRKeyValueMap = 1,
    cITCRTotalTypes
};

// TODO: Is this the best location for this?
struct vogl_backtrace_addrs
{
    enum
    {
        cMaxAddrs = 64
    };
    uint32_t m_num_addrs;
    uintptr_t m_addrs[cMaxAddrs];

    void clear()
    {
        utils::zero_object(*this);
    }

    bool operator==(const vogl_backtrace_addrs &rhs) const
    {
        if (m_num_addrs != rhs.m_num_addrs)
            return false;
        return memcmp(m_addrs, rhs.m_addrs, m_num_addrs * sizeof(m_addrs[0])) == 0;
    }

    bool operator!=(const vogl_backtrace_addrs &rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const vogl_backtrace_addrs &rhs) const
    {
        if (m_num_addrs < rhs.m_num_addrs)
            return true;
        else if (m_num_addrs == rhs.m_num_addrs)
        {
            for (uint32_t i = 0; i < m_num_addrs; i++)
            {
                if (m_addrs[i] < rhs.m_addrs[i])
                    return true;
                else if (m_addrs[i] > rhs.m_addrs[i])
                    return false;
            }
        }

        return false;
    }

    uint32_t get_hash() const
    {
        return vogl::fast_hash(&m_num_addrs, sizeof(m_num_addrs)) ^ vogl::fast_hash(m_addrs, m_num_addrs * sizeof(m_addrs[0]));
    }
};

typedef vogl::hash_map<vogl_backtrace_addrs, uint64_t, intrusive_hasher<vogl_backtrace_addrs> > vogl_backtrace_hashmap;

#define VOGL_TRACE_ARCHIVE_FRAME_FILE_OFFSETS_FILENAME   "frame_file_offsets"

#define VOGL_TRACE_ARCHIVE_COMPILER_INFO_FILENAME        "compiler_info.json"
#define VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME         "machine_info.json"
#define VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_SYMS_FILENAME   "backtrace_map_syms.json"
#define VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME  "backtrace_map_addrs.json"

#endif // VOGL_TRACE_STREAM_TYPES_H
