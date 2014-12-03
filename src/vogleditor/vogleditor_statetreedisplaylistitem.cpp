#include "vogleditor_statetreedisplaylistitem.h"
#include "vogl_display_list_state.h"

vogleditor_stateTreeDisplaylistItem::vogleditor_stateTreeDisplaylistItem(QString name, QString value, vogleditor_stateTreeItem *parentNode, vogl_display_list_state *pState)
    : vogleditor_stateTreeItem(name, value, parentNode),
      m_pState(pState),
      m_pDiffBaseState(NULL)
{
    // TODO: (Richg) We don't currently support snapshotting while composing a display list, so the current display list and mode can't be printed.
    //this->appendChild(new vogleditor_stateTreeItem("GL_LIST_INDEX", STR_INT(pState->get_current_display_list()), this));
    //this->appendChild(new vogleditor_stateTreeItem("GL_LIST_MODE", STR_ENUM(pState->get_current_display_list_mode()), this));

    vogl_display_list_map &displayLists = pState->get_display_list_map();
    if (displayLists.size() == 0)
    {
        return;
    }

    QString tmp;

    vogleditor_stateTreeItem *pDisplayListMapNode = new vogleditor_stateTreeItem("Existing Lists", "", this);
    this->appendChild(pDisplayListMapNode);
    for (vogl_display_list_map::iterator iter = displayLists.begin(); iter != displayLists.end(); iter++)
    {
        vogl_display_list *pDisplayList = &(iter->second);
        if (pDisplayList->is_valid())
        {
            vogleditor_stateTreeItem *pDisplayListNode = new vogleditor_stateTreeItem(tmp.sprintf("%d", iter->first), tmp.sprintf("%u calls", pDisplayList->get_handle()), pDisplayListMapNode);
            pDisplayListMapNode->appendChild(pDisplayListNode);

            vogl_trace_packet_array &packets = pDisplayList->get_packets();
            for (uint i = 0; i < packets.size(); i++)
            {
                const vogl_trace_gl_entrypoint_packet &packet = packets.get_packet<vogl_trace_gl_entrypoint_packet>(i);
                const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[packet.m_entrypoint_id];

                QString funcCall = entrypoint_desc.m_pName;

                // format parameters
                if (entrypoint_desc.m_num_params > 0)
                {
                    funcCall.append("(...)");
                }
                else
                {
                    funcCall.append("()");
                }

                pDisplayListNode->appendChild(new vogleditor_stateTreeItem(tmp.sprintf("%u", i), funcCall, pDisplayListNode));
            }
        }
        else
        {
            VOGL_ASSERT(!"Encountered an invalid displaylist.");
        }
    }
}
