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

// File: vogl_gl_replayer.h
#ifndef VOGL_GL_REPLAYER_H
#define VOGL_GL_REPLAYER_H

#include "vogl_unique_ptr.h"

#include "vogl_common.h"
#include "vogl_trace_stream_types.h"
#include "vogl_trace_packet.h"
#include "vogl_trace_file_reader.h"
#include "vogl_context_info.h"

#include "vogl_replay_window.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_blob_manager.h"
#include "vogl_fs_preprocessor.h"

// TODO: Make this a command line param
#define VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE (8U * 1024U * 1024U)

bool vogl_process_internal_trace_command_ctypes_packet(const key_value_map &kvm, const vogl_ctypes &ctypes);
vogl_void_func_ptr_t vogl_get_proc_address_helper(const char *pName);
bool load_gl();

//----------------------------------------------------------------------------------------------------------------------
// enum vogl_gl_replayer_flags
//----------------------------------------------------------------------------------------------------------------------
enum vogl_gl_replayer_flags
{
    cGLReplayerSnapshotCaching = 0x00000001, // cache last X state snapshots
    cGLReplayerBenchmarkMode = 0x00000002,   // disable all glGetError()'s (*excluding* calls to glGetError in vogl_utils.cpp), disable all divergence checks
    cGLReplayerVerboseMode = 0x00000004,
    cGLReplayerForceDebugContexts = 0x00000008,
    cGLReplayerDumpAllPackets = 0x00000010,
    cGLReplayerDebugMode = 0x00000020,
    cGLReplayerLockWindowDimensions = 0x00000040, // prevents replayer from ever changing the window dimensions (or waiting for the window to resize)
    cGLReplayerLowLevelDebugMode = 0x00000080,    // super low-level replayer debugging, slow
    cGLReplayerDumpPacketBlobFilesOnError = 0x00000100,
    cGLReplayerDumpShadersOnDraw = 0x00000200,
    cGLReplayerDumpFramebufferOnDraws = 0x00000400,
    cGLReplayerDumpPacketsOnError = 0x00000800,
    cGLReplayerDumpScreenshots = 0x00001000,
    cGLReplayerHashBackbuffer = 0x00002000,
    cGLReplayerDumpBackbufferHashes = 0x00004000,
    cGLReplayerSumHashing = 0x00008000,
    cGLReplayerClearUnintializedBuffers = 0x00010000,
    cGLReplayerDisableRestoreFrontBuffer = 0x00020000,
    cGLReplayerFSPreprocessor = 0x00040000, // run FSs through preprocessor prior to glShaderSource call
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_replayer
//----------------------------------------------------------------------------------------------------------------------
class vogl_gl_replayer
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_gl_replayer);

    typedef vogl_trace_ptr_value vogl_trace_context_ptr_value;
    typedef vogl_trace_ptr_value vogl_sync_ptr_value;

#define process_entrypoint_message(...) process_entrypoint_(VOGL_FUNCTION_INFO_CSTR, cMsgMessage, __VA_ARGS__)
#define process_entrypoint_warning(...) process_entrypoint_(VOGL_FUNCTION_INFO_CSTR, cMsgWarning, __VA_ARGS__)
#define process_entrypoint_error(...) process_entrypoint_(VOGL_FUNCTION_INFO_CSTR, cMsgError, __VA_ARGS__)

