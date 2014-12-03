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

#include "vogleditor_statetreeitem.h"
#include "vogleditor_qstatetreemodel.h"

//=============================================================================

vogleditor_stateTreeItem::vogleditor_stateTreeItem(QList<QVariant> columnTitles, vogleditor_QStateTreeModel *pModel)
    : m_columnData(columnTitles),
      m_parentItem(NULL),
      m_pModel(pModel),
      m_bWasEdited(false)
{
}

vogleditor_stateTreeItem::vogleditor_stateTreeItem(QString name, QString value, vogleditor_stateTreeItem *parent)
    : m_parentItem(parent),
      m_pModel(NULL)
{
    m_columnData << name;
    m_columnData << value;

    if (m_parentItem != NULL)
    {
        m_pModel = m_parentItem->m_pModel;
    }
}

vogleditor_stateTreeItem::~vogleditor_stateTreeItem()
{
    for (int i = 0; i < m_childItems.size(); i++)
    {
        delete m_childItems[i];
        m_childItems[i] = NULL;
    }

    m_childItems.clear();
}

vogleditor_stateTreeItem *vogleditor_stateTreeItem::parent() const
{
    return m_parentItem;
}

void vogleditor_stateTreeItem::setValue(QString value)
{
    if (m_columnData.size() >= 2)
    {
        m_columnData.removeAt(1);
        m_columnData.insert(1, value);
    }
}

QString vogleditor_stateTreeItem::getValueFromBools(const bool *values, uint count) const
{
    if (count == 0 || values == NULL)
    {
        return "";
    }
    else if (count == 1)
    {
        return (values[0]) ? "GL_TRUE" : "GL_FALSE";
    }

    QString tmp;
    tmp = tmp.sprintf("%s, %s", getValueFromBools(values, 1).toStdString().c_str(), getValueFromBools(&(values[1]), count - 1).toStdString().c_str());
    return tmp;
}

QString vogleditor_stateTreeItem::getValueFromInts(const int *values, uint count) const
{
    QString tmp;
    if (count == 0 || values == NULL)
    {
        return "";
    }
    else if (count == 1)
    {
        tmp = tmp.sprintf("%d", *values);
    }
    else
    {
        tmp = tmp.sprintf("%s, %s", getValueFromInts(values, 1).toStdString().c_str(), getValueFromInts(&(values[1]), count - 1).toStdString().c_str());
    }

    return tmp;
}

QString vogleditor_stateTreeItem::getValueFromUints(const uint *values, uint count) const
{
    QString tmp;
    if (count == 0 || values == NULL)
    {
        return "";
    }
    else if (count == 1)
    {
        tmp = tmp.sprintf("%d", *values);
    }
    else
    {
        tmp = tmp.sprintf("%s, %s", getValueFromUints(values, 1).toStdString().c_str(), getValueFromUints(&(values[1]), count - 1).toStdString().c_str());
    }

    return tmp;
}

QString vogleditor_stateTreeItem::getValueFromFloats(const float *values, uint count) const
{
    QString tmp;
    if (count == 0 || values == NULL)
    {
        return "";
    }
    else if (count == 1)
    {
        tmp = tmp.sprintf("%f", *values);
    }
    else
    {
        tmp = tmp.sprintf("%s, %s", getValueFromFloats(values, 1).toStdString().c_str(), getValueFromFloats(&(values[1]), count - 1).toStdString().c_str());
    }

    return tmp;
}

QString vogleditor_stateTreeItem::getValueFromDoubles(const double *values, uint count) const
{
    QString tmp;
    if (count == 0 || values == NULL)
    {
        return "";
    }
    else if (count == 1)
    {
        tmp = tmp.sprintf("%f", *values);
    }
    else
    {
        tmp = tmp.sprintf("%s, %s", getValueFromDoubles(values, 1).toStdString().c_str(), getValueFromDoubles(&(values[1]), count - 1).toStdString().c_str());
    }

    return tmp;
}

