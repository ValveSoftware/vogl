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

#ifndef VOGLEDITOR_STATETREEITEM_H
#define VOGLEDITOR_STATETREEITEM_H

#include <QList>
#include <QVariant>

#include "vogl_context_info.h"

class vogl_state_vector;
class vogleditor_QStateTreeModel;

class vogleditor_stateTreeItem
{
public:
    // Constructor for the root node
    vogleditor_stateTreeItem(QList<QVariant> columnTitles, vogleditor_QStateTreeModel *pModel);

    // Constructor for other nodes
    vogleditor_stateTreeItem(QString name, QString value, vogleditor_stateTreeItem *parent);

    virtual ~vogleditor_stateTreeItem();

    vogleditor_stateTreeItem *parent() const;

    void setValue(QString value);

    void setValue(int value)
    {
        QString tmp;
        setValue(tmp.sprintf("%d", value));
    }
    void setValue(uint value)
    {
        QString tmp;
        setValue(tmp.sprintf("%u", value));
    }
    void setValue(bool value)
    {
        QString tmp;
        setValue(tmp.sprintf("%s", value ? "GL_TRUE" : "GL_FALSE"));
    }
    void setValue(void *value)
    {
        QString tmp;
        setValue(tmp.sprintf("%p", value));
    }
    void setValue(const char *value)
    {
        setValue(QString(value));
    }

    QString getValueFromBools(const bool *values, uint count) const;
    QString getValueFromInts(const int *values, uint count) const;
    QString getValueFromUints(const uint *values, uint count) const;
    QString getValueFromFloats(const float *values, uint count) const;
    QString getValueFromDoubles(const double *values, uint count) const;
    QString getValueFromEnums(const int *values, uint count) const;
    QString getValueFromPtrs(const int *values, uint count) const;

    void appendChild(vogleditor_stateTreeItem *pChild);

    void transferChildren(vogleditor_stateTreeItem *pNewParent);

    int childCount() const;

    vogleditor_stateTreeItem *child(int index) const;

    int columnCount() const;

    QVariant columnData(int column, int role) const;

    int row() const;

    virtual bool hasChanged() const;

    virtual QString getDiffedValue() const
    {
        return "";
    }

    virtual void wasEdited(bool bEdited)
    {
        m_bWasEdited = bEdited;
    }
    virtual bool wasEdited() const
    {
        return m_bWasEdited;
    }

    typedef enum
    {
        cDEFAULT,
        cTEXTURE,
        cSHADER,
        cPROGRAM,
        cPROGRAMARB,
        cFRAMEBUFFER,
        cBUFFER
    } state_tree_type;

    virtual vogleditor_stateTreeItem::state_tree_type getStateType() const
    {
        return cDEFAULT;
    }

protected:
    QList<vogleditor_stateTreeItem *> m_childItems;
    QList<QVariant> m_columnData;
    vogleditor_stateTreeItem *m_parentItem;
    vogleditor_QStateTreeModel *m_pModel;
    bool m_bWasEdited;

    static QString enum_to_string(GLenum id)
    {
        static QString tmp;
        tmp = get_gl_enums().find_name(id);
        if (tmp.isNull() || tmp.isEmpty())
        {
            tmp = tmp.sprintf("0x%04x", id);
        }
        return tmp;
    }

    static QString int_to_string(int value)
    {
        static QString tmp;
        return tmp.sprintf("%d", value);
    }

    static QString int64_to_string(int64_t value)
    {
        static QString tmp;
        return tmp.sprintf("%" PRIu64, value);
    }
};

class vogleditor_stateTreeStateVecDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeStateVecDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pDiffBaseState(NULL)
    {
    }

    virtual void set_diff_base_state(const vogl_state_vector *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

protected:
    const vogl_state_vector *m_pDiffBaseState;
};

template <typename T>
class vogleditor_stateTreeDatatypeItem : public vogleditor_stateTreeStateVecDiffableItem
{
public:
    vogleditor_stateTreeDatatypeItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeDatatypeItem()
    {
        m_pStateVec = NULL;
    }

    virtual bool hasChanged() const;

    virtual QString getDiffedValue() const;

protected:
    GLenum m_name;
    unsigned int m_index;
    unsigned int m_numComponents;
    bool m_isIndexed;
    const vogl_state_vector *m_pStateVec;
};

class vogleditor_stateTreeStateVecBoolItem : public vogleditor_stateTreeDatatypeItem<bool>
{
public:
    vogleditor_stateTreeStateVecBoolItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, bool *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    vogleditor_stateTreeStateVecBoolItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeStateVecBoolItem()
    {
    }

    virtual QString getDiffedValue() const;
};

class vogleditor_stateTreeStateVecIntItem : public vogleditor_stateTreeDatatypeItem<int>
{
public:
    vogleditor_stateTreeStateVecIntItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeStateVecIntItem()
    {
    }

    virtual QString getDiffedValue() const;
};

class vogleditor_stateTreeStateVecFloatItem : public vogleditor_stateTreeDatatypeItem<float>
{
public:
    vogleditor_stateTreeStateVecFloatItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, float *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeStateVecFloatItem()
    {
    }

    virtual QString getDiffedValue() const;
};

class vogleditor_stateTreeStateVecEnumItem : public vogleditor_stateTreeDatatypeItem<int>
{
public:
    vogleditor_stateTreeStateVecEnumItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeStateVecEnumItem()
    {
    }

    virtual QString getDiffedValue() const;
};

class vogleditor_stateTreeStateVecPtrItem : public vogleditor_stateTreeDatatypeItem<int>
{
public:
    vogleditor_stateTreeStateVecPtrItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeStateVecPtrItem()
    {
    }

    virtual QString getDiffedValue() const;
};

class vogleditor_stateTreeStateVecMatrixRowItem : public vogleditor_stateTreeStateVecDiffableItem
{
public:
    vogleditor_stateTreeStateVecMatrixRowItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, float *values, unsigned int numComponents, unsigned int rowIndex, bool isIndexed, vogleditor_stateTreeItem *parent);

    virtual ~vogleditor_stateTreeStateVecMatrixRowItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;

    virtual QString getDiffedValue() const;

private:
    GLenum m_name;
    unsigned int m_index;
    unsigned int m_numComponents;
    bool m_isIndexed;
    const vogl_state_vector *m_pState;
    unsigned int m_rowIndex;
};

class vogleditor_stateTreeStateVecMatrixItem : public vogleditor_stateTreeStateVecDiffableItem
{
public:
    vogleditor_stateTreeStateVecMatrixItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, float *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);

    virtual ~vogleditor_stateTreeStateVecMatrixItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    virtual void set_diff_base_state(const vogl_state_vector *pBaseState)
    {
        m_pDiffBaseState = pBaseState;

        for (vogleditor_stateTreeStateVecMatrixRowItem **iter = m_rowItems.begin(); iter != m_rowItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

private:
    const vogl_state_vector *m_pState;
    vogl::vector<vogleditor_stateTreeStateVecMatrixRowItem *> m_rowItems;
};

#endif // VOGLEDITOR_STATETREEITEM_H
