
#include "vogleditor_apicalltreeitem.h"
#include "vogleditor_apicallitem.h"

#include "vogleditor_tracereplayer.h"

#include "vogl_find_files.h"
#include "vogl_file_utils.h"
#include "vogl_gl_replayer.h"
#include "vogleditor_output.h"

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

//----------------------------------------------------------------------------------------------------------------------
// X11_Pending - from SDL
//----------------------------------------------------------------------------------------------------------------------
static int X11_Pending(Display *display)
{
   VOGL_FUNC_TRACER

   /* Flush the display connection and look to see if events are queued */
   XFlush(display);
   if (XEventsQueued(display, QueuedAlready))
   {
      return(1);
   }

   /* More drastic measures are required -- see if X is ready to talk */
   {
      static struct timeval zero_time;	/* static == 0 */
      int x11_fd;
      fd_set fdset;

      x11_fd = ConnectionNumber(display);
      FD_ZERO(&fdset);
      FD_SET(x11_fd, &fdset);
      if (select(x11_fd+1, &fdset, NULL, NULL, &zero_time) == 1)
      {
         return(XPending(display));
      }
   }

   /* Oh well, nothing is ready .. */
   return(0);
}

bool vogleditor_traceReplayer::process_x_events()
{
    while (X11_Pending(m_window.get_display()))
    {
       XEvent newEvent;

       // Watch for new X events
       XNextEvent(m_window.get_display(), &newEvent);

       switch (newEvent.type)
       {
          case MapNotify:
          {
             m_pTraceReplayer->update_window_dimensions();
             break;
          }
          case ConfigureNotify:
          {
             m_pTraceReplayer->update_window_dimensions();
             break;
          }
          case DestroyNotify:
          {
             vogl_message_printf("Exiting\n");
             return false;
             break;
          }
          case ClientMessage:
          {
             if(newEvent.xclient.data.l[0] == (int)m_wmDeleteMessage)
             {
                vogl_message_printf("Exiting\n");
                return false;
             }

             break;
          }
          default: break;
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
        if (process_x_events())
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
            if (process_x_events())
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
                result = take_state_snapshot_if_needed(ppNewSnapshot, apiCallNumber);
            }
        }

        // replay the trace packet
        if (!status_indicates_end(status))
            status = m_pTraceReplayer->process_next_packet(*pTrace_packet);

        // if that was successful, check to see if a state snapshot is needed
        if (!status_indicates_end(status))
        {
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
            if (process_x_events() == false)
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

   XSelectInput(m_window.get_display(), m_window.get_xwindow(),
                EnterWindowMask | LeaveWindowMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | FocusChangeMask | KeyPressMask | KeyReleaseMask | PropertyChangeMask | StructureNotifyMask | KeymapStateMask);

   m_wmDeleteMessage = XInternAtom(m_window.get_display(), "WM_DELETE_WINDOW", False);
   XSetWMProtocols(m_window.get_display(), m_window.get_xwindow(), &m_wmDeleteMessage, 1);

   timer tm;
   tm.start();

   vogleditor_tracereplayer_result result = VOGLEDITOR_TRR_SUCCESS;

   for ( ; ; )
   {
      if (process_x_events() == false)
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
