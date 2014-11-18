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

#ifndef VOGLEDITOR_QAPICALLTREEMODEL_H
#define VOGLEDITOR_QAPICALLTREEMODEL_H

#include <QAbstractItemModel>
#include <QLinkedList>
#include "vogl_common.h"

class QVariant;
class vogleditor_apiCallTreeItem;
class vogl_ctypes;
class vogl_trace_file_reader;
class vogleditor_groupItem;
class vogleditor_frameItem;
class vogl_trace_packet;
class vogleditor_apiCallItem;

struct vogl_trace_gl_entrypoint_packet;

class vogleditor_QApiCallTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    vogleditor_QApiCallTreeModel(QObject *parent = 0);
    ~vogleditor_QApiCallTreeModel();

    bool init(vogl_trace_file_reader *pTrace_reader);

    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QModelIndex indexOf(const vogleditor_apiCallTreeItem *pItem) const;

    vogleditor_apiCallTreeItem *root() const
    {
        return m_rootItem;
    }

    vogleditor_apiCallTreeItem *create_group(vogleditor_frameItem *pFrameObj,
                                             vogleditor_groupItem *&pGroupObj,
                                             vogleditor_apiCallTreeItem *pParentNode);
    void set_highlight_search_string(const QString searchString);
    QModelIndex find_prev_search_result(vogleditor_apiCallTreeItem *start, const QString searchText);
    QModelIndex find_next_search_result(vogleditor_apiCallTreeItem *start, const QString searchText);

    vogleditor_apiCallTreeItem *find_prev_snapshot(vogleditor_apiCallTreeItem *start);
    vogleditor_apiCallTreeItem *find_next_snapshot(vogleditor_apiCallTreeItem *start);

    vogleditor_apiCallTreeItem *find_prev_drawcall(vogleditor_apiCallTreeItem *start);
    vogleditor_apiCallTreeItem *find_next_drawcall(vogleditor_apiCallTreeItem *start);

    vogleditor_apiCallTreeItem *find_call_number(unsigned int callNumber);
    vogleditor_apiCallTreeItem *find_frame_number(unsigned int frameNumber);

signals:

public
slots:

private:
    gl_entrypoint_id_t itemApiCallId(vogleditor_apiCallTreeItem *apiCall) const;
    gl_entrypoint_id_t lastItemApiCallId() const;

    bool isMarkerPushEntrypoint(gl_entrypoint_id_t id) const;
    bool isMarkerPopEntrypoint(gl_entrypoint_id_t id) const;
    bool isStartNestedEntrypoint(gl_entrypoint_id_t id) const;
    bool isEndNestedEntrypoint(gl_entrypoint_id_t id) const;
    bool isFrameBufferWriteEntrypoint(gl_entrypoint_id_t id) const;

    bool displayMarkerTextAsLabel() const;
    bool hideMarkerPopApiCall() const;

private:
    vogleditor_apiCallTreeItem *m_rootItem;
    vogl_ctypes *m_pTrace_ctypes;
    QLinkedList<vogleditor_apiCallTreeItem *> m_itemList;
    QString m_searchString;
};

#endif // VOGLEDITOR_QAPICALLTREEMODEL_H
