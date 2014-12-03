#ifndef VOGLEDITOR_STATETREESYNCITEM_H
#define VOGLEDITOR_STATETREESYNCITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_sync_state;

class vogleditor_stateTreeSyncItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeSyncItem(QString name, vogleditor_stateTreeItem *parent, const vogl_sync_state &state);
    virtual ~vogleditor_stateTreeSyncItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    const vogl_sync_state *get_current_state() const
    {
        return m_pState;
    }

    virtual void set_diff_base_state(const vogl_sync_state *pBaseState);

    virtual bool hasChanged() const;

private:
    const vogl_sync_state *m_pState;
    const vogl_sync_state *m_pDiffBaseState;
};

#endif // VOGLEDITOR_STATETREESYNCITEM_H
