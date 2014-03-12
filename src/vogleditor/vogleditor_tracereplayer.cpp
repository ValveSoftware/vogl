
#include "vogleditor_apicalltreeitem.h"

#include "vogleditor_tracereplayer.h"

#include "vogl_find_files.h"
#include "vogl_file_utils.h"
#include "vogl_gl_replayer.h"


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
        vogl_warning_printf("%s: Waiting for window to resize\n", VOGL_METHOD_NAME);

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
        vogl_error_printf("%s: Replay unable to apply snapshot\n", VOGL_FUNCTION_NAME);
        bStatus = false;
    }

    return bStatus;
}

bool vogleditor_traceReplayer::recursive_replay_apicallTreeItem(vogleditor_apiCallTreeItem* pItem, vogleditor_gl_state_snapshot** ppNewSnapshot, uint64_t apiCallNumber)
{
    bool bStatus = true;

    vogleditor_apiCallItem* pApiCall = pItem->apiCallItem();
    if (pApiCall != NULL)
    {
        vogl_trace_packet* pTrace_packet = pApiCall->getTracePacket();

        vogl_gl_replayer::status_t status = vogl_gl_replayer::cStatusOK;

        // See if a window resize or snapshot is pending. If a window resize is pending we must delay a while and pump X events until the window is resized.
        while (m_pTraceReplayer->get_has_pending_window_resize() || m_pTraceReplayer->get_pending_apply_snapshot())
        {
            // Pump X events in case the window is resizing
            bStatus = process_x_events();
            if (bStatus)
            {
                status = m_pTraceReplayer->process_pending_window_resize();
                if (status != vogl_gl_replayer::cStatusResizeWindow)
                    break;
            }
            else
            {
                // most likely the window wants to close, so let's return
                return false;
            }
        }

        // replay the trace packet
        if (status == vogl_gl_replayer::cStatusOK)
            status = m_pTraceReplayer->process_next_packet(*pTrace_packet);

        // if that was successful, check to see if a state snapshot is needed
        if ((status != vogl_gl_replayer::cStatusHardFailure) && (status != vogl_gl_replayer::cStatusAtEOF))
        {
            if (ppNewSnapshot != NULL)
            {
               // get the snapshot after the selected api call
               if ((!*ppNewSnapshot) && (m_pTraceReplayer->get_last_processed_call_counter() == static_cast<int64_t>(apiCallNumber)))
               {
                  vogl_printf("Taking snapshot on API call # %" PRIu64 "\n", apiCallNumber);

                  *ppNewSnapshot = vogl_new(vogleditor_gl_state_snapshot, m_pTraceReplayer->snapshot_state());

                  if (*ppNewSnapshot == NULL)
                  {
                     vogl_error_printf("Snapshot failed!\n");
                  }
                  else
                  {
                      vogl_printf("Snapshot succeeded\n");
                  }

                  bStatus = false;
               }
            }
        }
        else
        {
            // replaying the trace packet failed, set as error
            vogl_error_printf("%s: unable to replay gl entrypoint at call %" PRIu64 "\n", VOGL_FUNCTION_NAME, pTrace_packet->get_call_counter());
            bStatus = false;
        }
    }

    if (bStatus && pItem->has_snapshot() && pItem->get_snapshot()->is_edited())
    {
        bStatus = applying_snapshot_and_process_resize(pItem->get_snapshot()->get_snapshot());
    }

    if (bStatus)
    {
        for (int i = 0; i < pItem->childCount(); i++)
        {
            bStatus = recursive_replay_apicallTreeItem(pItem->child(i), ppNewSnapshot, apiCallNumber);

            if (!bStatus)
                break;
        }
    }

    return bStatus;
}

bool vogleditor_traceReplayer::replay(vogl_trace_file_reader* m_pTraceReader, vogleditor_apiCallTreeItem* pRootItem, vogleditor_gl_state_snapshot** ppNewSnapshot, uint64_t apiCallNumber, bool endlessMode)
{
   // reset to beginnning of trace file.
   m_pTraceReader->seek_to_frame(0);

   int initial_window_width = 1280;
   int initial_window_height = 1024;

   if (!m_window.open(initial_window_width, initial_window_height))
   {
      vogl_error_printf("%s: Failed opening GL replayer window!\n", VOGL_FUNCTION_NAME);
      return false;
   }

   uint replayer_flags = cGLReplayerForceDebugContexts;
   if (!m_pTraceReplayer->init(replayer_flags, &m_window, m_pTraceReader->get_sof_packet(), m_pTraceReader->get_multi_blob_manager()))
   {
      vogl_error_printf("%s: Failed initializing GL replayer\n", VOGL_FUNCTION_NAME);
      m_window.close();
      return false;
   }

   XSelectInput(m_window.get_display(), m_window.get_xwindow(),
                EnterWindowMask | LeaveWindowMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | FocusChangeMask | KeyPressMask | KeyReleaseMask | PropertyChangeMask | StructureNotifyMask | KeymapStateMask);

   m_wmDeleteMessage = XInternAtom(m_window.get_display(), "WM_DELETE_WINDOW", False);
   XSetWMProtocols(m_window.get_display(), m_window.get_xwindow(), &m_wmDeleteMessage, 1);

   timer tm;
   tm.start();

   bool bStatus = true;

   for ( ; ; )
   {
      if (process_x_events() == false)
      {
          break;
      }

      if (pRootItem->childCount() > 0)
      {
          vogleditor_apiCallTreeItem* pFirstFrame = pRootItem->child(0);

          // if the first snapshot has not been edited, then restore it here, otherwise it will get restored in the recursive call below.
          if (pFirstFrame->has_snapshot() && !pFirstFrame->get_snapshot()->is_edited())
          {
              bStatus = applying_snapshot_and_process_resize(pFirstFrame->get_snapshot()->get_snapshot());
          }

          if (bStatus)
          {
              // replay each API call.
              bStatus = recursive_replay_apicallTreeItem(pRootItem, ppNewSnapshot, apiCallNumber);

              if (bStatus == false)
              {
                 vogl_error_printf("%s: Replay ending abruptly at frame index %u, global api call %" PRIu64 "\n", VOGL_FUNCTION_NAME, m_pTraceReplayer->get_frame_index(), m_pTraceReplayer->get_last_processed_call_counter());
                 break;
              }
              else
              {
                 vogl_message_printf("%s: At trace EOF, frame index %u\n", VOGL_FUNCTION_NAME, m_pTraceReplayer->get_frame_index());
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
   return bStatus;
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
