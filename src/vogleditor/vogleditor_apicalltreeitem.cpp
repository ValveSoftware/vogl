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

#include <QColor>
#include <QIcon>

#include "vogleditor_apicalltreeitem.h"
#include "vogleditor_groupitem.h"
#include "vogleditor_qapicalltreemodel.h"

#include "vogleditor_frameitem.h"
#include "vogl_common.h"
#include "vogl_trace_file_reader.h"
#include "vogl_trace_packet.h"
#include "vogl_trace_stream_types.h"
#include "vogleditor_gl_state_snapshot.h"
#include "vogleditor_qsettings.h"

// Constructor for root node
vogleditor_apiCallTreeItem::vogleditor_apiCallTreeItem(vogleditor_QApiCallTreeModel *pModel)
    : m_parentItem(NULL),
      m_pApiCallItem(NULL),
      m_pGroupItem(NULL),
      m_pFrameItem(NULL),
      m_pModel(pModel),
      m_localRowIndex(0)
{
    m_columnData[VOGL_ACTC_APICALL] = "API Call";
    m_columnData[VOGL_ACTC_INDEX] = "Index";
    m_columnData[VOGL_ACTC_FLAGS] = "";
    m_columnData[VOGL_ACTC_GLCONTEXT] = "GL Context";
    //m_columnData[VOGL_ACTC_BEGINTIME] = "Begin Time";
    //m_columnData[VOGL_ACTC_ENDTIME] = "End Time";
    m_columnData[VOGL_ACTC_DURATION] = "Duration (ns)";
}

// Constructor for frame nodes
vogleditor_apiCallTreeItem::vogleditor_apiCallTreeItem(vogleditor_frameItem *frameItem, vogleditor_apiCallTreeItem *parent)
    : m_parentItem(parent),
      m_pApiCallItem(NULL),
      m_pGroupItem(NULL),
      m_pFrameItem(frameItem),
      m_pModel(NULL),
      m_localRowIndex(0)
{
    if (frameItem != NULL)
    {
        QString tmp;
        tmp.sprintf("Frame %" PRIu64, frameItem->frameNumber());
        m_columnData[VOGL_ACTC_APICALL] = tmp;
    }

    if (m_parentItem != NULL)
    {
        m_pModel = m_parentItem->m_pModel;
    }

    parent->appendChild(this);
}

// Constructor for group nodes
vogleditor_apiCallTreeItem::vogleditor_apiCallTreeItem(vogleditor_groupItem *groupItem, vogleditor_apiCallTreeItem *parent)
    : m_parentItem(parent),
      m_pApiCallItem(NULL),
      m_pGroupItem(groupItem),
      m_pFrameItem(NULL),
      m_pModel(NULL),
      m_localRowIndex(0)
{
    m_columnData[VOGL_ACTC_APICALL] = cTREEITEM_STATECHANGES;
    if (m_parentItem != NULL)
    {
        m_pModel = m_parentItem->m_pModel;
    }

    parent->appendChild(this);
}

// Constructor for apiCall nodes
vogleditor_apiCallTreeItem::vogleditor_apiCallTreeItem(vogleditor_apiCallItem *apiCallItem, vogleditor_apiCallTreeItem *parent)
    : m_parentItem(parent),
      m_pApiCallItem(apiCallItem),
      m_pGroupItem(NULL),
      m_pFrameItem(NULL),
      m_pModel(NULL),
      m_localRowIndex(0)
{
    m_columnData[VOGL_ACTC_APICALL] = apiCallItem->apiFunctionCall();

    if (apiCallItem != NULL)
    {
        m_columnData[VOGL_ACTC_INDEX] = (qulonglong)apiCallItem->globalCallIndex();
        m_columnData[VOGL_ACTC_FLAGS] = "";
        dynamic_string strContext;
        m_columnData[VOGL_ACTC_GLCONTEXT] = strContext.format("0x%" PRIx64, apiCallItem->getGLPacket()->m_context_handle).c_str();
        //m_columnData[VOGL_ACTC_BEGINTIME] = (qulonglong)apiCallItem->startTime();
        //m_columnData[VOGL_ACTC_ENDTIME] = (qulonglong)apiCallItem->endTime();
        m_columnData[VOGL_ACTC_DURATION] = (qulonglong)apiCallItem->duration();
    }

    if (m_parentItem != NULL)
    {
        m_pModel = m_parentItem->m_pModel;
    }

    parent->appendChild(this);
}