QString vogleditor_stateTreeItem::getValueFromEnums(const int *values, uint count) const
{
    QString tmp;
    if (count == 0 || values == NULL)
    {
        return "";
    }
    else if (count == 1)
    {
        tmp = enum_to_string(*values);
    }
    else
    {
        tmp = tmp.sprintf("%s, %s", getValueFromEnums(values, 1).toStdString().c_str(), getValueFromEnums(&(values[1]), count - 1).toStdString().c_str());
    }

    return tmp;
}

QString vogleditor_stateTreeItem::getValueFromPtrs(const int *values, uint count) const
{
    QString tmp;
    if (count == 0 || values == NULL)
    {
        return "";
    }
    else if (count == 1)
    {
        tmp = tmp.sprintf("0x%08x", *values);
    }
    else
    {
        tmp = tmp.sprintf("%s, %s", getValueFromPtrs(values, 1).toStdString().c_str(), getValueFromPtrs(&(values[1]), count - 1).toStdString().c_str());
    }

    return tmp;
}

void vogleditor_stateTreeItem::appendChild(vogleditor_stateTreeItem *pChild)
{
    m_childItems.append(pChild);
}

void vogleditor_stateTreeItem::transferChildren(vogleditor_stateTreeItem *pNewParent)
{
    // transfer the children to the new parent, then clear our list of children
    pNewParent->m_childItems = this->m_childItems;

    for (int i = 0; i < pNewParent->m_childItems.size(); i++)
    {
        pNewParent->m_childItems[i]->m_parentItem = pNewParent;
    }

    m_childItems.clear();
}

int vogleditor_stateTreeItem::childCount() const
{
    return m_childItems.size();
}

vogleditor_stateTreeItem *vogleditor_stateTreeItem::child(int index) const
{
    if (index < 0 || index >= childCount())
    {
        return NULL;
    }

    return m_childItems[index];
}

int vogleditor_stateTreeItem::columnCount() const
{
    int count = 0;
    if (m_parentItem == NULL)
    {
        // this must be the root node
        count = m_columnData.size();
    }
    else
    {
        m_pModel->columnCount();
    }

    return count;
}

QVariant vogleditor_stateTreeItem::columnData(int column, int role) const
{
    if (column >= m_columnData.size())
    {
        return QVariant();
    }

    if (role == Qt::ForegroundRole && parent() != NULL && hasChanged())
    {
        return QVariant(QColor(Qt::red));
    }

    if (role == Qt::ToolTipRole)
    {
        if (hasChanged())
        {
            QString prevValue = getDiffedValue();
            if (prevValue.isEmpty())
                return "";
            else
                return "Previous value was: " + prevValue;
        }
    }

    // catch any other roles that we don't specifically handle
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    // return the data since the only role left is the DisplayRole
    return m_columnData[column];
}

int vogleditor_stateTreeItem::row() const
{
    // note, this is just the row within the current level of the hierarchy
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<vogleditor_stateTreeItem *>(this));

    return 0;
}

bool vogleditor_stateTreeItem::hasChanged() const
{
    for (int i = 0; i < m_childItems.size(); i++)
    {
        if (m_childItems[i]->hasChanged())
        {
            return true;
        }
    }

    return false;
}

//=============================================================================

template <typename T>
vogleditor_stateTreeDatatypeItem<T>::vogleditor_stateTreeDatatypeItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeStateVecDiffableItem(glenumName, "", parent),
      m_name(name),
      m_index(index),
      m_numComponents(numComponents),
      m_isIndexed(isIndexed),
      m_pStateVec(&stateVec)
{
}

template <typename T>
bool vogleditor_stateTreeDatatypeItem<T>::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    static T baseValues[4];
    static T curValues[4];
    VOGL_ASSERT(m_numComponents <= 4);

    bool bFoundInBase = m_pDiffBaseState->get<T>(m_name, m_index, baseValues, m_numComponents, m_isIndexed);
    bool bFoundInCurrent = m_pStateVec->get<T>(m_name, m_index, curValues, m_numComponents, m_isIndexed);

    if (bFoundInBase && bFoundInCurrent)
    {
        for (unsigned int i = 0; i < m_numComponents; i++)
        {
            if (baseValues[i] != curValues[i])
            {
                // one of the values has changed, so return early
                return true;
            }
        }
    }

    if (bFoundInCurrent && !bFoundInBase)
    {
        // the enum must have been added, so it is different
        return true;
    }

    return false;
}

