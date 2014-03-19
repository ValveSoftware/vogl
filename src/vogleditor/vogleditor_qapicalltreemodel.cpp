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

#include "vogleditor_qapicalltreemodel.h"

#include "vogl_common.h"
#include "vogl_trace_file_reader.h"
#include "vogl_trace_packet.h"
#include "vogl_trace_stream_types.h"
#include "vogleditor_gl_state_snapshot.h"

vogleditor_QApiCallTreeModel::vogleditor_QApiCallTreeModel(vogl_trace_file_reader* reader, QObject *parent)
    : QAbstractItemModel(parent)
{
    m_rootItem = vogl_new(vogleditor_apiCallTreeItem, this);
    setupModelData(reader, m_rootItem);
}

vogleditor_QApiCallTreeModel::~vogleditor_QApiCallTreeModel()
{
   if (m_rootItem != NULL)
   {
      vogl_delete(m_rootItem);
      m_rootItem = NULL;
   }

   m_itemList.clear();
}

// TODO: return bool
void vogleditor_QApiCallTreeModel::setupModelData(vogl_trace_file_reader* pTrace_reader, vogleditor_apiCallTreeItem *parent)
{
   const vogl_trace_stream_start_of_file_packet &sof_packet = pTrace_reader->get_sof_packet();
   VOGL_NOTE_UNUSED(sof_packet);

   uint64_t total_swaps = 0;
   // TODO probably need to handle eof_packet
   //bool found_eof_packet = false;

   // make a PendingFrame node to hold the api calls
   // this will remain in the pending state until the first
   // api call is seen, then it will be made the CurFrame and
   // appended to the parent
   vogleditor_frameItem* pCurFrame = NULL;
   vogleditor_apiCallTreeItem* pCurParent = parent;

   // Make a PendingSnapshot that may or may not be populated when reading the trace.
   // This snapshot will be assigned to the next API call that occurs.
   vogleditor_gl_state_snapshot* pPendingSnapshot = NULL;

   m_trace_ctypes.init(pTrace_reader->get_sof_packet().m_pointer_sizes);

   for ( ; ; )
   {
      vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

      if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
      {
         vogl_error_printf("Failed reading from trace file!\n");
         break;
      }

      if (read_status == vogl_trace_file_reader::cEOF)
      {
         vogl_printf("At trace file EOF on swap %" PRIu64 "\n", total_swaps);
         break;
      }

      const vogl::vector<uint8> &packet_buf = pTrace_reader->get_packet_buf(); VOGL_NOTE_UNUSED(packet_buf);

      const vogl_trace_stream_packet_base &base_packet = pTrace_reader->get_base_packet(); VOGL_NOTE_UNUSED(base_packet);
      const vogl_trace_gl_entrypoint_packet *pGL_packet = NULL;

      if (pTrace_reader->get_packet_type() == cTSPTGLEntrypoint)
      {
         vogl_trace_packet* pTrace_packet = vogl_new(vogl_trace_packet, &m_trace_ctypes);

         if (!pTrace_packet->deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
         {
            console::error("%s: Failed parsing GL entrypoint packet\n", VOGL_FUNCTION_NAME);
            break;
         }

         pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
         gl_entrypoint_id_t entrypoint_id = static_cast<gl_entrypoint_id_t>(pGL_packet->m_entrypoint_id);

         if (entrypoint_id == VOGL_ENTRYPOINT_glInternalTraceCommandRAD)
         {
            // Check if this is a state snapshot.
            // This is entirely optional since the client is designed to dynamically get new snapshots
            // if they don't exist.
            GLuint cmd = pTrace_packet->get_param_value<GLuint>(0);
            GLuint size = pTrace_packet->get_param_value<GLuint>(1); VOGL_NOTE_UNUSED(size);

            if (cmd == cITCRKeyValueMap)
            {
               key_value_map &kvm = pTrace_packet->get_key_value_map();

               dynamic_string cmd_type(kvm.get_string("command_type"));
               if (cmd_type == "state_snapshot")
               {
                  dynamic_string id(kvm.get_string("binary_id"));
                  if (id.is_empty())
                  {
                     vogl_warning_printf("%s: Missing binary_id field in glInternalTraceCommandRAD key_valye_map command type: \"%s\"\n", VOGL_FUNCTION_NAME, cmd_type.get_ptr());
                     continue;
                  }

                  uint8_vec snapshot_data;
                  {
                     timed_scope ts("get_multi_blob_manager().get");
                     if (!pTrace_reader->get_multi_blob_manager().get(id, snapshot_data) || (snapshot_data.is_empty()))
                     {
                        vogl_warning_printf("%s: Failed reading snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                        continue;
                     }
                  }

                  json_document doc;
                  {
                     timed_scope ts("doc.binary_deserialize");
                     if (!doc.binary_deserialize(snapshot_data) || (!doc.get_root()))
                     {
                        vogl_warning_printf("%s: Failed deserializing JSON snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                        continue;
                     }
                  }

                  vogl_gl_state_snapshot* pGLSnapshot = vogl_new(vogl_gl_state_snapshot);
                  pPendingSnapshot = vogl_new(vogleditor_gl_state_snapshot, pGLSnapshot);

                  timed_scope ts("pPendingSnapshot->deserialize");
                  if (!pPendingSnapshot->get_snapshot()->deserialize(*doc.get_root(), pTrace_reader->get_multi_blob_manager(), &m_trace_ctypes))
                  {
                     vogl_delete(pPendingSnapshot);
                     pPendingSnapshot = NULL;

                     vogl_warning_printf("%s: Failed deserializing snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                  }
               }
            }

            continue;
         }

         const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_id];

         QString funcCall = entrypoint_desc.m_pName;

         // format parameters
         funcCall.append("( ");
         dynamic_string paramStr;
         for (uint param_index = 0; param_index < pTrace_packet->total_params(); param_index++)
         {
            if (param_index != 0)
               funcCall.append(", ");

            paramStr.clear();
            pTrace_packet->pretty_print_param(paramStr, param_index, false);

            funcCall.append(paramStr.c_str());
         }
         funcCall.append(" )");

         if (pTrace_packet->has_return_value())
         {
            funcCall.append(" = ");
            paramStr.clear();
            pTrace_packet->pretty_print_return_value(paramStr, false);
            funcCall.append(paramStr.c_str());
         }

         // if we don't have a current frame, make a new frame node
         // and append it to the parent
         if (pCurFrame == NULL)
         {
            pCurFrame = vogl_new(vogleditor_frameItem, total_swaps);
            vogleditor_apiCallTreeItem* pNewFrameNode = vogl_new(vogleditor_apiCallTreeItem, pCurFrame, pCurParent);
            pCurParent->appendChild(pNewFrameNode);
            m_itemList.append(pNewFrameNode);

            if (pPendingSnapshot != NULL)
            {
               pCurFrame->set_snapshot(pPendingSnapshot);
               pPendingSnapshot = NULL;
            }

            // update current parent
            pCurParent = pNewFrameNode;
         }

         // make item and node for the api call
         vogleditor_apiCallItem* pCallItem = vogl_new(vogleditor_apiCallItem, pCurFrame, pTrace_packet, *pGL_packet);
         pCurFrame->appendCall(pCallItem);

         if (pPendingSnapshot != NULL)
         {
            pCallItem->set_snapshot(pPendingSnapshot);
            pPendingSnapshot = NULL;
         }

         vogleditor_apiCallTreeItem* item = vogl_new(vogleditor_apiCallTreeItem, funcCall, pCallItem, pCurParent);
         pCurParent->appendChild(item);
         m_itemList.append(item);

         if (vogl_is_swap_buffers_entrypoint(entrypoint_id))
         {
            total_swaps++;

            // reset the CurParent back to the original parent so that the next frame will be at the root level
            pCurParent = parent;

            // reset the CurFrame so that a new frame node will be created on the next api call
            pCurFrame = NULL;
         }
      }

      if (pTrace_reader->get_packet_type() == cTSPTEOF)
      {
         //found_eof_packet = true;
         vogl_printf("Found trace file EOF packet on swap %" PRIu64 "\n", total_swaps);
         break;
      }
   }
}

QModelIndex vogleditor_QApiCallTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    vogleditor_apiCallTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_apiCallTreeItem*>(parent.internalPointer());

    vogleditor_apiCallTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex vogleditor_QApiCallTreeModel::indexOf(const vogleditor_apiCallTreeItem* pItem) const
{
    if (pItem != NULL)
        return createIndex(pItem->row(), VOGL_ACTC_APICALL, (void*)pItem);
    else
        return QModelIndex();
}

QModelIndex vogleditor_QApiCallTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    vogleditor_apiCallTreeItem* childItem = static_cast<vogleditor_apiCallTreeItem*>(index.internalPointer());
    vogleditor_apiCallTreeItem* parentItem = childItem->parent();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), VOGL_ACTC_APICALL, parentItem);
}

int vogleditor_QApiCallTreeModel::rowCount(const QModelIndex &parent) const
{
    vogleditor_apiCallTreeItem* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_apiCallTreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int vogleditor_QApiCallTreeModel::columnCount(const QModelIndex &parent) const
{
   VOGL_NOTE_UNUSED(parent);
   return VOGL_MAX_ACTC;
}

QVariant vogleditor_QApiCallTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    vogleditor_apiCallTreeItem* pItem = static_cast<vogleditor_apiCallTreeItem*>(index.internalPointer());

    // highlight the API call cell if it has a substring which matches the searchString
    if (role == Qt::BackgroundRole && index.column() == VOGL_ACTC_APICALL)
    {
        if (!m_searchString.isEmpty())
        {
            QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
            QString string = data.toString();
            if (string.contains(m_searchString, Qt::CaseInsensitive))
            {
                return QColor(Qt::yellow);
            }
        }
    }

    return pItem->columnData(index.column(), role);
}

Qt::ItemFlags vogleditor_QApiCallTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant vogleditor_QApiCallTreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_rootItem->columnData(section, role);

    return QVariant();
}