vogleditor_apiCallTreeItem::~vogleditor_apiCallTreeItem()
{
    if (m_pFrameItem != NULL)
    {
        vogl_delete(m_pFrameItem);
        m_pFrameItem = NULL;
    }

    if (m_pGroupItem != NULL)
    {
        vogl_delete(m_pGroupItem);
        m_pGroupItem = NULL;
    }

    if (m_pApiCallItem != NULL)
    {
        vogl_delete(m_pApiCallItem);
        m_pApiCallItem = NULL;
    }

    for (int i = 0; i < m_childItems.size(); i++)
    {
        vogl_delete(m_childItems[i]);
        m_childItems[i] = NULL;
    }

    m_childItems.clear();
}

vogleditor_apiCallTreeItem *vogleditor_apiCallTreeItem::parent() const
{
    return m_parentItem;
}
bool vogleditor_apiCallTreeItem::isApiCall() const
{
    return m_pApiCallItem != NULL;
}
bool vogleditor_apiCallTreeItem::isGroup() const
{
    return (g_settings.group_state_render_stat() && (m_pGroupItem != NULL));
}
bool vogleditor_apiCallTreeItem::isGroupAncestry() const
{
    bool bRetVal = false;

    if (g_settings.group_state_render_stat())
    {
        vogleditor_apiCallTreeItem const *pCallTreeItem = this;
        while (pCallTreeItem)
        {
            if (pCallTreeItem->isGroup())
            {
                bRetVal = true;
                break;
            }
            pCallTreeItem = pCallTreeItem->parent();
        }
    }
    return bRetVal;
}
bool vogleditor_apiCallTreeItem::isFrame() const
{
    return m_pFrameItem != NULL;
}
bool vogleditor_apiCallTreeItem::isRoot() const
{
    return !(isApiCall() | isGroup() | isFrame());
}

void vogleditor_apiCallTreeItem::appendChild(vogleditor_apiCallTreeItem *pChild)
{
    pChild->m_localRowIndex = m_childItems.size();
    m_childItems.append(pChild);
}

void vogleditor_apiCallTreeItem::popChild()
{
    m_childItems.removeLast();
}

int vogleditor_apiCallTreeItem::childCount() const
{
    return m_childItems.size();
}

vogleditor_apiCallTreeItem *vogleditor_apiCallTreeItem::child(int index) const
{
    if (index < 0 || index >= childCount())
    {
        return NULL;
    }

    return m_childItems[index];
}

vogleditor_apiCallItem *vogleditor_apiCallTreeItem::apiCallItem() const
{
    return m_pApiCallItem;
}

vogleditor_groupItem *vogleditor_apiCallTreeItem::groupItem() const
{
    return m_pGroupItem;
}

vogleditor_frameItem *vogleditor_apiCallTreeItem::frameItem() const
{
    return m_pFrameItem;
}

uint64_t vogleditor_apiCallTreeItem::startTime() const
{
    uint64_t startTime = 0;

    if (m_pApiCallItem)
    {
        startTime = m_pApiCallItem->startTime();
    }
    else if (m_pGroupItem)
    {
        startTime = m_pGroupItem->startTime();
    }
    else if (m_pFrameItem)
    {
        startTime = m_pFrameItem->startTime();
    }
    else // root
    {
        startTime = child(0)->startTime();
    }
    return startTime;
}

uint64_t vogleditor_apiCallTreeItem::endTime() const
{
    uint64_t endTime = 0;

    if (m_pApiCallItem)
    {
        endTime = m_pApiCallItem->endTime();
    }
    else if (m_pGroupItem)
    {
        endTime = m_pGroupItem->endTime();
    }
    else if (m_pFrameItem)
    {
        endTime = m_pFrameItem->endTime();
    }
    else // root
    {
        endTime = child(childCount() - 1)->endTime();
    }
    return endTime;
}

uint64_t vogleditor_apiCallTreeItem::duration() const
{
    return endTime() - startTime();
}

void vogleditor_apiCallTreeItem::set_snapshot(vogleditor_gl_state_snapshot *pSnapshot)
{
    if (m_pFrameItem)
    {
        m_pFrameItem->set_snapshot(pSnapshot);
    }

    if (m_pApiCallItem)
    {
        m_pApiCallItem->set_snapshot(pSnapshot);
    }
}

