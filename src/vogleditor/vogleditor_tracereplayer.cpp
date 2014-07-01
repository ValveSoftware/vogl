
#include "vogleditor_apicalltreeitem.h"
#include "vogleditor_apicallitem.h"

#include "vogleditor_tracereplayer.h"

#include "vogl_find_files.h"
#include "vogl_file_utils.h"
#include "vogl_gl_replayer.h"
#include "vogleditor_output.h"

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------
static void *g_actual_libgl_module_handle;

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_proc_address_helper
//----------------------------------------------------------------------------------------------------------------------
static vogl_void_func_ptr_t vogl_get_proc_address_helper(const char *pName)
{
    VOGL_FUNC_TRACER

    vogl_void_func_ptr_t pFunc = g_actual_libgl_module_handle ? reinterpret_cast<vogl_void_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, pName)) : NULL;

#if (VOGL_PLATFORM_HAS_GLX)
    if ((!pFunc) && (GL_ENTRYPOINT(glXGetProcAddress)))
        pFunc = reinterpret_cast<vogl_void_func_ptr_t>(GL_ENTRYPOINT(glXGetProcAddress)(reinterpret_cast<const GLubyte *>(pName)));
#elif (VOGL_PLATFORM_HAS_WGL)
    if ((!pFunc) && (GL_ENTRYPOINT(wglGetProcAddress)))
        pFunc = reinterpret_cast<vogl_void_func_ptr_t>(GL_ENTRYPOINT(wglGetProcAddress)(pName));
#else
#error "Implement vogl_get_proc_address_helper this platform."
#endif

    return pFunc;
}


//----------------------------------------------------------------------------------------------------------------------
// load_gl
//----------------------------------------------------------------------------------------------------------------------
static bool load_gl()
{
    VOGL_FUNC_TRACER

    g_actual_libgl_module_handle = plat_load_system_gl(PLAT_RTLD_LAZY);
    if (!g_actual_libgl_module_handle)
    {
        vogl_error_printf("Failed loading %s!\n", plat_get_system_gl_module_name());
        return false;
    }

#if VOGL_PLATFORM_HAS_GLX
    GL_ENTRYPOINT(glXGetProcAddress) = reinterpret_cast<glXGetProcAddress_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, "glXGetProcAddress"));
    if (!GL_ENTRYPOINT(glXGetProcAddress))
    {
        vogl_error_printf("Failed getting address of glXGetProcAddress() from %s!\n", plat_get_system_gl_module_name());
        return false;
    }
#elif VOGL_PLATFORM_HAS_WGL
    GL_ENTRYPOINT(wglGetProcAddress) = reinterpret_cast<wglGetProcAddress_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, "wglGetProcAddress"));
    if (!GL_ENTRYPOINT(wglGetProcAddress))
    {
        vogl_error_printf("Failed getting address of wglGetProcAddress() from %s!\n", plat_get_system_gl_module_name());
        return false;
    }
#else
#error "Need to implement load_gl for this platform."
#endif

    return true;
}

vogleditor_traceReplayer::vogleditor_traceReplayer()
    : m_pTraceReplayer(vogl_new(vogl_gl_replayer))
{

}

vogleditor_traceReplayer::~vogleditor_traceReplayer()
{
    if (m_pTraceReplayer != NULL)
    {
        vogl_delete(m_pTraceReplayer);
        m_pTraceReplayer = NULL;
    }
}

bool vogleditor_traceReplayer::process_events()
{
    SDL_Event wnd_event;
    while (SDL_PollEvent(&wnd_event))
    {
        switch(wnd_event.type)
        {
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_RESTORED:
            {
                m_pTraceReplayer->update_window_dimensions();
                break;
            }

            case SDL_WINDOWEVENT_MOVED:
            case SDL_WINDOWEVENT_RESIZED:
            {
                m_pTraceReplayer->update_window_dimensions();
                break;
            }

            case SDL_WINDOWEVENT:
            {
                switch(wnd_event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        vogl_message_printf("Exiting\n");
                        return false;
                        break;
                    default:
                        break;
                };
                break;
            }

            default:
                break;
        }
    }


    return true;
}

bool vogleditor_traceReplayer::applying_snapshot_and_process_resize(const vogl_gl_state_snapshot* pSnapshot)
{
    vogl_gl_replayer::status_t status = m_pTraceReplayer->begin_applying_snapshot(pSnapshot, false);

    bool bStatus = true;
    while (status == vogl_gl_replayer::cStatusResizeWindow)
    {
        vogleditor_output_message("Waiting for replay window to resize.");

        // Pump X events in case the window is resizing
        if (process_events())
        {
            status = m_pTraceReplayer->process_pending_window_resize();
        }
        else
        {
            bStatus = false;
            break;
        }
    }

    if (bStatus && status != vogl_gl_replayer::cStatusOK)
    {
        vogleditor_output_error("Replay unable to apply snapshot");
        bStatus = false;
    }

    return bStatus;
}

