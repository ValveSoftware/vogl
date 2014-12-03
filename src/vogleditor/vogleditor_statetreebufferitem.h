#ifndef VOGLEDITOR_QSTATETREEBUFFERITEM_H
#define VOGLEDITOR_QSTATETREEBUFFERITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_buffer_state;

class vogleditor_stateTreeBufferItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeBufferItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_buffer_state *pState);
    virtual ~vogleditor_stateTreeBufferItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }
    virtual state_tree_type getStateType() const
    {
        return vogleditor_stateTreeItem::cBUFFER;
    }

    const vogl_buffer_state *get_buffer_state() const
    {
        return m_pState;
    }

    void set_diff_base_state(const vogl_buffer_state *pBaseState);

private:
    const vogl_buffer_state *m_pState;
    const vogl_buffer_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_QSTATETREEBUFFERITEM_H
