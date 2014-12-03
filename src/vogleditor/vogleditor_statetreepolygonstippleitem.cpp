#include "vogleditor_statetreepolygonstippleitem.h"

vogleditor_stateTreePolygonStippleRowItem::vogleditor_stateTreePolygonStippleRowItem(QString name, unsigned int rowIndex, vogleditor_stateTreeItem *parent, const vogl_polygon_stipple_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_rowIndex(rowIndex),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    setValue(uint32_to_bits(m_pState->get_pattern_row(m_rowIndex)));
}

bool vogleditor_stateTreePolygonStippleRowItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }

    if (m_rowIndex >= m_pDiffBaseState->get_num_pattern_rows())
        return true;

    uint32_t curRow = m_pState->get_pattern_row(m_rowIndex);
    uint32_t baseRow = m_pDiffBaseState->get_pattern_row(m_rowIndex);

    return curRow != baseRow;
}

QString vogleditor_stateTreePolygonStippleRowItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
    {
        return "";
    }

    if (m_rowIndex >= m_pDiffBaseState->get_num_pattern_rows())
        return "non-existent";

    uint32_t baseRow = m_pDiffBaseState->get_pattern_row(m_rowIndex);

    return uint32_to_bits(baseRow);
}

vogleditor_stateTreePolygonStippleItem::vogleditor_stateTreePolygonStippleItem(QString name, vogleditor_stateTreeItem *parent, const vogl_polygon_stipple_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    QString tmp;
    for (uint i = 0; i < m_pState->get_num_pattern_rows(); i++)
    {
        vogleditor_stateTreePolygonStippleRowItem *pRowNode = new vogleditor_stateTreePolygonStippleRowItem(tmp.sprintf("Row %u", i), i, this, state);
        m_rowItems.push_back(pRowNode);
        this->appendChild(pRowNode);
    }
}
