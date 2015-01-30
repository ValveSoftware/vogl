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

#ifndef VOGLEDITOR_APICALLTREEITEM_H
#define VOGLEDITOR_APICALLTREEITEM_H

#include <QList>
#include <QVariant>
#include "vogl_core.h"
#include "vogl_common.h"

class vogleditor_frameItem;
class vogleditor_groupItem;
class vogleditor_apiCallItem;
class vogleditor_gl_state_snapshot;

class vogl_trace_file_reader;
struct vogl_trace_gl_entrypoint_packet;

class vogleditor_QApiCallTreeModel;

enum VOGL_API_CALL_TREE_COLUMN
{
    VOGL_ACTC_APICALL,
    VOGL_ACTC_INDEX,
    VOGL_ACTC_GLCONTEXT,
    VOGL_ACTC_FLAGS,
    //VOGL_ACTC_BEGINTIME,
    //VOGL_ACTC_ENDTIME,
    VOGL_ACTC_DURATION,
    VOGL_MAX_ACTC
};

// TODO: Maybe think about a less general group name for "Render" so as not to
//       possibly be confused with a marker_push entrypoint name that could also
//       be named "Render"
const QString cTREEITEM_STATECHANGES("State changes");
const QString cTREEITEM_RENDER("Draw calls");

class vogleditor_apiCallTreeItem
{
public:
    // Constructor for the root node
    vogleditor_apiCallTreeItem(vogleditor_QApiCallTreeModel *pModel);

    // Constructor for frame nodes
    vogleditor_apiCallTreeItem(vogleditor_frameItem *frameItem, vogleditor_apiCallTreeItem *parent);

    // Constructor for group nodes
    vogleditor_apiCallTreeItem(vogleditor_groupItem *groupItem, vogleditor_apiCallTreeItem *parent);

    // Constructor for apiCall nodes
    vogleditor_apiCallTreeItem(vogleditor_apiCallItem *apiCallItem, vogleditor_apiCallTreeItem *parent);

    ~vogleditor_apiCallTreeItem();

    vogleditor_apiCallTreeItem *parent() const;

    void appendChild(vogleditor_apiCallTreeItem *pChild);
    void popChild();

    int childCount() const;

    vogleditor_apiCallTreeItem *child(int index) const;

    vogleditor_apiCallItem *apiCallItem() const;
    vogleditor_groupItem *groupItem() const;
    vogleditor_frameItem *frameItem() const;

    uint64_t startTime() const;
    uint64_t endTime() const;
    uint64_t duration() const;

    void set_snapshot(vogleditor_gl_state_snapshot *pSnapshot);

    bool has_snapshot() const;

    vogleditor_gl_state_snapshot *get_snapshot() const;

    int columnCount() const;

    QVariant columnData(int column, int role) const;

    void setDurationColumn(uint64_t span = 0);
    void setApiCallColumn(QString name);
    QString apiCallColumn() const;
    QString apiCallStringArg() const;

    int row() const;

    bool isApiCall() const;
    bool isGroup() const;
    bool isGroupAncestry() const;
    bool isFrame() const;
    bool isRoot() const;

    bool isRenderGroup();
    bool isStateChangeGroup();

    void setRenderGroup();
    void setStateChangeGroup();

    gl_entrypoint_id_t entrypoint_id() const;

private:
    void setColumnData(QVariant data, int column);

private:
    QList<vogleditor_apiCallTreeItem *> m_childItems;
    QVariant m_columnData[VOGL_MAX_ACTC];
    vogleditor_apiCallTreeItem *m_parentItem;
    vogleditor_apiCallItem *m_pApiCallItem;
    vogleditor_groupItem *m_pGroupItem;
    vogleditor_frameItem *m_pFrameItem;
    vogleditor_QApiCallTreeModel *m_pModel;
    int m_localRowIndex;
};

#endif // VOGLEDITOR_APICALLTREEITEM_H
