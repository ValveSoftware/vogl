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

// File: vogl_gl_state_snapshot.cpp
#include "vogl_gl_state_snapshot.h"
#include "vogl_uuid.h"

vogl_context_snapshot::vogl_context_snapshot()
    : m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_context_snapshot::~vogl_context_snapshot()
{
    VOGL_FUNC_TRACER

    destroy_objects();
}

void vogl_context_snapshot::destroy_objects()
{
    VOGL_FUNC_TRACER

    for (uint32_t i = 0; i < m_object_ptrs.size(); i++)
        vogl_delete(m_object_ptrs[i]);
    m_object_ptrs.clear();
}

void vogl_context_snapshot::clear()
{
    VOGL_FUNC_TRACER

    m_context_desc.clear();
    m_context_info.clear();
    m_general_state.clear();
    m_texenv_state.clear();
    m_light_state.clear();
    m_material_state.clear();
    m_display_list_state.clear();
    m_matrix_state.clear();
    m_polygon_stipple_state.clear();
    m_current_vertex_attrib_state.clear();
    m_arb_program_environment_state.clear();

    destroy_objects();

    m_is_valid = false;
}

// Note info may NOT be valid here if the context was never made current!
bool vogl_context_snapshot::capture(const vogl_context_desc &desc, const vogl_context_info &info, const vogl_capture_context_params &capture_params, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    VOGL_CHECK_GL_ERROR;

    clear();

    vogl_verbose_printf("Starting capture on trace context 0x%" PRIx64 ", has context info: %u\n", cast_val_to_uint64(desc.get_trace_context()), info.is_valid());

    m_context_desc = desc;
    m_context_info = info;
    m_display_list_state = capture_params.m_display_lists;

    // Has this context been ever made current?
    if (info.is_valid())
    {
        if (!m_general_state.snapshot(m_context_info))
            goto handle_error;

        if (!m_current_vertex_attrib_state.snapshot(m_context_info))
            goto handle_error;

        if (!info.is_core_profile())
        {
            if (!m_texenv_state.snapshot(m_context_info))
                goto handle_error;

            if (!m_light_state.snapshot(m_context_info))
                goto handle_error;

            if (!m_material_state.snapshot(m_context_info))
                goto handle_error;

            if (!m_matrix_state.snapshot(m_context_info))
                goto handle_error;

            if (!m_polygon_stipple_state.snapshot(m_context_info))
                goto handle_error;

            if (info.supports_extension("GL_ARB_vertex_program"))
            {
                if (!m_arb_program_environment_state.snapshot(m_context_info))
                    goto handle_error;
            }
        }

        // Keep this list in sync with vogl_gl_object_state_type (order doesn't matter, just make sure all valid object types are present)
        const vogl_gl_object_state_type s_object_type_capture_order[] = { cGLSTTexture, cGLSTBuffer, cGLSTSampler, cGLSTQuery, cGLSTRenderbuffer, cGLSTFramebuffer, cGLSTVertexArray, cGLSTShader, cGLSTProgram, cGLSTSync, cGLSTARBProgram, cGLSTProgramPipeline };
        VOGL_ASSUME(VOGL_ARRAY_SIZE(s_object_type_capture_order) == cGLSTTotalTypes - 1);

        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_object_type_capture_order); i++)
        {
            if (!capture_objects(s_object_type_capture_order[i], capture_params, remapper))
                goto handle_error;
        }
    }

    m_is_valid = true;

    VOGL_CHECK_GL_ERROR;

    vogl_verbose_printf("Capture succeeded\n");

    return true;

handle_error:
    VOGL_CHECK_GL_ERROR;
    vogl_error_printf("Capture failed\n");
    return false;
}

bool vogl_context_snapshot::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    vogl_verbose_printf("Remapping object handles and program locations\n");

    if (!m_general_state.remap_handles(remapper))
        return false;

    if (m_arb_program_environment_state.is_valid())
    {
        if (!m_arb_program_environment_state.remap_handles(remapper))
            return false;
    }

    for (uint32_t i = 0; i < m_object_ptrs.size(); i++)
    {
        uint64_t orig_handle = m_object_ptrs[i]->get_snapshot_handle();

        if (!m_object_ptrs[i]->remap_handles(remapper))
            return false;

        uint64_t new_handle = m_object_ptrs[i]->get_snapshot_handle();
        VOGL_NOTE_UNUSED(new_handle);

        if (orig_handle)
        {
            VOGL_ASSERT(new_handle);

            // TODO: Validate the inverse mapping is good to catch silly bugs
        }
    }

    vogl_verbose_printf("Remap complete\n");

    return true;
}