public:
    vogl_gl_replayer();
    ~vogl_gl_replayer();

    // The window must be opened before calling init().
    bool init(uint32_t flags, vogl_replay_window *pWindow, const vogl_trace_stream_start_of_file_packet &sof_packet, const vogl_blob_manager &blob_manager);

    void deinit();

    // Configuration: call after init().

    void set_flags(uint32_t flags)
    {
        m_flags = flags;
    }
    uint32_t get_flags() const
    {
        return m_flags;
    }

    void set_swap_sleep_time(uint32_t swap_sleep_time)
    {
        m_swap_sleep_time = swap_sleep_time;
    }
    uint32_t get_swap_sleep_time() const
    {
        return m_swap_sleep_time;
    }

    const dynamic_string &get_dump_framebuffer_on_draw_prefix() const
    {
        return m_dump_framebuffer_on_draw_prefix;
    }
    void set_dump_framebuffer_on_draw_prefix(const dynamic_string &str)
    {
        m_dump_framebuffer_on_draw_prefix = str;
    }

    const dynamic_string &get_screenshot_prefix() const
    {
        return m_screenshot_prefix;
    }
    void set_screenshot_prefix(const dynamic_string &str)
    {
        m_screenshot_prefix = str;
    }

    const dynamic_string &get_backbuffer_hash_filename() const
    {
        return m_backbuffer_hash_filename;
    }
    void set_backbuffer_hash_filename(const dynamic_string &str)
    {
        m_backbuffer_hash_filename = str;
    }

    void set_dump_framebuffer_on_draw_frame_index(int64_t index)
    {
        m_dump_framebuffer_on_draw_frame_index = index;
    }
    int64_t get_dump_framebuffer_on_draw_frame_index() const
    {
        return m_dump_framebuffer_on_draw_frame_index;
    }

    void set_dump_framebuffer_on_draw_first_gl_call_index(int64_t index)
    {
        m_dump_framebuffer_on_draw_first_gl_call_index = index;
    }
    int64_t get_dump_framebuffer_on_draw_first_gl_call_index() const
    {
        return m_dump_framebuffer_on_draw_first_gl_call_index;
    }

    void set_dump_framebuffer_on_draw_last_gl_call_index(int64_t index)
    {
        m_dump_framebuffer_on_draw_last_gl_call_index = index;
    }
    int64_t get_dump_framebuffer_on_draw_last_gl_call_index() const
    {
        return m_dump_framebuffer_on_draw_last_gl_call_index;
    }

    void set_fs_preprocessor(const dynamic_string &str)
    {
        m_fs_pp->set_pp(str);
    }
    void set_fs_preprocessor_options(const dynamic_string &str)
    {
        m_fs_pp->set_pp_opts(str);
    }
    void set_fs_preprocessor_prefix(const dynamic_string &str)
    {
        m_fs_pp->set_pp_prefix(str);
    }
    double get_fs_pp_time() const
    {
        return m_fs_pp->get_pp_time();
    }

    void set_allow_snapshot_restoring(bool bAllowed)
    {
        m_allow_snapshot_restoring = bAllowed;
    }

    bool is_valid() const
    {
        return m_is_valid;
    }

    vogl_replay_window *get_window()
    {
        return m_pWindow;
    }

    enum status_t
    {
        cStatusHardFailure = -3,
        cStatusSoftFailure = -2,
        cStatusGLError = -1,
        cStatusOK = 0,
        cStatusNextFrame,
        cStatusResizeWindow,
        cStatusAtEOF
    };

    void set_proc_address_helper(vogl_gl_get_proc_address_helper_func_ptr_t pGet_proc_address_helper_func, bool wrap_all_gl_calls)
    {
        m_proc_address_helper_func = pGet_proc_address_helper_func;
        m_wrap_all_gl_calls = wrap_all_gl_calls;
    }

    // Call at the beginning of every frame to do internal housekeeping related to window resizing, which requires delaying all GL calls until the window system finishes resizing the window.
    status_t process_pending_window_resize(bool *pApplied_snapshot = NULL);

    // Processes the next packet in the trace file.
    bool has_pending_packets();
    status_t process_pending_packets();
    status_t process_next_packet(const vogl_trace_packet &gl_packet);
    status_t process_next_packet(vogl_trace_file_reader &trace_reader);

    // process_frame() calls process_next_packet() in a loop until the window must be resized, or until the next frame, or until the EOF or an error occurs.
    status_t process_frame(vogl_trace_file_reader &trace_reader);

    // Resets the replayer's state: kills all contexts, the pending snapshot, etc.
    void reset_state();

    bool get_has_pending_window_resize() const
    {
        return m_pending_window_resize_width != 0;
    }
    uint32_t get_pending_window_resize_width() const
    {
        return m_pending_window_resize_width;
    }
    uint32_t get_pending_winow_resize_height() const
    {
        return m_pending_window_resize_height;
    }

    bool update_window_dimensions();

    // Will cause a screenshot of the front buffer immediately before the next swap. This doesn't take the screenshot immediately because there may not be any active contents, depending on the state of replayer.
    bool dump_frontbuffer_screenshot_before_next_swap(const dynamic_string &filename);

    uint32_t get_frame_index() const
    {
        return m_frame_index;
    }
    uint32_t get_total_swaps() const
    {
        return m_total_swaps;
    }
    int64_t get_last_parsed_call_counter() const
    {
        return m_last_parsed_call_counter;
    }
    int64_t get_last_processed_call_counter() const
    {
        return m_last_processed_call_counter;
    }

    uint64_t get_frame_draw_counter() const
    {
        return m_frame_draw_counter;
    }

    bool get_at_frame_boundary() const
    {
        return m_at_frame_boundary;
    }

    // Caller must vogl_delete the snapshot.
    vogl_gl_state_snapshot *snapshot_state(const vogl_trace_packet_array *pTrim_packets = NULL, bool optimize_snapshot = false);

    status_t begin_applying_snapshot(const vogl_gl_state_snapshot *pSnapshot, bool delete_snapshot_after_applying);
    const vogl_gl_state_snapshot *get_pending_apply_snapshot() const
    {
        return m_pPending_snapshot;
    }

    void set_frame_draw_counter_kill_threshold(uint64_t thresh)
    {
        m_frame_draw_counter_kill_threshold = thresh;
    }
    uint64_t get_frame_draw_counter_kill_threshold() const
    {
        return m_frame_draw_counter_kill_threshold;
    }

    const vogl_trace_stream_start_of_file_packet &get_sof_packet() const
    {
        return m_sof_packet;
    }

    // Will be updated after the ctypes packet is processed, otherwise it'll be invalid
    const vogl_trace_packet &get_ctypes_packet() const
    {
        return m_ctypes_packet;
    }

    // Will be first updated at init(), then updated after the ctypes packet is processed
    const vogl_ctypes &get_trace_gl_ctypes() const
    {
        return m_trace_gl_ctypes;
    }

    // If from_start_of_frame is true: Call at the very beginning of the frame to write a trim file containing trim_len frames.
    // If from_start_of_frame is false: Call at any point to write a trim file starting at the current position with the current state.
    enum write_trim_file_flags
    {
        cWriteTrimFileFromStartOfFrame = 1,
        cWriteTrimFileOptimizeSnapshot = 2
    };

    bool write_trim_file(uint32_t flags, const dynamic_string &trim_filename, uint32_t trim_len, vogl_trace_file_reader &trace_reader, dynamic_string *pSnapshot_id = NULL);

    void snapshot_backbuffer();

