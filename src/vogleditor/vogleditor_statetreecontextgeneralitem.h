#ifndef VOGLEDITOR_STATETREECONTEXTGENERALITEM_H
#define VOGLEDITOR_STATETREECONTEXTGENERALITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_general_context_state.h"

class vogl_state_vector;

class vogleditor_stateTreeContextGeneralCompressTextureFormatItem : public vogleditor_stateTreeDatatypeItem<int>
{
public:
    vogleditor_stateTreeContextGeneralCompressTextureFormatItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int formatEnum, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeContextGeneralCompressTextureFormatItem()
    {
    }

    virtual void set_diff_base_state(const vogl_state_vector *pBaseState)
    {
        VOGL_NOTE_UNUSED(pBaseState);
        VOGL_ASSERT(!"This version of the function is not supported for vogleditor_stateTreeContextGeneralCompressTextureFormatItem");
    }
    void set_diff_base_state(const vogl_general_context_state *pBaseGeneralState)
    {
        m_pDiffBaseGeneralState = pBaseGeneralState;
    }

    virtual bool hasChanged() const;

private:
    int m_formatEnum;
    const vogl_general_context_state *m_pDiffBaseGeneralState;
};

class vogleditor_stateTreeContextGeneralItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextGeneralItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_general_context_state &generalState, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeContextGeneralItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    vogl_general_context_state *get_context_general_state() const
    {
        return m_pState;
    }
    void set_diff_base_state(const vogl_general_context_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeStateVecDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(static_cast<const vogl_state_vector *>(pBaseState));
        }

        for (vogleditor_stateTreeContextGeneralCompressTextureFormatItem **iter = m_formatItems.begin(); iter != m_formatItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

private:
    vogl_general_context_state *m_pState;
    const vogl_general_context_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
    vogl::vector<vogleditor_stateTreeContextGeneralCompressTextureFormatItem *> m_formatItems;
};

#endif // VOGLEDITOR_STATETREECONTEXTGENERALITEM_H