bool vogl_context_snapshot::capture_objects(vogl_gl_object_state_type state_type, const vogl_capture_context_params &capture_params, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    vogl_debug_printf("Capturing %ss\n", get_gl_object_state_type_str(state_type));

    uint32_t total = 0;

    if ((state_type == cGLSTVertexArray) && (m_context_info.is_compatibility_profile()))
    {
        // Save the default VAO.
        vogl_gl_object_state *p = vogl_gl_object_state_factory(cGLSTVertexArray);
        VOGL_VERIFY(p);

        bool success = p->snapshot(m_context_info, remapper, 0, GL_NONE);
        if (!success)
        {
            vogl_delete(p);
            return false;
        }

        m_object_ptrs.push_back(p);

        total++;
    }

    if (state_type == cGLSTSync)
    {
        m_object_ptrs.reserve(m_object_ptrs.size() + capture_params.m_syncs.size());

        for (vogl_sync_hash_set::const_iterator it = capture_params.m_syncs.begin(); it != capture_params.m_syncs.end(); ++it)
        {
            uint64_t handle = it->first;

            if (!GL_ENTRYPOINT(glIsSync)(vogl_handle_to_sync(handle)))
            {
                VOGL_ASSERT_ALWAYS;
                continue;
            }

            vogl_gl_object_state *p = vogl_gl_object_state_factory(state_type);
            VOGL_VERIFY(p);

            bool success = p->snapshot(m_context_info, remapper, handle, GL_NONE);
            if (!success)
            {
                vogl_delete(p);
                return false;
            }

            m_object_ptrs.push_back(p);
            total++;
        }
    }
    else
    {
        vogl::vector<std::pair<GLuint, GLenum> > handles_to_capture;
        handles_to_capture.reserve(4096);

        switch (state_type)
        {
            case cGLSTARBProgram:
            {
                for (vogl_handle_hash_map::const_iterator it = capture_params.m_arb_program_targets.begin(); it != capture_params.m_arb_program_targets.end(); ++it)
                    handles_to_capture.push_back(*it);
                break;
            }
            case cGLSTBuffer:
            {
                for (vogl_handle_hash_map::const_iterator it = capture_params.m_buffer_targets.begin(); it != capture_params.m_buffer_targets.end(); ++it)
                    handles_to_capture.push_back(*it);
                break;
            }
            case cGLSTTexture:
            {
                for (uint32_t i = 0; i < capture_params.m_textures.size(); i++)
                {
                    const vogl_handle_tracker::handle_def &def = capture_params.m_textures[i];
                    if (def.is_valid())
                        handles_to_capture.push_back(std::make_pair(def.get_inv_handle(), def.get_target()));
                }
                break;
            }
            case cGLSTRenderbuffer:
            {
                for (uint32_t i = 0; i < capture_params.m_rbos.size(); i++)
                {
                    const vogl_handle_tracker::handle_def &def = capture_params.m_rbos[i];
                    if (def.is_valid())
                        handles_to_capture.push_back(std::make_pair(def.get_inv_handle(), def.get_target()));
                }

                break;
            }
            case cGLSTFramebuffer:
            {
                for (vogl_handle_hash_set::const_iterator it = capture_params.m_framebuffers.begin(); it != capture_params.m_framebuffers.end(); ++it)
                    handles_to_capture.push_back(std::make_pair(it->first, GL_NONE));

                break;
            }
            case cGLSTQuery:
            {
                for (vogl_handle_hash_map::const_iterator it = capture_params.m_query_targets.begin(); it != capture_params.m_query_targets.end(); ++it)
                    handles_to_capture.push_back(*it);
                break;
            }
            case cGLSTShader:
            {
                for (uint32_t i = 0; i < capture_params.m_objs.size(); i++)
                {
                    const vogl_handle_tracker::handle_def &def = capture_params.m_objs[i];
                    if ((def.is_valid()) && (def.get_target() == VOGL_SHADER_OBJECT))
                        handles_to_capture.push_back(std::make_pair(def.get_inv_handle(), def.get_target()));
                }

                break;
            }
            case cGLSTProgram:
            {
                for (uint32_t i = 0; i < capture_params.m_objs.size(); i++)
                {
                    const vogl_handle_tracker::handle_def &def = capture_params.m_objs[i];
                    if ((def.is_valid()) && (def.get_target() == VOGL_PROGRAM_OBJECT))
                    {
                        GLuint handle = def.get_inv_handle();

                        if (capture_params.m_filter_program_handles)
                        {
                            if (!capture_params.m_program_handles_filter.contains(handle))
                                continue;
                        }

                        handles_to_capture.push_back(std::make_pair(handle, def.get_target()));
                    }
                }

                break;
            }
            case cGLSTProgramPipeline:
            {
                for (uint32_t i = 0; i < capture_params.m_program_pipelines.size(); i++)
                {
                    const vogl_handle_tracker::handle_def &def = capture_params.m_program_pipelines[i];
                    if (def.is_valid())
                        handles_to_capture.push_back(std::make_pair(def.get_inv_handle(), def.get_target()));
                }

                break;
            }
            case cGLSTSampler:
            {
                for (vogl_handle_hash_set::const_iterator it = capture_params.m_samplers.begin(); it != capture_params.m_samplers.end(); ++it)
                    handles_to_capture.push_back(std::make_pair(it->first, GL_NONE));

                break;
            }
            case cGLSTVertexArray:
            {
                for (vogl_handle_hash_set::const_iterator it = capture_params.m_vaos.begin(); it != capture_params.m_vaos.end(); ++it)
                    handles_to_capture.push_back(std::make_pair(it->first, GL_NONE));

                break;
            }
            default:
            {
                VOGL_VERIFY(0);
                return false;
            }
        }

        m_object_ptrs.reserve(m_object_ptrs.size() + handles_to_capture.size());

        for (uint32_t i = 0; i < handles_to_capture.size(); ++i)
        {
            GLuint handle = handles_to_capture[i].first;
            GLenum target = handles_to_capture[i].second;

            vogl_gl_object_state *p = vogl_gl_object_state_factory(state_type);
            VOGL_VERIFY(p);

            bool success = p->snapshot(m_context_info, remapper, handle, target);
            if (!success)
            {
                vogl_delete(p);
                return false;
            }

            if (state_type == cGLSTProgram)
            {
                vogl_program_state *pProg = static_cast<vogl_program_state *>(p);

                const vogl_program_state *pLink_snapshot = capture_params.m_linked_programs.find_snapshot(handle);
                if (pLink_snapshot)
                {
                    vogl_unique_ptr<vogl_program_state> pLink_snapshot_clone(vogl_new(vogl_program_state, *pLink_snapshot));

                    pProg->set_link_time_snapshot(pLink_snapshot_clone);
                }
                else if (pProg->get_link_status())
                {
                    vogl_error_printf("GL program %u was snapshotted with a successful link status, but the link snapshot shadow doesn't contain this program!\n", handle);
                }
            }
            else if (state_type == cGLSTBuffer)
            {
                // Determine if this buffer has been mapped. I don't expect this array to be very big (typically empty) so a simple search is fine.
                uint32_t j;
                for (j = 0; j < capture_params.m_mapped_buffers.size(); j++)
                    if (capture_params.m_mapped_buffers[j].m_buffer == handle)
                        break;

                if (j < capture_params.m_mapped_buffers.size())
                {
                    const vogl_mapped_buffer_desc &map_desc = capture_params.m_mapped_buffers[j];

                    vogl_buffer_state *pBuf_state = static_cast<vogl_buffer_state *>(p);

                    pBuf_state->set_mapped_buffer_snapshot_state(map_desc);
                }
            }

            m_object_ptrs.push_back(p);
            total++;
        }
    }

    VOGL_CHECK_GL_ERROR;

    vogl_debug_printf("Found %u %ss\n", total, get_gl_object_state_type_str(state_type));

    return true;
}