private:
    status_t handle_ShaderSource(GLhandleARB trace_object,
                                 GLsizei count,
                                 const vogl_client_memory_array trace_strings_glchar_ptr_array,
                                 const GLint *pTrace_lengths);

    status_t post_draw_call();

    bool dump_framebuffer(uint32_t width, uint32_t height, GLuint read_framebuffer, GLenum read_buffer, GLenum internal_format, uint32_t orig_samples, GLuint replay_texture, GLuint replay_rbo,
                          int channel_to_write, bool force_rgb, GLuint fmt, GLuint type, const char *pFilename_suffix);
    void dump_current_framebuffer();
    void dump_current_shaders();

    uint32_t m_flags;
    uint32_t m_swap_sleep_time;
    dynamic_string m_dump_framebuffer_on_draw_prefix;
    dynamic_string m_screenshot_prefix;
    dynamic_string m_backbuffer_hash_filename;
    int64_t m_dump_framebuffer_on_draw_frame_index;
    int64_t m_dump_framebuffer_on_draw_first_gl_call_index;
    int64_t m_dump_framebuffer_on_draw_last_gl_call_index;
    vogl_fs_preprocessor* m_fs_pp;

    bool m_allow_snapshot_restoring;

    dynamic_string m_dump_frontbuffer_filename;

    vogl_trace_stream_start_of_file_packet m_sof_packet;
    vogl_trace_packet m_ctypes_packet;
    uint32_t m_trace_pointer_size_in_bytes;
    uint32_t m_trace_pointer_size_in_uints;

    vogl_ctypes m_trace_gl_ctypes;

    // DO NOT refer to this packet while processing GL commands, use m_pCur_gl_packet.
    vogl_trace_packet m_temp_gl_packet;
    vogl_trace_packet m_temp2_gl_packet;
    const vogl_trace_packet *m_pCur_gl_packet;

    vogl_replay_window *m_pWindow;

    vogl_trace_packet m_pending_make_current_packet;

    uint32_t m_pending_window_resize_width, m_pending_window_resize_height;
    timer m_time_since_pending_window_resize;
    uint32_t m_pending_window_resize_attempt_counter;

    uint32_t m_frame_index;
    uint32_t m_total_swaps;
    int64_t m_last_parsed_call_counter;
    int64_t m_last_processed_call_counter;

    bool m_at_frame_boundary;

    typedef vogl::hash_map<GLuint, GLuint> gl_handle_hash_map;
    typedef vogl::hash_map<vogl_sync_ptr_value, GLsync, bit_hasher<vogl_sync_ptr_value> > gl_sync_hash_map;

    typedef vogl::hash_map<GLint, GLint> uniform_location_hash_map;
    struct glsl_program_state
    {
        // maps trace program locations to replay program locations
        uniform_location_hash_map m_uniform_locations;
    };
    typedef vogl::hash_map<GLuint, glsl_program_state> glsl_program_hash_map;

    class context_state
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(context_state);

    public:
        context_state(vogl_gl_replayer &replayer)
            : m_replayer(replayer)
        {
            VOGL_FUNC_TRACER

            m_pShared_state = this;
            m_ref_count = 1;
            m_deleted = false;

            m_last_call_counter = 0;

            m_has_been_made_current = false;
            m_inside_gl_begin = false;

            m_trace_context = 0;
            m_replay_context = 0;

            m_cur_replay_program = 0;
            m_cur_trace_program = 0;

            m_current_display_list_handle = -1;
            m_current_display_list_mode = GL_NONE;
        }

        bool handle_context_made_current();

        bool is_root_context() const
        {
            return m_pShared_state == this;
        }
        bool is_share_context() const
        {
            return m_pShared_state != this;
        }

        bool is_composing_display_list() const
        {
            return m_current_display_list_handle >= 0;
        }

        vogl_gl_replayer &m_replayer;

        vogl_context_desc m_context_desc;
        vogl_context_info m_context_info;

        context_state *m_pShared_state;
        int m_ref_count;
        bool m_deleted;

        uint64_t m_last_call_counter;

        bool m_has_been_made_current;
        bool m_inside_gl_begin;

        vogl_trace_context_ptr_value m_trace_context;
        GLReplayContextType m_replay_context;

        // maps trace to replay handles
        gl_handle_hash_map m_framebuffers;
        gl_handle_hash_map m_queries;
        gl_handle_hash_map m_sampler_objects;
        gl_handle_hash_map m_buffers;
        gl_handle_hash_map m_buffer_targets; // maps trace handles to buffer targets
        gl_handle_hash_map m_vertex_array_objects;

        gl_handle_hash_map m_lists;

        gl_sync_hash_map m_syncs;

        // maps trace programs to glsl_program_state's
        glsl_program_hash_map m_glsl_program_hash_map;

        // maps replay query handles to the last active begin target
        vogl_handle_hash_map m_query_targets;

        gl_handle_hash_map m_arb_programs;        // ARB_vertex_program/ARB_fragment_program, maps trace to replay handles
        gl_handle_hash_map m_arb_program_targets; // maps trace programs to targets
        
        GLuint m_cur_replay_program;
        GLuint m_cur_trace_program;

        vogl::vector<GLfloat> m_feedback_buffer;
        vogl::vector<GLuint> m_select_buffer;

        vogl_capture_context_params m_shadow_state;

        int m_current_display_list_handle;
        GLenum m_current_display_list_mode;
    };

    typedef vogl::hash_map<vogl_trace_context_ptr_value, context_state *, bit_hasher<vogl_trace_context_ptr_value> > context_hash_map; // maps trace to replay contexts
    context_hash_map m_contexts;

    vogl_trace_context_ptr_value m_cur_trace_context;
    GLReplayContextType m_cur_replay_context;
    context_state *m_pCur_context_state;

    context_state *get_context_state()
    {
        return m_pCur_context_state;
    }
    context_state *get_shared_state()
    {
        return m_pCur_context_state->m_pShared_state;
    }

    enum
    {
        VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES = 32,
        VOGL_MAX_SUPPORTED_GL_TEXCOORD_ARRAYS = 32
    };

    uint8_vec m_client_side_vertex_attrib_data[VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES];
    uint8_vec m_client_side_array_data[VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS];
    uint8_vec m_client_side_texcoord_data[VOGL_MAX_SUPPORTED_GL_TEXCOORD_ARRAYS];

    uint8_vec m_screenshot_buffer;
    uint8_vec m_screenshot_buffer2;

    vogl::vector<uint8_t> m_index_data;

    uint64_t m_frame_draw_counter;
    uint64_t m_frame_draw_counter_kill_threshold;

    bool m_is_valid;

    const vogl_blob_manager *m_pBlob_manager;

    const vogl_gl_state_snapshot *m_pPending_snapshot;
    bool m_delete_pending_snapshot_after_applying;

    vogl_gl_get_proc_address_helper_func_ptr_t m_proc_address_helper_func;
    bool m_wrap_all_gl_calls;

    // TODO: Make a 1st class snapshot cache class
    struct snapshot_cache_entry
    {
        snapshot_cache_entry()
            : m_pSnapshot(NULL)
        {
        }

        dynamic_string m_name;
        vogl_gl_state_snapshot *m_pSnapshot;
    };
    typedef vogl::vector<snapshot_cache_entry> snapshot_vec;
    snapshot_vec m_snapshots;

    void dump_packet_as_func_call(const vogl_trace_packet &trace_packet);
    void dump_trace_gl_packet_debug_info(const vogl_trace_gl_entrypoint_packet &gl_packet);

    status_t trigger_pending_window_resize(uint32_t win_width, uint32_t win_height);
    void clear_pending_window_resize();
    status_t process_frame_check_for_pending_window_resize();

    void destroy_pending_snapshot();

    // Returns *true* if any errors occurred.
    bool check_gl_error_internal(bool quietly = false, const char *pFile = "", uint32_t line = 0, const char *pFunc = "");
