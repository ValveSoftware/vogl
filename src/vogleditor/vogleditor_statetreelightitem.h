#ifndef VOGLEDITOR_STATETREELIGHTITEM_H
#define VOGLEDITOR_STATETREELIGHTITEM_H

#include "vogleditor_statetreeitem.h"

class vogleditor_stateTreeLightItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeLightItem(QString name, unsigned int lightIndex, vogleditor_stateTreeItem *parent, const vogl_state_vector *pState);
    virtual ~vogleditor_stateTreeLightItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_state_vector *pBaseState);

    unsigned int get_light_index() const
    {
        return m_lightIndex;
    }

private:
    unsigned int m_lightIndex;
    const vogl_state_vector *m_pState;
    const vogl_state_vector *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREELIGHTITEM_H