void vogleditor_QApiCallTreeModel::set_highlight_search_string(const QString searchString)
{
    m_searchString = searchString;
}

QModelIndex vogleditor_QApiCallTreeModel::find_prev_search_result(vogleditor_apiCallTreeItem* start, const QString searchText)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list, so return a default (invalid) item
            return QModelIndex();
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekPrevious();
        QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
        QString string = data.toString();
        if (string.contains(searchText, Qt::CaseInsensitive))
        {
            pFound = pItem;
            break;
        }

        iter.previous();
    }

    return indexOf(pFound);
}

QModelIndex vogleditor_QApiCallTreeModel::find_next_search_result(vogleditor_apiCallTreeItem* start, const QString searchText)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list, so return a default (invalid) item
            return QModelIndex();
        }
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        QVariant data = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole);
        QString string = data.toString();
        if (string.contains(searchText, Qt::CaseInsensitive))
        {
            pFound = pItem;
            break;
        }

        iter.next();
    }

    return indexOf(pFound);
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_prev_snapshot(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        if (iter.peekPrevious()->has_snapshot())
        {
            pFound = iter.peekPrevious();
            break;
        }

        iter.previous();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_next_snapshot(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    // if start is NULL, then search will begin from top, otherwise it will begin from the start item and search onwards
    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        if (iter.peekNext()->has_snapshot())
        {
            pFound = iter.peekNext();
            break;
        }

        iter.next();
    }

    return pFound;
}


vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::find_prev_drawcall(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (start != NULL)
    {
        if (iter.findNext(start) == false)
        {
            // the object wasn't found in the list
            return NULL;
        }

        // need to back up past the current item
        iter.previous();
    }
    else
    {
        // set the iterator to the back so that searching starts from the end of the list
        iter.toBack();
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the prev item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasPrevious())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekPrevious();
        if (pItem->apiCallItem() != NULL)
        {
            gl_entrypoint_id_t entrypointId = static_cast<gl_entrypoint_id_t>(pItem->apiCallItem()->getGLPacket()->m_entrypoint_id);
            if (vogl_is_draw_entrypoint(entrypointId) ||
                vogl_is_clear_entrypoint(entrypointId))
            {
                pFound = iter.peekPrevious();
                break;
            }
        }

        iter.previous();
    }

    return pFound;
}

vogleditor_apiCallTreeItem *vogleditor_QApiCallTreeModel::find_next_drawcall(vogleditor_apiCallTreeItem* start)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    if (iter.findNext(start) == false)
    {
        // the object wasn't found in the list
        return NULL;
    }

    // now the iterator is pointing to the desired start object in the list,
    // continually check the next item and find one with a snapshot
    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->apiCallItem() != NULL)
        {
            gl_entrypoint_id_t entrypointId = static_cast<gl_entrypoint_id_t>(pItem->apiCallItem()->getGLPacket()->m_entrypoint_id);
            if (vogl_is_draw_entrypoint(entrypointId) ||
                vogl_is_clear_entrypoint(entrypointId))
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_call_number(uint64_t callNumber)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->apiCallItem() != NULL)
        {
            if (pItem->apiCallItem()->globalCallIndex() == callNumber)
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}

vogleditor_apiCallTreeItem* vogleditor_QApiCallTreeModel::find_frame_number(uint64_t frameNumber)
{
    QLinkedListIterator<vogleditor_apiCallTreeItem*> iter(m_itemList);

    vogleditor_apiCallTreeItem* pFound = NULL;
    while (iter.hasNext())
    {
        vogleditor_apiCallTreeItem* pItem = iter.peekNext();
        if (pItem->frameItem() != NULL)
        {
            if (pItem->frameItem()->frameNumber() == frameNumber)
            {
                pFound = iter.peekNext();
                break;
            }
        }

        iter.next();
    }

    return pFound;
}