static bool vogl_object_ptr_sorter(const vogl_gl_object_state *pLHS, vogl_gl_object_state *pRHS)
{
    return pLHS->get_snapshot_handle() < pRHS->get_snapshot_handle();
}

void vogl_context_snapshot::get_all_objects_of_category(vogl_gl_object_state_type state_type, vogl_gl_object_state_ptr_vec &obj_ptr_vec) const
{
    VOGL_FUNC_TRACER

    obj_ptr_vec.resize(0);

    for (uint32_t i = 0; i < m_object_ptrs.size(); i++)
        if (m_object_ptrs[i]->get_type() == state_type)
            obj_ptr_vec.push_back(m_object_ptrs[i]);

    obj_ptr_vec.sort(vogl_object_ptr_sorter);
}

bool vogl_context_snapshot::serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    if (!m_context_desc.serialize(node.add_object("context_desc"), blob_manager))
        return false;

    if (m_context_info.is_valid() && !m_context_info.serialize(node.add_object("context_info"), blob_manager))
        return false;

    if (!m_general_state.serialize(node.add_object("general_state"), blob_manager))
        return false;

    if (!m_display_list_state.serialize(node.add_object("display_lists"), blob_manager, pCtypes))
        return false;

    if (m_texenv_state.is_valid() && !m_texenv_state.serialize(node.add_object("tex_env_gen_state"), blob_manager))
        return false;

    if (m_material_state.is_valid() && !m_material_state.serialize(node.add_object("material_state"), blob_manager))
        return false;

    if (m_light_state.is_valid() && !m_light_state.serialize(node.add_object("light_state"), blob_manager))
        return false;

    if (m_matrix_state.is_valid() && !m_matrix_state.serialize(node.add_array("matrix_state"), blob_manager))
        return false;

    if (m_polygon_stipple_state.is_valid() && !m_polygon_stipple_state.serialize(node.add_array("polygon_stipple"), blob_manager))
        return false;

    if (m_current_vertex_attrib_state.is_valid() && !m_current_vertex_attrib_state.serialize(node.add_array("current_vertex_attribs"), blob_manager))
        return false;

    if (m_arb_program_environment_state.is_valid() && !m_arb_program_environment_state.serialize(node.add_array("arb_program_environment_state"), blob_manager))
        return false;

    if (m_object_ptrs.size())
    {
        json_node &objects_node = node.add_object("state_objects");

        vogl_gl_object_state_ptr_vec obj_ptrs;

        for (vogl_gl_object_state_type state_type = static_cast<vogl_gl_object_state_type>(0); state_type < cGLSTTotalTypes; state_type = static_cast<vogl_gl_object_state_type>(state_type + 1))
        {
            get_all_objects_of_category(state_type, obj_ptrs);
            if (obj_ptrs.is_empty())
                continue;

            json_node &array_node = objects_node.add_array(get_gl_object_state_type_str(state_type));

            for (uint32_t i = 0; i < obj_ptrs.size(); i++)
            {
                json_node &new_obj = array_node.add_object();
                if (!obj_ptrs[i]->serialize(new_obj, blob_manager))
                    return false;
            }
        }
    }

    return true;
}

