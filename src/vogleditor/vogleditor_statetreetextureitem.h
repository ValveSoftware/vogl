#ifndef VOGLEDITOR_STATETREETEXTUREITEM_H
#define VOGLEDITOR_STATETREETEXTUREITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_texture_state;

class vogleditor_stateTreeTextureItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeTextureItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_texture_state *pState, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeTextureItem()
    {
        m_pTexture = NULL;
        m_pDiffBaseState = NULL;
    }

    virtual state_tree_type getStateType() const
    {
        return vogleditor_stateTreeItem::cTEXTURE;
    }
    vogl_texture_state *get_texture_state() const
    {
        return m_pTexture;
    }

    void set_diff_base_state(const vogl_texture_state *pBaseState);

private:
    vogl_texture_state *m_pTexture;
    const vogl_texture_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
};
#endif // VOGLEDITOR_STATETREETEXTUREITEM_H