template <typename T>
QString vogleditor_stateTreeDatatypeItem<T>::getDiffedValue() const
{
    static T baseValues[4];
    VOGL_ASSERT(m_numComponents <= 4);

    bool bFoundInBase = m_pDiffBaseState->get<T>(m_name, m_index, baseValues, m_numComponents, m_isIndexed);

    QString result = "";
    if (bFoundInBase)
    {
        result = "TODO: An actual diff'ed value";
    }

    return result;
}

//=============================================================================
vogleditor_stateTreeStateVecBoolItem::vogleditor_stateTreeStateVecBoolItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, bool *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeDatatypeItem<bool>(glenumName, name, index, stateVec, numComponents, isIndexed, parent)
{
    setValue(getValueFromBools(values, numComponents));
}

vogleditor_stateTreeStateVecBoolItem::vogleditor_stateTreeStateVecBoolItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeDatatypeItem<bool>(glenumName, name, index, stateVec, numComponents, isIndexed, parent)
{
    bool bVals[4] = { values[0] != 0, values[1] != 0, values[2] != 0, values[3] != 0 };
    setValue(getValueFromBools(bVals, numComponents));
}

QString vogleditor_stateTreeStateVecBoolItem::getDiffedValue() const
{
    static bool baseValues[4];
    VOGL_ASSERT(m_numComponents <= 4);

    QString result = "";
    if (m_pDiffBaseState->get<bool>(m_name, m_index, baseValues, m_numComponents, m_isIndexed))
    {
        result = getValueFromBools(baseValues, m_numComponents);
    }

    return result;
}

//=============================================================================

vogleditor_stateTreeStateVecIntItem::vogleditor_stateTreeStateVecIntItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeDatatypeItem<int>(glenumName, name, index, stateVec, numComponents, isIndexed, parent)
{
    setValue(getValueFromInts(values, numComponents));
}

QString vogleditor_stateTreeStateVecIntItem::getDiffedValue() const
{
    static int baseValues[4];
    VOGL_ASSERT(m_numComponents <= 4);

    QString result = "";
    if (m_pDiffBaseState->get<int>(m_name, m_index, baseValues, m_numComponents, m_isIndexed))
    {
        result = getValueFromInts(baseValues, m_numComponents);
    }

    return result;
}

//=============================================================================

vogleditor_stateTreeStateVecPtrItem::vogleditor_stateTreeStateVecPtrItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeDatatypeItem<int>(glenumName, name, index, stateVec, numComponents, isIndexed, parent)
{
    setValue(getValueFromPtrs(values, numComponents));
}

QString vogleditor_stateTreeStateVecPtrItem::getDiffedValue() const
{
    static int baseValues[4];
    VOGL_ASSERT(m_numComponents <= 4);

    QString result = "";
    if (m_pDiffBaseState->get<int>(m_name, m_index, baseValues, m_numComponents, m_isIndexed))
    {
        result = getValueFromPtrs(baseValues, m_numComponents);
    }

    return result;
}

//=============================================================================

vogleditor_stateTreeStateVecFloatItem::vogleditor_stateTreeStateVecFloatItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, float *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeDatatypeItem<float>(glenumName, name, index, stateVec, numComponents, isIndexed, parent)
{
    setValue(getValueFromFloats(values, numComponents));
}

QString vogleditor_stateTreeStateVecFloatItem::getDiffedValue() const
{
    static float baseValues[4];
    VOGL_ASSERT(m_numComponents <= 4);

    QString result = "";
    if (m_pDiffBaseState->get<float>(m_name, m_index, baseValues, m_numComponents, m_isIndexed))
    {
        result = getValueFromFloats(baseValues, m_numComponents);
    }

    return result;
}