#define check_gl_error() (((m_flags &cGLReplayerBenchmarkMode) == 0) ? check_gl_error_internal(false, __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR) : false)
#define check_gl_error_quietly() (((m_flags &cGLReplayerBenchmarkMode) == 0) ? check_gl_error_internal(true, __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR) : false)

    void destroy_contexts();
    void clear_contexts();

    // from=replay, to=trace
    class replay_to_trace_handle_remapper : public vogl_handle_remapper
    {
        vogl_gl_replayer &m_replayer;

        // TODO: This searches the entire hash map! Slow!
        bool remap_replay_to_trace_handle(const gl_handle_hash_map &hash_map, GLuint &handle) const
        {
            VOGL_ASSERT(hash_map.search_table_for_value_get_count(handle) <= 1);

            gl_handle_hash_map::const_iterator it(hash_map.search_table_for_value(handle));
            if (it != hash_map.end())
            {
                VOGL_ASSERT(it->second == handle);
                handle = it->first;
                return true;
            }
            return false;
        }

    public:
        replay_to_trace_handle_remapper(vogl_gl_replayer &replayer)
            : m_replayer(replayer)
        {
        }

        virtual bool is_default_remapper() const
        {
            return false;
        }

        virtual uint64_t remap_handle(vogl_namespace_t handle_namespace, uint64_t replay_handle);

        virtual bool is_valid_handle(vogl_namespace_t handle_namespace, uint64_t replay_handle);

        virtual int32_t remap_location(uint32_t replay_program, int32_t replay_location);

        virtual vogl_trace_ptr_value remap_vertex_attrib_ptr(uint32_t index, vogl_trace_ptr_value ptr_val)
        {
            VOGL_NOTE_UNUSED(index);
            return ptr_val;
        }

        virtual vogl_trace_ptr_value remap_vertex_array_ptr(vogl_client_side_array_desc_id_t id, uint32_t index, vogl_trace_ptr_value ptr_val)
        {
            VOGL_NOTE_UNUSED(id);
            VOGL_NOTE_UNUSED(index);
            return ptr_val;
        }

        virtual bool determine_from_object_target(vogl_namespace_t handle_namespace, uint64_t replay_handle, GLenum &target);

        virtual bool determine_to_object_target(vogl_namespace_t handle_namespace, uint64_t trace_handle, GLenum &target);
    };

    replay_to_trace_handle_remapper m_replay_to_trace_remapper;

    // from=trace, to=replay
    class trace_to_replay_handle_remapper : public vogl_handle_remapper
    {
        vogl_gl_replayer &m_replayer;

    public:
        // Note: This used to also take a vogl_gl_state_snapshot ref
        trace_to_replay_handle_remapper(vogl_gl_replayer &replayer)
            : m_replayer(replayer)
        {
        }

        virtual bool is_default_remapper() const
        {
            return false;
        }

        virtual uint64_t remap_handle(vogl_namespace_t handle_namespace, uint64_t from_handle);

        virtual bool is_valid_handle(vogl_namespace_t handle_namespace, uint64_t replay_handle);

        virtual int32_t remap_location(uint32_t trace_program, int32_t from_location);

        virtual vogl_trace_ptr_value remap_vertex_attrib_ptr(uint32_t index, vogl_trace_ptr_value ptr_val);

        virtual vogl_trace_ptr_value remap_vertex_array_ptr(vogl_client_side_array_desc_id_t id, uint32_t index, vogl_trace_ptr_value ptr_val);

        virtual void declare_handle(vogl_namespace_t handle_namespace, uint64_t from_handle, uint64_t to_handle, GLenum target);

        virtual void delete_handle_and_object(vogl_namespace_t handle_namespace, uint64_t from_handle, uint64_t to_handle);

        virtual void declare_location(uint32_t from_program_handle, uint32_t to_program_handle, int32_t from_location, int32_t to_location);

        virtual bool determine_from_object_target(vogl_namespace_t handle_namespace, uint64_t trace_handle, GLenum &target);

        virtual bool determine_to_object_target(vogl_namespace_t handle_namespace, uint64_t replay_handle, GLenum &target);
    };

    inline bool gen_handle(gl_handle_hash_map &handle_hash_map, GLuint trace_handle, GLuint replay_handle)
    {
        VOGL_FUNC_TRACER

        if (!trace_handle)
        {
            VOGL_ASSERT(!replay_handle);
            return true;
        }

        if (!replay_handle)
        {
            process_entrypoint_error("Handle gen failed during replay, but succeeded during trace! Trace handle: %u\n", trace_handle);
            return false;
        }
        else
        {
            gl_handle_hash_map::insert_result result(handle_hash_map.insert(trace_handle, replay_handle));
            if (!result.second)
            {
                process_entrypoint_error("Replacing genned GL handle %u trace handle %u in handle hash map (this indicates a handle shadowing error)\n", replay_handle, trace_handle);

                gl_handle_hash_map::iterator it = result.first;
                it->second = replay_handle;
            }
        }

        return true;
    }

    template <typename T>
    inline bool gen_handles(gl_handle_hash_map &handle_hash_map, GLsizei n, const GLuint *pTrace_ids, T gl_gen_function, GLuint *pReplay_handles)
    {
        VOGL_FUNC_TRACER

        for (GLsizei i = 0; i < n; i++)
        {
            if (pReplay_handles)
                pReplay_handles[i] = 0;

            if (!pTrace_ids[i])
                continue;

            GLuint replay_id = 0;
            gl_gen_function(1, &replay_id);

            if (!replay_id)
            {
                process_entrypoint_error("GL handle gen call failed, but succeeded in the trace!\n");
                return false;
            }

            if (pReplay_handles)
                pReplay_handles[i] = replay_id;

            gl_handle_hash_map::insert_result result(handle_hash_map.insert(pTrace_ids[i], replay_id));
            if (!result.second)
            {
                process_entrypoint_error("TODO: Replacing genned GL handle %u trace handle %u in handle hash map (this indicates a handle shadowing error)\n", replay_id, pTrace_ids[i]);

                gl_handle_hash_map::iterator it = result.first;
                it->second = replay_id;
            }
        }

        return true;
    }

    template <typename T>
    inline void delete_handles(gl_handle_hash_map &handle_hash_map, GLsizei trace_n, const GLuint *pTrace_ids, T gl_delete_function)
    {
        VOGL_FUNC_TRACER

        vogl::vector<GLuint> replay_ids;
        replay_ids.reserve(trace_n);

        for (int i = 0; i < trace_n; i++)
        {
            GLuint trace_id = pTrace_ids[i];
            if (!trace_id)
                continue;

            gl_handle_hash_map::const_iterator it(handle_hash_map.find(trace_id));

            if (it != handle_hash_map.end())
            {
                replay_ids.push_back(it->second);

                handle_hash_map.erase(pTrace_ids[i]);
            }
            else
            {
                replay_ids.push_back(trace_id);

                process_entrypoint_warning("Couldn't map trace GL handle %u to replay GL handle, using trace handle instead\n", pTrace_ids[i]);
            }
        }

        if (replay_ids.size())
            gl_delete_function(replay_ids.size(), replay_ids.get_ptr());
    }

    template <typename T>
    inline void delete_handle(gl_handle_hash_map &handle_hash_map, GLuint trace_id, T gl_delete_function)
    {
        VOGL_FUNC_TRACER

        delete_handles(handle_hash_map, 1, &trace_id, gl_delete_function);
    }

    template <typename T>
    inline bool gen_handles(vogl_handle_tracker &handle_tracker, GLsizei n, const GLuint *pTrace_ids, T gl_gen_function, GLuint *pReplay_handles, GLenum def_target)
    {
        VOGL_FUNC_TRACER

        for (GLsizei i = 0; i < n; i++)
        {
            if (pReplay_handles)
                pReplay_handles[i] = 0;

            if (!pTrace_ids[i])
                continue;

            GLuint replay_id = 0;
            gl_gen_function(1, &replay_id);

            if (!replay_id)
            {
                process_entrypoint_error("GL handle gen call failed, but succeeded in the trace!\n");
                return false;
            }

            if (pReplay_handles)
                pReplay_handles[i] = replay_id;

            if (!handle_tracker.insert(pTrace_ids[i], replay_id, def_target))
            {
                process_entrypoint_error("Replacing genned GL handle %u trace handle %u in handle hash map (this indicates a handle shadowing error)\n", replay_id, pTrace_ids[i]);

                handle_tracker.erase(pTrace_ids[i]);

                bool success = handle_tracker.insert(pTrace_ids[i], replay_id, def_target);
                VOGL_ASSERT(success);
                VOGL_NOTE_UNUSED(success);
            }
        }

        return true;
    }

    inline bool gen_handle(vogl_handle_tracker &handle_tracker, const GLuint trace_id, GLuint replay_id, GLenum def_target)
    {
        VOGL_FUNC_TRACER

        if (!trace_id)
        {
            VOGL_ASSERT(!replay_id);
            return true;
        }

        if (!handle_tracker.insert(trace_id, replay_id, def_target))
        {
            process_entrypoint_error("Replacing genned GL handle %u trace handle %u in handle hash map, namespace %s (this indicates a handle shadowing error)\n", replay_id, trace_id, vogl_get_namespace_name(handle_tracker.get_namespace()));

            handle_tracker.erase(trace_id);

            bool success = handle_tracker.insert(trace_id, replay_id, def_target);
            VOGL_ASSERT(success);
            VOGL_NOTE_UNUSED(success);
        }

        return true;
    }

    template <typename T>
    inline void delete_handles(vogl_handle_tracker &handle_tracker, GLsizei trace_n, const GLuint *pTrace_ids, T gl_delete_function)
    {
        VOGL_FUNC_TRACER

        vogl::growable_array<GLuint, 32> replay_ids;
        replay_ids.reserve(trace_n);

        for (int i = 0; i < trace_n; i++)
        {
            GLuint trace_id = pTrace_ids[i];
            if (!trace_id)
                continue;

            GLuint replay_id = trace_id;
            if (!handle_tracker.map_handle_to_inv_handle(trace_id, replay_id))
                process_entrypoint_warning("Couldn't map trace GL handle %u to replay GL handle, using trace handle instead, namespace %s\n", pTrace_ids[i], vogl_get_namespace_name(handle_tracker.get_namespace()));
            else
                handle_tracker.erase(trace_id);

            replay_ids.push_back(replay_id);
        }

        if (replay_ids.size())
            gl_delete_function(replay_ids.size(), replay_ids.get_ptr());
    }

    static void delete_program_helper(GLsizei n, const GLuint *pIDs)
    {
        VOGL_FUNC_TRACER

        for (GLsizei i = 0; i < n; i++)
            GL_ENTRYPOINT(glDeleteProgram)(pIDs[i]);
    }

    static void delete_list_helper(GLsizei n, const GLuint *pIDs)
    {
        VOGL_FUNC_TRACER

        for (GLsizei i = 0; i < n; i++)
            GL_ENTRYPOINT(glDeleteLists)(pIDs[i], 1);
    }

    // TODO: Closely examine each object type and set insert_if_not_found to true for the ones that don't really need to be genned (textures is already done)
    inline GLuint map_handle(gl_handle_hash_map &handle_hash_map, GLuint trace_handle, bool insert_if_not_found = false)
    {
        VOGL_FUNC_TRACER

        if (!trace_handle)
            return 0;

        gl_handle_hash_map::const_iterator it = handle_hash_map.find(trace_handle);
        if (it == handle_hash_map.end())
        {
            process_entrypoint_warning("Couldn't map trace GL handle %u to GL handle, using trace handle instead (handle may not have been genned)\n", trace_handle);

            if (insert_if_not_found)
                handle_hash_map.insert(trace_handle, trace_handle);

            return trace_handle;
        }

        return it->second;
    }

    inline bool map_handle(vogl_handle_tracker &handle_tracker, GLuint trace_handle, GLuint &replay_handle)
    {
        VOGL_FUNC_TRACER

        replay_handle = trace_handle;

        if (!trace_handle)
            return true;

        if (!handle_tracker.map_handle_to_inv_handle(trace_handle, replay_handle))
        {
            process_entrypoint_warning("Couldn't map trace GL handle %u to GL handle, using trace handle instead (handle may not have been genned), namespace: %s\n", trace_handle, vogl_get_namespace_name(handle_tracker.get_namespace()));
            return false;
        }

        return true;
    }

    inline GLuint map_handle(vogl_handle_tracker &handle_tracker, GLuint trace_handle, bool *pSuccess = NULL)
    {
        VOGL_FUNC_TRACER

        if (!trace_handle)
        {
            if (pSuccess)
                *pSuccess = true;
            return trace_handle;
        }

        GLuint replay_handle = trace_handle;
        if (!handle_tracker.map_handle_to_inv_handle(trace_handle, replay_handle))
        {
            if (pSuccess)
                *pSuccess = false;
            process_entrypoint_warning("Couldn't map trace GL handle %u to GL handle, using trace handle instead (handle may not have been genned), namespace: %s\n", trace_handle, vogl_get_namespace_name(handle_tracker.get_namespace()));
            return replay_handle;
        }

        if (pSuccess)
            *pSuccess = true;
        return replay_handle;
    }

    inline context_state *get_trace_context_state(vogl_trace_context_ptr_value trace_context)
    {
        VOGL_FUNC_TRACER

        if (!trace_context)
            return NULL;

        context_hash_map::iterator it = m_contexts.find(trace_context);
        return (it == m_contexts.end()) ? NULL : it->second;
    }

    context_state *define_new_context(vogl_trace_context_ptr_value trace_context, GLReplayContextType replay_context, vogl_trace_context_ptr_value trace_share_context, GLboolean direct, gl_entrypoint_id_t creation_func, const int *pAttrib_list, uint32_t attrib_list_size);

    GLReplayContextType remap_context(vogl_trace_context_ptr_value trace_context);

    bool destroy_context(vogl_trace_context_ptr_value trace_context);

    // glVertexPointer, glNormalPointer, etc. client side data
    bool set_client_side_array_data(const key_value_map &map, GLuint start, GLuint end, GLuint basevertex);

    // glVertexAttrib client side data
    bool set_client_side_vertex_attrib_array_data(const key_value_map &map, GLuint start, GLuint end, GLuint basevertex);

    bool draw_elements_client_side_array_setup(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, vogl_trace_ptr_value trace_indices_ptr_value, const GLvoid *&pIndices, GLint basevertex, bool has_valid_start_end, bool indexed);

    GLint determine_uniform_replay_location(GLuint trace_program, GLint trace_location);

    inline GLint determine_uniform_replay_location(GLint trace_location)
    {
        return determine_uniform_replay_location(m_pCur_context_state->m_cur_trace_program, trace_location);
    }

    template <uint32_t N, class T, class F>
    inline void set_uniformv_helper(F func)
    {
        VOGL_FUNC_TRACER

        GLint replay_location = determine_uniform_replay_location(m_pCur_gl_packet->get_param_value<GLint>(0));

        GLsizei count = m_pCur_gl_packet->get_param_value<GLsizei>(1);
        const T *pValues = m_pCur_gl_packet->get_param_client_memory<T>(2);
        VOGL_ASSERT((uint32_t)m_pCur_gl_packet->get_param_client_memory_data_size(2) == sizeof(T) * N * count);

        func(replay_location, count, pValues);
    }

    template <uint32_t C, uint32_t R, class T, class F>
    inline void set_uniform_matrixv_helper(F func)
    {
        VOGL_FUNC_TRACER

        GLint replay_location = determine_uniform_replay_location(m_pCur_gl_packet->get_param_value<GLint>(0));

        GLsizei count = m_pCur_gl_packet->get_param_value<GLsizei>(1);
        GLboolean transpose = m_pCur_gl_packet->get_param_value<GLboolean>(2);
        const T *pValues = m_pCur_gl_packet->get_param_client_memory<T>(3);
        VOGL_ASSERT((uint32_t)m_pCur_gl_packet->get_param_client_memory_data_size(3) == sizeof(T) * C * R * count);

        func(replay_location, count, transpose, pValues);
    }

    // glUniform* helpers
    template <class T, class F>
    inline void set_uniform_helper1(F func)
    {
        VOGL_FUNC_TRACER

        func(determine_uniform_replay_location(m_pCur_gl_packet->get_param_value<GLint>(0)), m_pCur_gl_packet->get_param_value<T>(1));
    }

    template <class T, class F>
    inline void set_uniform_helper2(F func)
    {
        VOGL_FUNC_TRACER

        func(determine_uniform_replay_location(m_pCur_gl_packet->get_param_value<GLint>(0)), m_pCur_gl_packet->get_param_value<T>(1), m_pCur_gl_packet->get_param_value<T>(2));
    }

    template <class T, class F>
    inline void set_uniform_helper3(F func)
    {
        VOGL_FUNC_TRACER

        func(determine_uniform_replay_location(m_pCur_gl_packet->get_param_value<GLint>(0)), m_pCur_gl_packet->get_param_value<T>(1), m_pCur_gl_packet->get_param_value<T>(2), m_pCur_gl_packet->get_param_value<T>(3));
    }

    template <class T, class F>
    inline void set_uniform_helper4(F func)
    {
        VOGL_FUNC_TRACER

        func(determine_uniform_replay_location(m_pCur_gl_packet->get_param_value<GLint>(0)), m_pCur_gl_packet->get_param_value<T>(1), m_pCur_gl_packet->get_param_value<T>(2), m_pCur_gl_packet->get_param_value<T>(3), m_pCur_gl_packet->get_param_value<T>(4));
    }

    // glSetProgramUniform* helpers
    template <class T, class F>
    inline void set_program_uniform_helper1(F func)
    {
        VOGL_FUNC_TRACER

        GLuint trace_handle = m_pCur_gl_packet->get_param_value<GLuint>(0);
        GLuint replay_handle = map_handle(m_pCur_context_state->m_pShared_state->m_shadow_state.m_objs, trace_handle);
        func(replay_handle, determine_uniform_replay_location(trace_handle, m_pCur_gl_packet->get_param_value<GLint>(1)), m_pCur_gl_packet->get_param_value<T>(2));
    }

    template <class T, class F>
    inline void set_program_uniform_helper2(F func)
    {
        VOGL_FUNC_TRACER

        GLuint trace_handle = m_pCur_gl_packet->get_param_value<GLuint>(0);
        GLuint replay_handle = map_handle(m_pCur_context_state->m_pShared_state->m_shadow_state.m_objs, trace_handle);
        func(replay_handle, determine_uniform_replay_location(trace_handle, m_pCur_gl_packet->get_param_value<GLint>(1)), m_pCur_gl_packet->get_param_value<T>(2), m_pCur_gl_packet->get_param_value<T>(3));
    }

    template <class T, class F>
    inline void set_program_uniform_helper3(F func)
    {
        VOGL_FUNC_TRACER

        GLuint trace_handle = m_pCur_gl_packet->get_param_value<GLuint>(0);
        GLuint replay_handle = map_handle(m_pCur_context_state->m_pShared_state->m_shadow_state.m_objs, trace_handle);
        func(replay_handle, determine_uniform_replay_location(trace_handle, m_pCur_gl_packet->get_param_value<GLint>(1)), m_pCur_gl_packet->get_param_value<T>(2), m_pCur_gl_packet->get_param_value<T>(3), m_pCur_gl_packet->get_param_value<T>(4));
    }

    template <class T, class F>
    inline void set_program_uniform_helper4(F func)
    {
        VOGL_FUNC_TRACER

        GLuint trace_handle = m_pCur_gl_packet->get_param_value<GLuint>(0);
        GLuint replay_handle = map_handle(m_pCur_context_state->m_pShared_state->m_shadow_state.m_objs, trace_handle);
        func(replay_handle, determine_uniform_replay_location(trace_handle, m_pCur_gl_packet->get_param_value<GLint>(1)), m_pCur_gl_packet->get_param_value<T>(2), m_pCur_gl_packet->get_param_value<T>(3), m_pCur_gl_packet->get_param_value<T>(4), m_pCur_gl_packet->get_param_value<T>(5));
    }

    template <uint32_t C, uint32_t R, class T, class F>
    inline void set_program_uniform_matrixv_helper(F func)
    {
        VOGL_FUNC_TRACER

        GLuint trace_handle = m_pCur_gl_packet->get_param_value<GLuint>(0);
        GLuint replay_handle = map_handle(m_pCur_context_state->m_pShared_state->m_shadow_state.m_objs, trace_handle);

        GLint replay_location = determine_uniform_replay_location(trace_handle, m_pCur_gl_packet->get_param_value<GLint>(1));

        GLsizei count = m_pCur_gl_packet->get_param_value<GLsizei>(2);
        GLboolean transpose = m_pCur_gl_packet->get_param_value<GLboolean>(3);
        const T *pValues = m_pCur_gl_packet->get_param_client_memory<T>(4);
        VOGL_ASSERT((uint32_t)m_pCur_gl_packet->get_param_client_memory_data_size(4) == sizeof(T) * C * R * count);

        func(replay_handle, replay_location, count, transpose, pValues);
    }

    template <uint32_t N, class T, class F>
    inline void set_program_uniformv_helper(F func)
    {
        VOGL_FUNC_TRACER

        GLuint trace_handle = m_pCur_gl_packet->get_param_value<GLuint>(0);
        GLuint replay_handle = map_handle(m_pCur_context_state->m_pShared_state->m_shadow_state.m_objs, trace_handle);

        GLint replay_location = determine_uniform_replay_location(trace_handle, m_pCur_gl_packet->get_param_value<GLint>(1));

        GLsizei count = m_pCur_gl_packet->get_param_value<GLsizei>(2);
        const T *pValues = m_pCur_gl_packet->get_param_client_memory<T>(3);

        VOGL_ASSERT((uint32_t)m_pCur_gl_packet->get_param_client_memory_data_size(3) == sizeof(T) * N * count);

        func(replay_handle, replay_location, count, pValues);
    }

    static void delete_objects_arb(GLsizei n, const GLuint *pHandles)
    {
        VOGL_FUNC_TRACER

        for (GLsizei i = 0; i < n; i++)
            GL_ENTRYPOINT(glDeleteObjectARB)(pHandles[i]);
    }

    template <typename T, typename F>
    inline status_t get_vertex_attrib_helper(F entrypoint)
    {
        if (benchmark_mode())
            return cStatusOK;

        VOGL_FUNC_TRACER

        GLint index = m_pCur_gl_packet->get_param_value<GLint>(0);
        GLenum pname = m_pCur_gl_packet->get_param_value<GLenum>(1);
        const T *pTrace_params = m_pCur_gl_packet->get_param_client_memory<const T>(2);
        uint32_t num_trace_params = m_pCur_gl_packet->get_param_client_memory_data_size(2) / sizeof(T);

        int n = get_gl_enums().get_pname_count(pname);
        if (n <= 0)
        {
            process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
            return cStatusSoftFailure;
        }

        vogl::vector<T> params(n);
        entrypoint(index, pname, params.get_ptr());

        if (num_trace_params < static_cast<uint32_t>(n))
        {
            process_entrypoint_error("Was expecting at least %u params for GL pname 0x%08X, but only got %u params in the trace\n", n, pname, num_trace_params);
            return cStatusSoftFailure;
        }
        else if (!pTrace_params)
        {
            process_entrypoint_error("Trace has NULL params field, can't diff trace vs. replay's params\n");
            return cStatusSoftFailure;
        }
        else
        {
            if (memcmp(pTrace_params, params.get_ptr(), n * sizeof(T)) != 0)
            {
                process_entrypoint_error("Replay's results differ from trace's\n");
                return cStatusSoftFailure;
            }
        }

        return cStatusOK;
    }

    template <typename F>
    inline void vertex_array_helper(
        GLint size, GLenum type, GLsizei stride, vogl_trace_ptr_value trace_pointer,
        vogl_client_side_array_desc_id_t id, F func, uint8_vec &array_data)
    {
        VOGL_FUNC_TRACER

        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[id];
        VOGL_NOTE_UNUSED(desc);

        GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);

        GLvoid *pPtr = reinterpret_cast<GLvoid *>(trace_pointer);
        if ((!buffer) && (trace_pointer))
        {
            // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
            // So point this guy into one of our client size memory buffers that's hopefully large enough.
            if (!array_data.size())
            {
                array_data.resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
            }
            pPtr = array_data.get_ptr();
        }

        func(size, type, stride, pPtr);
    }

    template <typename F>
    inline void vertex_array_helper_count(
        GLint size, GLenum type, GLsizei stride, GLsizei count, vogl_trace_ptr_value trace_pointer,
        vogl_client_side_array_desc_id_t id, F func, uint8_vec &array_data)
    {
        VOGL_FUNC_TRACER

        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[id];
        VOGL_NOTE_UNUSED(desc);

        GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);

        GLvoid *pPtr = reinterpret_cast<GLvoid *>(trace_pointer);
        if ((!buffer) && (trace_pointer))
        {
            // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
            // So point this guy into one of our client size memory buffers that's hopefully large enough.
            if (!array_data.size())
            {
                array_data.resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
            }
            pPtr = array_data.get_ptr();
        }

        func(size, type, stride, count, pPtr);
    }

    template <typename F>
    inline void vertex_array_helper_no_size(
        GLenum type, GLsizei stride, vogl_trace_ptr_value trace_pointer,
        vogl_client_side_array_desc_id_t id, F func)
    {
        VOGL_FUNC_TRACER

        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[id];
        VOGL_NOTE_UNUSED(desc);

        GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);
        GLvoid *pPtr = reinterpret_cast<GLvoid *>(trace_pointer);
        if ((!buffer) && (trace_pointer))
        {
            // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
            // So point this guy into one of our client size memory buffers that's hopefully large enough.
            if (!m_client_side_array_data[id].size())
            {
                m_client_side_array_data[id].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
            }
            pPtr = m_client_side_array_data[id].get_ptr();
        }

        func(type, stride, pPtr);
    }

    template <typename F>
    inline void vertex_array_helper_no_size_count(
        GLenum type, GLsizei stride, GLsizei count, vogl_trace_ptr_value trace_pointer,
        vogl_client_side_array_desc_id_t id, F func)
    {
        VOGL_FUNC_TRACER

        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[id];
        VOGL_NOTE_UNUSED(desc);

        GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);
        GLvoid *pPtr = reinterpret_cast<GLvoid *>(trace_pointer);
        if ((!buffer) && (trace_pointer))
        {
            // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
            // So point this guy into one of our client size memory buffers that's hopefully large enough.
            if (!m_client_side_array_data[id].size())
            {
                m_client_side_array_data[id].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
            }
            pPtr = m_client_side_array_data[id].get_ptr();
        }

        func(type, stride, count, pPtr);
    }

    template <typename F>
    inline void vertex_array_helper_no_type_no_size(
        GLsizei stride, vogl_trace_ptr_value trace_pointer,
        vogl_client_side_array_desc_id_t id, F func)
    {
        VOGL_FUNC_TRACER

        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[id];
        VOGL_NOTE_UNUSED(desc);

        GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);
        GLvoid *pPtr = reinterpret_cast<GLvoid *>(trace_pointer);
        if ((!buffer) && (trace_pointer))
        {
            // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
            // So point this guy into one of our client size memory buffers that's hopefully large enough.
            if (!m_client_side_array_data[id].size())
            {
                m_client_side_array_data[id].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
            }
            pPtr = m_client_side_array_data[id].get_ptr();
        }

        func(stride, pPtr);
    }

    template <typename F>
    inline void vertex_array_helper_no_type_no_size_count(
        GLsizei stride, GLsizei count, vogl_trace_ptr_value trace_pointer,
        vogl_client_side_array_desc_id_t id, F func)
    {
        VOGL_FUNC_TRACER

        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[id];
        VOGL_NOTE_UNUSED(desc);

        GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);
        GLvoid *pPtr = reinterpret_cast<GLvoid *>(trace_pointer);
        if ((!buffer) && (trace_pointer))
        {
            // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
            // So point this guy into one of our client size memory buffers that's hopefully large enough.
            if (!m_client_side_array_data[id].size())
            {
                m_client_side_array_data[id].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
            }
            pPtr = m_client_side_array_data[id].get_ptr();
        }

        func(stride, count, static_cast<const unsigned char*>(pPtr));
    }

    void process_entrypoint_print_summary_context(const char *caller_info, eConsoleMessageType msg_type);
    void print_detailed_context(const char *caller_info, eConsoleMessageType msg_type);
    void process_entrypoint_msg_print_detailed_context(const char *caller_info, eConsoleMessageType msg_type);

    void process_entrypoint_(const char *caller_info, eConsoleMessageType msgtype, const char *pFmt, ...) VOGL_ATTRIBUTE_PRINTF(4, 5);

    status_t switch_contexts(vogl_trace_context_ptr_value trace_context);

    // Loosely derived from http://www.altdevblogaday.com/2011/06/23/improving-opengl-error-messages/
    static void GLAPIENTRY debug_callback_arb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *pUser_param);
    static void GLAPIENTRY debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *pUser_param);

    bool is_extension_supported(const char *pExt);

    bool handle_context_made_current();

    void dump_context_attrib_list(const int *pAttrib_list, uint32_t size);

    int find_attrib_key(const vogl::vector<int> &attrib_list, int key_to_find);

    status_t create_context_attribs(
        vogl_trace_context_ptr_value trace_context, vogl_trace_context_ptr_value trace_share_context, GLReplayContextType replay_share_context, Bool direct,
        const int *pTrace_attrib_list, int trace_attrib_list_size, bool expecting_attribs);

    status_t process_pending_make_current();

    status_t process_internal_trace_command(const vogl_trace_gl_entrypoint_packet &gl_packet);

    bool check_program_binding_shadow();
    void handle_use_program(GLuint trace_handle, gl_entrypoint_id_t entrypoint_id);
    void handle_delete_program(GLuint trace_handle);
    void handle_delete_shader(GLuint trace_handle);
    void handle_detach_shader(gl_entrypoint_id_t entrypoint_id);
    void handle_link_program(gl_entrypoint_id_t entrypoint_id);
    void handle_post_link_program(gl_entrypoint_id_t entrypoint_id, GLuint trace_handle, GLuint replay_handle, GLenum type, GLsizei count, GLchar *const *strings);

    static void display_list_bind_callback(vogl_namespace_t handle_namespace, GLenum target, GLuint handle, void *pOpaque);

    status_t restore_context(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot);
    status_t restore_objects(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_state, vogl_gl_object_state_type state_type, vogl_const_gl_object_state_ptr_vec &objects_to_delete);
    vogl_gl_replayer::status_t restore_display_lists(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot);
    status_t restore_general_state(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot);
    status_t update_context_shadows(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot);
    void handle_marked_for_deleted_objects(vogl_const_gl_object_state_ptr_vec &objects_to_delete, trace_to_replay_handle_remapper &trace_to_replay_remapper);
    bool determine_used_program_handles(const vogl_trace_packet_array &trim_packets, vogl_handle_hash_set &replay_program_handles);

    vogl_gl_replayer::status_t process_applying_pending_snapshot();
    bool validate_program_and_shader_handle_tables();
    bool validate_textures();

    void fill_replay_handle_hash_set(vogl_handle_hash_set &replay_handle_hash, const gl_handle_hash_map &trace_to_replay_hash);

    // write_trim_file_internal() may modify trim_packets
    bool write_trim_file_internal(vogl_trace_packet_array &trim_packets, const dynamic_string &trim_filename, vogl_trace_file_reader &trace_reader, bool optimize_snapshot, dynamic_string *pSnapshot_id);

    bool dump_frontbuffer_to_file(const dynamic_string &filename);

    bool benchmark_mode() const
    {
        return (m_flags & cGLReplayerBenchmarkMode) != 0;
    }

    // DO NOT make these methods public
    status_t process_pending_gl_entrypoint_packets();
    status_t process_gl_entrypoint_packet(vogl_trace_packet& trace_packet);
    status_t process_gl_entrypoint_packet_internal(vogl_trace_packet &trace_packet);
};

#endif // VOGL_GL_REPLAYER_H
