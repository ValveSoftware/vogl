#ifndef VOGLEDITOR_STATETREESAMPLERITEM_H
#define VOGLEDITOR_STATETREESAMPLERITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_sampler_state;

class vogleditor_stateTreeSamplerItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeSamplerItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_sampler_state *pState, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeSamplerItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    const vogl_sampler_state *get_sampler_state() const
    {
        return m_pState;
    }

    void set_diff_base_state(const vogl_sampler_state *pBaseState);

private:
    const vogl_sampler_state *m_pState;
    const vogl_sampler_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREESAMPLERITEM_H