bool vogleditor_apiCallTreeItem::has_snapshot() const
{
    bool bHasSnapshot = false;
    if (m_pFrameItem)
    {
        bHasSnapshot = m_pFrameItem->has_snapshot();
    }

    if (m_pApiCallItem)
    {
        bHasSnapshot = m_pApiCallItem->has_snapshot();
    }
    return bHasSnapshot;
}

vogleditor_gl_state_snapshot *vogleditor_apiCallTreeItem::get_snapshot() const
{
    vogleditor_gl_state_snapshot *pSnapshot = NULL;
    if (m_pFrameItem)
    {
        pSnapshot = m_pFrameItem->get_snapshot();
    }

    if (m_pApiCallItem)
    {
        pSnapshot = m_pApiCallItem->get_snapshot();
    }
    return pSnapshot;
}

int vogleditor_apiCallTreeItem::columnCount() const
{
    int count = 0;
    if (m_parentItem == NULL)
    {
        count = VOGL_MAX_ACTC;
    }
    else
    {
        m_pModel->columnCount();
    }

    return count;
}

QVariant vogleditor_apiCallTreeItem::columnData(int column, int role) const
{
    if (column >= VOGL_MAX_ACTC)
    {
        return QVariant();
    }

    if (role == Qt::DecorationRole)
    {
        // handle flags
        if (column == VOGL_ACTC_FLAGS)
        {
            if (has_snapshot())
            {
                if (get_snapshot()->is_outdated())
                {
                    // snapshot was dirtied due to an earlier edit
                    return QColor(200, 0, 0);
                }
                else if (get_snapshot()->is_edited())
                {
                    // snapshot has been edited
                    return QColor(200, 102, 0);
                }
                else
                {
                    // snapshot is good
                    return QColor(0, 0, 255);
                }
            }
            else if (frameItem() != NULL && frameItem()->get_screenshot_filename().size() > 0)
            {
                return QIcon(frameItem()->get_screenshot_filename().c_str());
            }
        }
    }

    if (role == Qt::DisplayRole)
    {
        return m_columnData[column];
    }

    return QVariant();
}

void vogleditor_apiCallTreeItem::setDurationColumn(uint64_t span)
{
    if (0 == span)
    {
        // If given span is 0, use span of current tree item.
        if (isApiCall())
        {
            span = apiCallItem()->duration();
        }
        else if (isGroup())
        {
            span = groupItem()->duration();
        }
        else if (isFrame())
        {
            span = frameItem()->duration();
        }
    }
    setColumnData(QVariant(qulonglong(span)), VOGL_ACTC_DURATION);
}

void vogleditor_apiCallTreeItem::setApiCallColumn(QString name)
{
    setColumnData(QVariant(name), VOGL_ACTC_APICALL);
}

void vogleditor_apiCallTreeItem::setColumnData(QVariant data, int column)
{
    m_columnData[column] = data;
}

bool vogleditor_apiCallTreeItem::isRenderGroup()
{
    return (apiCallColumn() == cTREEITEM_RENDER);
}

bool vogleditor_apiCallTreeItem::isStateChangeGroup()
{
    return (apiCallColumn() == cTREEITEM_STATECHANGES);
}

void vogleditor_apiCallTreeItem::setRenderGroup()
{
    setApiCallColumn(cTREEITEM_RENDER);
}

void vogleditor_apiCallTreeItem::setStateChangeGroup()
{
    setApiCallColumn(cTREEITEM_STATECHANGES);
}

QString vogleditor_apiCallTreeItem::apiCallColumn() const
{
    return (columnData(VOGL_ACTC_APICALL, Qt::DisplayRole)).toString();
}

QString vogleditor_apiCallTreeItem::apiCallStringArg() const
{
    return isApiCall() ? apiCallItem()->stringArg() : QString();
}

int vogleditor_apiCallTreeItem::row() const
{
    // note, this is just the row within the current level of the hierarchy
    return m_localRowIndex;
}

gl_entrypoint_id_t vogleditor_apiCallTreeItem::entrypoint_id() const
{
   return m_pApiCallItem ? m_pApiCallItem->getTracePacket()->get_entrypoint_id() : VOGL_ENTRYPOINT_INVALID;
}
