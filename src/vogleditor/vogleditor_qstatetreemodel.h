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

#ifndef VOGLEDITOR_QSTATETREEMODEL_H
#define VOGLEDITOR_QSTATETREEMODEL_H

#include <QAbstractItemModel>
#include <QList>

#include "vogleditor_gl_state_snapshot.h"

class QVariant;
class vogleditor_stateTreeItem;
class vogleditor_stateTreeProgramItem;
class vogleditor_stateTreeContextItem;

class vogleditor_QStateTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    vogleditor_QStateTreeModel(QObject *parent);
    vogleditor_QStateTreeModel(vogleditor_gl_state_snapshot *pSnapshot, vogl_context_snapshot *pContext, vogleditor_gl_state_snapshot *pDiffBaseSnapshot, QObject *parent);

    virtual ~vogleditor_QStateTreeModel();

    // QAbstractItemModel interface
    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;

    virtual QModelIndex parent(const QModelIndex &child) const;

    virtual int rowCount(const QModelIndex &parent) const;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex &index, int role) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    // end QAbstractItemModel interface

    vogleditor_stateTreeItem *root() const;

    vogleditor_gl_state_snapshot *get_snapshot() const;

private:
    vogleditor_stateTreeItem *m_rootItem;
    QList<QVariant> m_ColumnTitles;
    vogleditor_gl_state_snapshot *m_pSnapshot;
    const vogleditor_gl_state_snapshot *m_pBaseSnapshot;

    void setupModelData(vogleditor_gl_state_snapshot *pSnapshot, vogl_context_snapshot *pContext, vogleditor_stateTreeItem *parent);
};

#endif // VOGLEDITOR_QSTATETREEMODEL_H