vogleditor_tracereplayer_result vogleditor_traceReplayer::take_state_snapshot_if_needed(vogleditor_gl_state_snapshot** ppNewSnapshot, uint32_t apiCallNumber)
{
    vogleditor_tracereplayer_result result = VOGLEDITOR_TRR_SUCCESS;

    if (ppNewSnapshot != NULL)
    {
       // get the snapshot after the selected api call
       if ((!*ppNewSnapshot) && (m_pTraceReplayer->get_last_processed_call_counter() == static_cast<int64_t>(apiCallNumber)))
       {
          dynamic_string info;
          vogleditor_output_message(info.format("Taking snapshot on API call # %u...", apiCallNumber).c_str());

          vogl_gl_state_snapshot* pNewSnapshot = m_pTraceReplayer->snapshot_state();
          if (pNewSnapshot == NULL)
          {
              result = VOGLEDITOR_TRR_ERROR;
              vogleditor_output_error("... snapshot failed!");
          }
          else
          {
              result = VOGLEDITOR_TRR_SNAPSHOT_SUCCESS;
              vogleditor_output_message("... snapshot succeeded!\n");
              *ppNewSnapshot = vogl_new(vogleditor_gl_state_snapshot, pNewSnapshot);
              if (*ppNewSnapshot == NULL)
              {
                 result = VOGLEDITOR_TRR_ERROR;
                 vogleditor_output_error("Allocating memory for snapshot container failed!");
                 vogl_delete(pNewSnapshot);
              }
          }
       }
    }

    return result;
}

inline bool status_indicates_end(vogl_gl_replayer::status_t status)
{
    return (status == vogl_gl_replayer::cStatusHardFailure) || (status == vogl_gl_replayer::cStatusAtEOF);
}

vogleditor_tracereplayer_result vogleditor_traceReplayer::recursive_replay_apicallTreeItem(vogleditor_apiCallTreeItem* pItem, vogleditor_gl_state_snapshot** ppNewSnapshot, uint64_t apiCallNumber)
{
    vogleditor_tracereplayer_result result = VOGLEDITOR_TRR_SUCCESS;
    vogleditor_apiCallItem* pApiCall = pItem->apiCallItem();
    if (pApiCall != NULL)
    {
        vogl_trace_packet* pTrace_packet = pApiCall->getTracePacket();

        vogl_gl_replayer::status_t status = vogl_gl_replayer::cStatusOK;

        // See if a window resize or snapshot is pending. If a window resize is pending we must delay a while and pump X events until the window is resized.
        while (m_pTraceReplayer->get_has_pending_window_resize() || m_pTraceReplayer->get_pending_apply_snapshot())
        {
            // Pump X events in case the window is resizing
            if (process_events())
            {
                status = m_pTraceReplayer->process_pending_window_resize();
                if (status != vogl_gl_replayer::cStatusResizeWindow)
                    break;
            }
            else
            {
                // most likely the window wants to close, so let's return
                return VOGLEDITOR_TRR_USER_EXIT;
            }
        }

        // process pending trace packets (this could include glXMakeCurrent)
        if (!status_indicates_end(status) && m_pTraceReplayer->has_pending_packets())
        {
            status = m_pTraceReplayer->process_pending_packets();

            // if that was successful, check to see if a state snapshot is needed
            if (!status_indicates_end(status))
            {
                // update gl entrypoints if needed
                if (vogl_is_make_current_entrypoint(pTrace_packet->get_entrypoint_id()) && load_gl())
                {
                    vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper);
                }

                result = take_state_snapshot_if_needed(ppNewSnapshot, apiCallNumber);
            }
        }

        // replay the trace packet
        if (!status_indicates_end(status))
            status = m_pTraceReplayer->process_next_packet(*pTrace_packet);

        // if that was successful, check to see if a state snapshot is needed
        if (!status_indicates_end(status))
        {
            // update gl entrypoints if needed
            if (vogl_is_make_current_entrypoint(pTrace_packet->get_entrypoint_id()) && load_gl())
            {
                vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper);
            }

            result = take_state_snapshot_if_needed(ppNewSnapshot, apiCallNumber);
        }
        else
        {
            // replaying the trace packet failed, set as error
            result = VOGLEDITOR_TRR_ERROR;
            dynamic_string info;
            vogleditor_output_error(info.format("Unable to replay gl entrypoint at call %" PRIu64, pTrace_packet->get_call_counter()).c_str());
        }
    }

    if (result == VOGLEDITOR_TRR_SUCCESS && pItem->has_snapshot() && pItem->get_snapshot()->is_edited() && pItem->get_snapshot()->is_valid())
    {
        if(applying_snapshot_and_process_resize(pItem->get_snapshot()->get_snapshot()))
        {
            result = VOGLEDITOR_TRR_SUCCESS;
        }
    }

    if (result == VOGLEDITOR_TRR_SUCCESS)
    {
        for (int i = 0; i < pItem->childCount(); i++)
        {
            result = recursive_replay_apicallTreeItem(pItem->child(i), ppNewSnapshot, apiCallNumber);

            if (result != VOGLEDITOR_TRR_SUCCESS)
                break;

            // Pump X events in case the window is resizing
            if (process_events() == false)
            {
                // most likely the window wants to close, so let's return
                return VOGLEDITOR_TRR_USER_EXIT;
            }
        }
    }

    return result;
}