bool vogl_context_snapshot::deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes)
{
    VOGL_FUNC_TRACER

    clear();

    if ((!vogl_json_deserialize_obj(node, blob_manager, "context_desc", m_context_desc)) ||
        (!vogl_json_deserialize_obj(node, blob_manager, "general_state", m_general_state)))
    {
        clear();
        return false;
    }

    if (node.has_object("context_info") && !vogl_json_deserialize_obj(node, blob_manager, "context_info", m_context_info))
    {
        clear();
        return false;
    }

    if (node.has_object("display_lists") && !m_display_list_state.deserialize(*node.find_child_object("display_lists"), blob_manager, pCtypes))
    {
        clear();
        return false;
    }

    if (node.has_key("tex_env_gen_state") && !vogl_json_deserialize_obj(node, blob_manager, "tex_env_gen_state", m_texenv_state))
    {
        clear();
        return false;
    }

    if (node.has_key("material_state") && !vogl_json_deserialize_obj(node, blob_manager, "material_state", m_material_state))
    {
        clear();
        return false;
    }

    if (node.has_key("light_state") && !vogl_json_deserialize_obj(node, blob_manager, "light_state", m_light_state))
    {
        clear();
        return false;
    }

    if (node.has_key("matrix_state") && !vogl_json_deserialize_array(node, blob_manager, "matrix_state", m_matrix_state))
    {
        clear();
        return false;
    }

    if (node.has_key("polygon_stipple") && !vogl_json_deserialize_obj(node, blob_manager, "polygon_stipple", m_polygon_stipple_state))
    {
        clear();
        return false;
    }

    if (node.has_key("current_vertex_attribs") && !vogl_json_deserialize_array(node, blob_manager, "current_vertex_attribs", m_current_vertex_attrib_state))
    {
        clear();
        return false;
    }

    if (node.has_key("arb_program_environment_state") && !vogl_json_deserialize_obj(node, blob_manager, "arb_program_environment_state", m_arb_program_environment_state))
    {
        clear();
        return false;
    }

    const json_node *pObjects_node = node.find_child_object("state_objects");
    if (pObjects_node)
    {
        for (uint32_t obj_iter = 0; obj_iter < pObjects_node->size(); obj_iter++)
        {
            const dynamic_string &obj_type_str = pObjects_node->get_key(obj_iter);

            vogl_gl_object_state_type state_type = determine_gl_object_state_type_from_str(obj_type_str.get_ptr());
            if (state_type == cGLSTInvalid)
            {
                vogl_warning_printf("Unknown object state type \"%s\", skipping node\n", obj_type_str.get_ptr());
                continue;
            }

            const json_node *pArray_node = pObjects_node->get_value_as_array(obj_iter);
            if (!pArray_node)
            {
                clear();
                return false;
            }

            m_object_ptrs.reserve(m_object_ptrs.size() + pArray_node->size());

            for (uint32_t i = 0; i < pArray_node->size(); i++)
            {
                const json_node *pObj_node = pArray_node->get_value_as_object(i);
                if (!pObj_node)
                {
                    clear();
                    return false;
                }

                vogl_gl_object_state *pState_obj = vogl_gl_object_state_factory(state_type);
                if (!pState_obj)
                {
                    clear();
                    return false;
                }

                if (!pState_obj->deserialize(*pObj_node, blob_manager))
                {
                    vogl_delete(pState_obj);

                    clear();
                    return false;
                }

                m_object_ptrs.push_back(pState_obj);
            }
        }
    }

    m_is_valid = true;

    return true;
}

