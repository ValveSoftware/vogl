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

#include "vogleditor_qstatetreemodel.h"
#include "vogleditor_statetreeitem.h"
#include "vogleditor_statetreecontextitem.h"

#include "vogl_sync_object.h"

//===============================================

vogleditor_QStateTreeModel::vogleditor_QStateTreeModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_pSnapshot(NULL),
      m_pBaseSnapshot(NULL)
{
    m_ColumnTitles << "State";
    m_ColumnTitles << "Value";

    m_rootItem = new vogleditor_stateTreeItem(m_ColumnTitles, this);
}

vogleditor_QStateTreeModel::vogleditor_QStateTreeModel(vogleditor_gl_state_snapshot *pSnapshot, vogl_context_snapshot *pContext, vogleditor_gl_state_snapshot *pDiffBaseSnapshot, QObject *parent)
    : QAbstractItemModel(parent),
      m_pSnapshot(pSnapshot),
      m_pBaseSnapshot(pDiffBaseSnapshot)
{
    m_ColumnTitles << "State";
    m_ColumnTitles << "Value";

    m_rootItem = new vogleditor_stateTreeItem(m_ColumnTitles, this);
    setupModelData(m_pSnapshot, pContext, m_rootItem);
}

vogleditor_QStateTreeModel::~vogleditor_QStateTreeModel()
{
    if (m_rootItem != NULL)
    {
        delete m_rootItem;
        m_rootItem = NULL;
    }
}

QModelIndex vogleditor_QStateTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    vogleditor_stateTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_stateTreeItem *>(parent.internalPointer());

    vogleditor_stateTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex vogleditor_QStateTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    vogleditor_stateTreeItem *childItem = static_cast<vogleditor_stateTreeItem *>(child.internalPointer());
    vogleditor_stateTreeItem *parentItem = childItem->parent();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int vogleditor_QStateTreeModel::rowCount(const QModelIndex &parent) const
{
    vogleditor_stateTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<vogleditor_stateTreeItem *>(parent.internalPointer());

    return parentItem->childCount();
}

int vogleditor_QStateTreeModel::columnCount(const QModelIndex &parent) const
{
    VOGL_NOTE_UNUSED(parent);
    return m_ColumnTitles.size();
}

QVariant vogleditor_QStateTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    vogleditor_stateTreeItem *item = static_cast<vogleditor_stateTreeItem *>(index.internalPointer());

    return item->columnData(index.column(), role);
}

QVariant vogleditor_QStateTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
        return m_rootItem->columnData(section, role);

    return QVariant();
}

vogleditor_stateTreeItem *vogleditor_QStateTreeModel::root() const
{
    return m_rootItem;
}

vogleditor_gl_state_snapshot *vogleditor_QStateTreeModel::get_snapshot() const
{
    return m_pSnapshot;
}

void vogleditor_QStateTreeModel::setupModelData(vogleditor_gl_state_snapshot *pSnapshot, vogl_context_snapshot *pContext, vogleditor_stateTreeItem *parent)
{
    VOGL_NOTE_UNUSED(pSnapshot);

    QString tmp;

    const vogl_context_desc &desc = pContext->get_context_desc();
    vogleditor_stateTreeContextItem *pContextItem = new vogleditor_stateTreeContextItem(tmp.sprintf("Context %p", (void *)desc.get_trace_context()), "", parent, *pContext);
    if (m_pBaseSnapshot != NULL && m_pBaseSnapshot->is_valid() && m_pBaseSnapshot->get_contexts().size() > 0 && m_pBaseSnapshot->get_context(desc.get_trace_context()) != NULL)
    {
        // set the diff state to be the same state, so that there does not appear to be any diff's
        const vogl_context_snapshot *pDiffContext = m_pBaseSnapshot->get_context(desc.get_trace_context());
        pContextItem->set_diff_base_state(pDiffContext);
    }

    pContextItem->transferChildren(parent);
    delete pContextItem;
}