//=============================================================================

vogleditor_stateTreeStateVecEnumItem::vogleditor_stateTreeStateVecEnumItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeDatatypeItem<int>(glenumName, name, index, stateVec, numComponents, isIndexed, parent)
{
    setValue(getValueFromEnums(values, numComponents));
}

QString vogleditor_stateTreeStateVecEnumItem::getDiffedValue() const
{
    static int baseValues[4];
    VOGL_ASSERT(m_numComponents <= 4);

    QString result = "";
    if (m_pDiffBaseState->get<int>(m_name, m_index, baseValues, m_numComponents, m_isIndexed))
    {
        result = getValueFromEnums(baseValues, m_numComponents);
    }

    return result;
}

//=============================================================================

vogleditor_stateTreeStateVecMatrixRowItem::vogleditor_stateTreeStateVecMatrixRowItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, float *values, unsigned int numComponents, unsigned int rowIndex, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeStateVecDiffableItem(glenumName, "", parent),
      m_name(name),
      m_index(index),
      m_numComponents(numComponents),
      m_isIndexed(isIndexed),
      m_pState(&stateVec),
      m_rowIndex(rowIndex)
{
    setValue(getValueFromFloats(values, 4));
}

bool vogleditor_stateTreeStateVecMatrixRowItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }

    static float baseValues[16];
    static float curValues[16];
    VOGL_ASSERT(m_numComponents <= 16);

    if (m_pDiffBaseState->get<float>(m_name, m_index, baseValues, m_numComponents, m_isIndexed) &&
        m_pState->get<float>(m_name, m_index, curValues, m_numComponents, m_isIndexed))
    {
        for (unsigned int i = 0; i < 4; i++)
        {
            if (baseValues[i + (m_rowIndex * 4)] != curValues[i + (m_rowIndex * 4)])
            {
                // one of the values has changed, so return early
                return true;
            }
        }
    }
    else
    {
        // the enum must have been added, so it is different
        return true;
    }
    return false;
}

QString vogleditor_stateTreeStateVecMatrixRowItem::getDiffedValue() const
{
    static float baseValues[16];
    VOGL_ASSERT(m_numComponents <= 16);

    QString result = "";
    if (m_pDiffBaseState->get<float>(m_name, m_index, baseValues, m_numComponents, m_isIndexed))
    {
        result = getValueFromFloats(&(baseValues[m_rowIndex * 4]), 4);
    }

    return result;
}

vogleditor_stateTreeStateVecMatrixItem::vogleditor_stateTreeStateVecMatrixItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, float *values, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeStateVecDiffableItem(glenumName, "", parent),
      m_pState(&stateVec)
{
    vogleditor_stateTreeStateVecMatrixRowItem *pRow1 = new vogleditor_stateTreeStateVecMatrixRowItem("row 0", name, index, stateVec, &(values[0]), numComponents, 0, isIndexed, this);
    m_rowItems.push_back(pRow1);
    this->appendChild(pRow1);
    vogleditor_stateTreeStateVecMatrixRowItem *pRow2 = new vogleditor_stateTreeStateVecMatrixRowItem("row 1", name, index, stateVec, &(values[4]), numComponents, 1, isIndexed, this);
    m_rowItems.push_back(pRow2);
    this->appendChild(pRow2);
    vogleditor_stateTreeStateVecMatrixRowItem *pRow3 = new vogleditor_stateTreeStateVecMatrixRowItem("row 2", name, index, stateVec, &(values[8]), numComponents, 2, isIndexed, this);
    m_rowItems.push_back(pRow3);
    this->appendChild(pRow3);
    vogleditor_stateTreeStateVecMatrixRowItem *pRow4 = new vogleditor_stateTreeStateVecMatrixRowItem("row 3", name, index, stateVec, &(values[12]), numComponents, 3, isIndexed, this);
    m_rowItems.push_back(pRow4);
    this->appendChild(pRow4);
}
