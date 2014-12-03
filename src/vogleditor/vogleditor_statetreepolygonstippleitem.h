#ifndef VOGLEDITOR_STATETREEPOLYGONSTIPPLEITEM_H
#define VOGLEDITOR_STATETREEPOLYGONSTIPPLEITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_general_context_state.h"

class vogleditor_stateTreePolygonStippleRowItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreePolygonStippleRowItem(QString name, unsigned int rowIndex, vogleditor_stateTreeItem *parent, const vogl_polygon_stipple_state &state);

    virtual ~vogleditor_stateTreePolygonStippleRowItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_polygon_stipple_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    unsigned int m_rowIndex;
    const vogl_polygon_stipple_state *m_pState;
    const vogl_polygon_stipple_state *m_pDiffBaseState;

    QString uint32_to_bits(uint32_t id) const
    {
        QString tmp;
        QString tmp2;
        uint32_t size = sizeof(uint32_t);
        uint32_t maxPow = 1 << (size * 8 - 1);
        for (uint32_t i = 0; i < size * 8; ++i)
        {
            // print last bit and shift left.
            tmp += tmp2.sprintf("%u", id & maxPow ? 1 : 0);
            id = id << 1;
        }
        return tmp;
    }
};

class vogleditor_stateTreePolygonStippleItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreePolygonStippleItem(QString name, vogleditor_stateTreeItem *parent, const vogl_polygon_stipple_state &state);

    virtual ~vogleditor_stateTreePolygonStippleItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_polygon_stipple_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;

        for (vogleditor_stateTreePolygonStippleRowItem **iter = m_rowItems.begin(); iter != m_rowItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

private:
    const vogl_polygon_stipple_state *m_pState;
    const vogl_polygon_stipple_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreePolygonStippleRowItem *> m_rowItems;
};

#endif // VOGLEDITOR_STATETREEPOLYGONSTIPPLEITEM_H