vogl_gl_state_snapshot::vogl_gl_state_snapshot()
    : m_window_width(0),
      m_window_height(0),
      m_cur_trace_context(0),
      m_frame_index(0),
      m_gl_call_counter(0),
      m_at_frame_boundary(false),
      m_is_restorable(false),
      m_captured_default_framebuffer(false),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER

    m_uuid.clear();
}

vogl_gl_state_snapshot::~vogl_gl_state_snapshot()
{
    VOGL_FUNC_TRACER

    destroy_contexts();
}

void vogl_gl_state_snapshot::clear()
{
    VOGL_FUNC_TRACER

    m_uuid.clear();
    m_window_width = 0;
    m_window_height = 0;
    m_cur_trace_context = 0;
    m_frame_index = 0;
    m_gl_call_counter = 0;
    m_at_frame_boundary = false;
    m_is_restorable = false;

    m_client_side_vertex_attrib_ptrs.clear();
    m_client_side_array_ptrs.clear();
    m_client_side_texcoord_ptrs.clear();

    destroy_contexts();

    m_default_framebuffer.clear();

    m_captured_default_framebuffer = false;
    m_is_valid = false;
}

// frame_index indicates the beginning of frame X, at a swap boundary
bool vogl_gl_state_snapshot::begin_capture(uint32_t window_width, uint32_t window_height, vogl_trace_ptr_value cur_context, uint32_t frame_index, int64_t gl_call_counter, bool at_frame_boundary)
{
    VOGL_FUNC_TRACER

    clear();

    m_uuid = gen_uuid();
    m_window_width = window_width;
    m_window_height = window_height;
    m_cur_trace_context = cur_context;
    m_frame_index = frame_index;
    m_gl_call_counter = gl_call_counter;
    m_at_frame_boundary = at_frame_boundary;
    m_is_restorable = true;

    m_is_valid = true;
    m_captured_default_framebuffer = false;

    return true;
}

void vogl_gl_state_snapshot::add_client_side_array_ptrs(const vogl_client_side_array_desc_vec &client_side_vertex_attrib_ptrs, const vogl_client_side_array_desc_vec &client_side_array_ptrs, const vogl_client_side_array_desc_vec &client_side_texcoord_ptrs)
{
    VOGL_FUNC_TRACER

    m_client_side_vertex_attrib_ptrs = client_side_vertex_attrib_ptrs;
    m_client_side_array_ptrs = client_side_array_ptrs;
    m_client_side_texcoord_ptrs = client_side_texcoord_ptrs;
}