vogleditor_tracereplayer_result vogleditor_traceReplayer::replay(vogl_trace_file_reader* m_pTraceReader, vogleditor_apiCallTreeItem* pRootItem, vogleditor_gl_state_snapshot** ppNewSnapshot, uint64_t apiCallNumber, bool endlessMode)
{
   // reset to beginnning of trace file.
   m_pTraceReader->seek_to_frame(0);

   int initial_window_width = 1280;
   int initial_window_height = 1024;

   if (!m_window.open(initial_window_width, initial_window_height))
   {
      vogleditor_output_error("Failed opening GL replayer window!");
      return VOGLEDITOR_TRR_ERROR;
   }

   uint replayer_flags = cGLReplayerForceDebugContexts;
   if (!m_pTraceReplayer->init(replayer_flags, &m_window, m_pTraceReader->get_sof_packet(), m_pTraceReader->get_multi_blob_manager()))
   {
      vogleditor_output_error("Failed initializing GL replayer!");
      m_window.close();
      return VOGLEDITOR_TRR_ERROR;
   }

   timer tm;
   tm.start();

   vogleditor_tracereplayer_result result = VOGLEDITOR_TRR_SUCCESS;

   for ( ; ; )
   {
      if (process_events() == false)
      {
          result = VOGLEDITOR_TRR_USER_EXIT;
          break;
      }

      if (pRootItem->childCount() > 0)
      {
          vogleditor_apiCallTreeItem* pFirstFrame = pRootItem->child(0);

          bool bStatus = true;
          // if the first snapshot has not been edited, then restore it here, otherwise it will get restored in the recursive call below.
          if (pFirstFrame->has_snapshot() && !pFirstFrame->get_snapshot()->is_edited())
          {
              bStatus = applying_snapshot_and_process_resize(pFirstFrame->get_snapshot()->get_snapshot());
          }

          if (bStatus)
          {
              // replay each API call.
              result = recursive_replay_apicallTreeItem(pRootItem, ppNewSnapshot, apiCallNumber);

              if (result == VOGLEDITOR_TRR_ERROR)
              {
                  QString msg = QString("Replay ending abruptly at frame index %1, global api call %2").arg(m_pTraceReplayer->get_frame_index()).arg(m_pTraceReplayer->get_last_processed_call_counter());
                  vogleditor_output_error(msg.toStdString().c_str());
                  break;
              }
              else if (result == VOGLEDITOR_TRR_SNAPSHOT_SUCCESS)
              {
                  break;
              }
              else if (result == VOGLEDITOR_TRR_USER_EXIT)
              {
                  vogleditor_output_message("Replay stopped");
                  break;
              }
              else
              {
                  QString msg = QString("At trace EOF, frame index %1").arg(m_pTraceReplayer->get_frame_index());
                  vogleditor_output_message(msg.toStdString().c_str());
                  if (!endlessMode)
                  {
                      break;
                  }
              }
          }
          else
          {
              break;
          }
      }
   }

   m_pTraceReplayer->deinit();
   m_window.close();
   return result;
}

bool vogleditor_traceReplayer::pause()
{
    VOGL_ASSERT(!"Not implemented");
    return false;
}

bool vogleditor_traceReplayer::restart()
{
    VOGL_ASSERT(!"Not implemented");
    return false;
}

bool vogleditor_traceReplayer::trim()
{
    VOGL_ASSERT(!"Not implemented");
    return false;
}

bool vogleditor_traceReplayer::stop()
{
    VOGL_ASSERT(!"Not implemented");
    return false;
}
