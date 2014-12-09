#ifndef VOGLEDITOR_STATETREEPROGRAMPIPELINEITEM_H
#define VOGLEDITOR_STATETREEPROGRAMPIPELINEITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_sso_state.h"

class vogleditor_stateTreeProgramPipelineDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramPipelineDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_sso_state &state)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pState(&state),
          m_pDiffBaseState(NULL)
    {
    }

    void set_diff_base_state(const vogl_sso_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }
    const vogl_sso_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_sso_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    virtual bool hasChanged() const = 0;

protected:
    const vogl_sso_state *m_pState;
    const vogl_sso_state *m_pDiffBaseState;
};

class vogleditor_stateTreeProgramPipelineUIntItem : public vogleditor_stateTreeProgramPipelineDiffableItem
{
public:
    vogleditor_stateTreeProgramPipelineUIntItem(QString name, uint (vogl_sso_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_sso_state &state);
    virtual ~vogleditor_stateTreeProgramPipelineUIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    uint (vogl_sso_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeProgramPipelineUIntVecItem : public vogleditor_stateTreeProgramPipelineDiffableItem
{
public:
    vogleditor_stateTreeProgramPipelineUIntVecItem(QString name, uint (vogl_sso_state::*func)(uint) const, uint index, vogleditor_stateTreeItem *parent, const vogl_sso_state &state);
    virtual ~vogleditor_stateTreeProgramPipelineUIntVecItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    uint (vogl_sso_state::*m_pFunc)(uint) const;
    uint m_index;
};

class vogleditor_stateTreeProgramPipelineItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramPipelineItem(QString name, GLuint64 handle, vogleditor_stateTreeItem *parent, const vogl_sso_state &state);
    virtual ~vogleditor_stateTreeProgramPipelineItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_sso_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeProgramPipelineDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

    GLuint64 get_handle() const
    {
        return m_handle;
    }

private:
    GLuint64 m_handle;
    const vogl_sso_state *m_pState;
    const vogl_sso_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeProgramPipelineDiffableItem *> m_diffableItems;
};
#endif // VOGLEDITOR_STATETREEPROGRAMPIPELINEITEM_H