// TODO: check if the context was ever made current yet (if not the context_info won't be valid), make sure this path works
bool vogl_gl_state_snapshot::capture_context(
    const vogl_context_desc &desc, const vogl_context_info &info, vogl_handle_remapper &remapper,
    const vogl_capture_context_params &capture_params)
{
    VOGL_FUNC_TRACER

    if (!m_captured_default_framebuffer)
    {
        // TODO: Move this!
        vogl_default_framebuffer_attribs fb_attribs;
        fb_attribs.clear();

        bool status = false;
        if (vogl_get_default_framebuffer_attribs(fb_attribs, 0))
        {
            if ((fb_attribs.m_width) && (fb_attribs.m_height))
            {
                status = m_default_framebuffer.snapshot(info, fb_attribs);
            }
        }

        if (!status)
        {
            vogl_error_printf("Failed snapshotting default framebuffer!\n");
        }

        m_captured_default_framebuffer = true;
    }

    vogl_context_snapshot *pSnapshot = vogl_new(vogl_context_snapshot);

    if (!pSnapshot->capture(desc, info, capture_params, remapper))
    {
        m_is_valid = false;

        vogl_delete(pSnapshot);

        return false;
    }

    if (!pSnapshot->remap_handles(remapper))
        return false;

    m_context_ptrs.push_back(pSnapshot);

    return true;
}

bool vogl_gl_state_snapshot::end_capture()
{
    VOGL_FUNC_TRACER

    return m_is_valid;
}

bool vogl_gl_state_snapshot::serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_uuid.json_serialize(node.add("uuid"));
    node.add_key_value("window_width", m_window_width);
    node.add_key_value("window_height", m_window_height);
    node.add_key_value("cur_trace_context", m_cur_trace_context);
    node.add_key_value("frame_index", m_frame_index);
    node.add_key_value("gl_call_counter", m_gl_call_counter);
    node.add_key_value("at_frame_boundary", m_at_frame_boundary);
    node.add_key_value("is_restorable", m_is_restorable);

    if (!vogl_json_serialize_vec(node, blob_manager, "client_side_vertex_attrib_ptrs", m_client_side_vertex_attrib_ptrs))
        return false;
    if (!vogl_json_serialize_vec(node, blob_manager, "client_side_array_ptrs", m_client_side_array_ptrs))
        return false;
    if (!vogl_json_serialize_vec(node, blob_manager, "client_side_texcoord_ptrs", m_client_side_texcoord_ptrs))
        return false;

    if (!vogl_json_serialize_ptr_vec(node, blob_manager, "context_snapshots", m_context_ptrs, pCtypes))
        return false;

    if (m_default_framebuffer.is_valid())
    {
        if (!m_default_framebuffer.serialize(node.add_object("default_framebuffer"), blob_manager))
            return false;
    }

    return true;
}

bool vogl_gl_state_snapshot::deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes)
{
    VOGL_FUNC_TRACER

    clear();

    if (!m_uuid.json_deserialize(node, "uuid"))
    {
        // For old trace files that don't have uuid's
        m_uuid = gen_uuid();
    }

    m_window_width = node.value_as_uint32("window_width");
    m_window_height = node.value_as_uint32("window_height");

    if ((!m_window_width) || (!m_window_height))
    {
        clear();
        return false;
    }

    m_cur_trace_context = static_cast<vogl_trace_ptr_value>(node.value_as_uint64("cur_trace_context"));
    m_frame_index = node.value_as_uint32("frame_index");
    m_gl_call_counter = node.value_as_int64("gl_call_counter");
    m_at_frame_boundary = node.value_as_bool("at_frame_boundary");
    m_is_restorable = node.value_as_bool("is_restorable", true);

    if (!vogl_json_deserialize_vec(node, blob_manager, "client_side_vertex_attrib_ptrs", m_client_side_vertex_attrib_ptrs))
        return false;
    if (!vogl_json_deserialize_vec(node, blob_manager, "client_side_array_ptrs", m_client_side_array_ptrs))
        return false;
    if (!vogl_json_deserialize_vec(node, blob_manager, "client_side_texcoord_ptrs", m_client_side_texcoord_ptrs))
        return false;

    if (!vogl_json_deserialize_ptr_vec(node, blob_manager, "context_snapshots", m_context_ptrs, pCtypes))
        return false;

    if  (node.has_object("default_framebuffer"))
    {
        if (!m_default_framebuffer.deserialize(*node.find_child_object("default_framebuffer"), blob_manager))
            return false;
    }

    m_is_valid = true;

    return true;
}
