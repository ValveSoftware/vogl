#include "vogleditor_statetreematrixitem.h"
#include "vogl_matrix.h"

vogleditor_stateTreeMatrixRowItem::vogleditor_stateTreeMatrixRowItem(QString name, unsigned int rowIndex, vogleditor_stateTreeItem *parent, const matrix44D &matrix)
    : vogleditor_stateTreeItem(name, "", parent),
      m_rowIndex(rowIndex),
      m_pState(&matrix),
      m_pDiffBaseState(NULL)
{
    const matrix44D::row_vec &row = matrix.get_row(rowIndex);
    QString tmp;
    setValue(tmp.sprintf("%lf, %lf, %lf, %lf", row[0], row[1], row[2], row[3]));
}

bool vogleditor_stateTreeMatrixRowItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }

    const matrix44D::row_vec &curRow = m_pState->get_row(m_rowIndex);
    const matrix44D::row_vec &baseRow = m_pDiffBaseState->get_row(m_rowIndex);
    if ((curRow[0] != baseRow[0]) ||
        (curRow[1] != baseRow[1]) ||
        (curRow[2] != baseRow[2]) ||
        (curRow[3] != baseRow[3]))
    {
        return true;
    }

    return false;
}

QString vogleditor_stateTreeMatrixRowItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
    {
        return "";
    }

    const matrix44D::row_vec &baseRow = m_pDiffBaseState->get_row(m_rowIndex);

    return getValueFromDoubles(baseRow.get_ptr(), 4);
}

vogleditor_stateTreeMatrixItem::vogleditor_stateTreeMatrixItem(QString name, vogleditor_stateTreeItem *parent, const matrix44D &matrix, unsigned int stackIndex)
    : vogleditor_stateTreeItem(name, "", parent),
      m_stackIndex(stackIndex),
      m_pState(&matrix),
      m_pDiffBaseState(NULL)
{
    {
        vogleditor_stateTreeMatrixRowItem *pItem = new vogleditor_stateTreeMatrixRowItem("row 0", 0, this, matrix);
        m_rowItems.push_back(pItem);
        this->appendChild(pItem);
    }
    {
        vogleditor_stateTreeMatrixRowItem *pItem = new vogleditor_stateTreeMatrixRowItem("row 1", 1, this, matrix);
        m_rowItems.push_back(pItem);
        this->appendChild(pItem);
    }
    {
        vogleditor_stateTreeMatrixRowItem *pItem = new vogleditor_stateTreeMatrixRowItem("row 2", 2, this, matrix);
        m_rowItems.push_back(pItem);
        this->appendChild(pItem);
    }
    {
        vogleditor_stateTreeMatrixRowItem *pItem = new vogleditor_stateTreeMatrixRowItem("row 3", 3, this, matrix);
        m_rowItems.push_back(pItem);
        this->appendChild(pItem);
    }
}

vogleditor_stateTreeMatrixStackItem::vogleditor_stateTreeMatrixStackItem(QString name, GLenum target, unsigned int index, vogleditor_stateTreeItem *parent, const vogl_matrix_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_target(target),
      m_index(index),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    uint stack_depth = m_pState->get_matrix_stack_depth(m_target, m_index);
    for (uint i = 0; i < stack_depth; i++)
    {
        QString tmpName;
        tmpName = tmpName.sprintf("%d", i);
        if (i == stack_depth - 1)
        {
            tmpName = tmpName.append(" (top)");
        }
        if (i == 0)
        {
            tmpName = tmpName.append(" (bottom)");
        }

        const matrix44D *pMatrix = m_pState->get_matrix(m_target, m_index, i);
        if (pMatrix != NULL)
        {
            vogleditor_stateTreeMatrixItem *pItem = new vogleditor_stateTreeMatrixItem(tmpName, this, *pMatrix, i);
            m_matrixItems.push_back(pItem);
            this->appendChild(pItem);
        }
    }
}
