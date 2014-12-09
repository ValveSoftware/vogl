#ifndef VOGLEDITOR_STATETREEDISPLAYLISTITEM_H
#define VOGLEDITOR_STATETREEDISPLAYLISTITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_display_list_state;

class vogleditor_stateTreeDisplaylistItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeDisplaylistItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_display_list_state *pState);
    virtual ~vogleditor_stateTreeDisplaylistItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_display_list_state *pBaseState);

private:
    vogl_display_list_state *m_pState;
    const vogl_display_list_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREEDISPLAYLISTITEM_H
