#ifndef VOGLEDITOR_STATETREETEXENVITEM_H
#define VOGLEDITOR_STATETREETEXENVITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_texenv_state;

class vogleditor_stateTreeTexEnvStateVecDiffableItem : public vogleditor_stateTreeStateVecDiffableItem
{
public:
    vogleditor_stateTreeTexEnvStateVecDiffableItem(GLenum target, GLenum enumId, unsigned int index, unsigned int numComponents, QString name, QString value, vogleditor_stateTreeItem *parent);

    virtual void set_diff_base_state(const vogl_state_vector *pBaseState)
    {
        VOGL_NOTE_UNUSED(pBaseState);
        VOGL_ASSERT(!"This version of the function is not supported for vogleditor_stateTreeTexEnvStateVecDiffableItem");
    }
    void set_diff_base_state(const vogl_context_info *pInfo, const vogl_texenv_state *pBaseState)
    {
        m_pDiffBaseInfo = pInfo;
        m_pDiffBaseState = pBaseState;
    }

    GLenum get_target() const
    {
        return m_target;
    }

protected:
    GLenum m_target;
    GLenum m_name;
    unsigned int m_index;
    unsigned int m_numComponents;
    const vogl_context_info *m_pDiffBaseInfo;
    const vogl_texenv_state *m_pDiffBaseState;
};

class vogleditor_stateTreeTexEnvStateVecEnumItem : public vogleditor_stateTreeTexEnvStateVecDiffableItem
{
public:
    vogleditor_stateTreeTexEnvStateVecEnumItem(GLenum target, QString glenumName, GLenum name, unsigned int index, int *values, unsigned int numComponents, vogleditor_stateTreeItem *parent, vogl_texenv_state &state);
    virtual ~vogleditor_stateTreeTexEnvStateVecEnumItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    vogl_texenv_state *m_pState;
};

class vogleditor_stateTreeTexEnvStateVecFloatItem : public vogleditor_stateTreeTexEnvStateVecDiffableItem
{
public:
    vogleditor_stateTreeTexEnvStateVecFloatItem(GLenum target, QString glenumName, GLenum name, unsigned int index, float *values, unsigned int numComponents, vogleditor_stateTreeItem *parent, vogl_texenv_state &state);
    virtual ~vogleditor_stateTreeTexEnvStateVecFloatItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    vogl_texenv_state *m_pState;
};

class vogleditor_stateTreeTexEnvItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeTexEnvItem(QString name, vogleditor_stateTreeItem *parent, vogl_texenv_state &state, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeTexEnvItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
        m_pDiffBaseInfo = NULL;
    }

    void set_diff_base_state(const vogl_context_info *pInfo, const vogl_texenv_state *pBaseState);

private:
    vogl_texenv_state *m_pState;
    const vogl_texenv_state *m_pDiffBaseState;
    const vogl_context_info *m_pDiffBaseInfo;
    vogl::vector<vogleditor_stateTreeTexEnvStateVecDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREETEXENVITEM_H
