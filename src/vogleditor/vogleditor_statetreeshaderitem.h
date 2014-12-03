#ifndef VOGLEDITOR_STATETREESHADERITEM_H
#define VOGLEDITOR_STATETREESHADERITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_shader_state;

class vogleditor_stateTreeShaderDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeShaderDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_shader_state &state)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pState(&state),
          m_pDiffBaseState(NULL)
    {
    }

    void set_diff_base_state(const vogl_shader_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    const vogl_shader_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_shader_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    virtual bool hasChanged() const = 0;

protected:
    const vogl_shader_state *m_pState;
    const vogl_shader_state *m_pDiffBaseState;
};

class vogleditor_stateTreeShaderBoolItem : public vogleditor_stateTreeShaderDiffableItem
{
public:
    vogleditor_stateTreeShaderBoolItem(QString name, bool (vogl_shader_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_shader_state &state);
    virtual ~vogleditor_stateTreeShaderBoolItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    bool (vogl_shader_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeShaderLogItem : public vogleditor_stateTreeShaderDiffableItem
{
public:
    vogleditor_stateTreeShaderLogItem(QString name, const vogl::dynamic_string &(vogl_shader_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_shader_state &state);
    virtual ~vogleditor_stateTreeShaderLogItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl::dynamic_string &(vogl_shader_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeShaderSourceLengthItem : public vogleditor_stateTreeShaderDiffableItem
{
public:
    vogleditor_stateTreeShaderSourceLengthItem(QString name, vogleditor_stateTreeItem *parent, const vogl_shader_state &state);
    virtual ~vogleditor_stateTreeShaderSourceLengthItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
};

class vogleditor_stateTreeShaderStringItem : public vogleditor_stateTreeShaderDiffableItem
{
public:
    vogleditor_stateTreeShaderStringItem(QString name, const vogl::dynamic_string &(vogl_shader_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_shader_state &state);
    virtual ~vogleditor_stateTreeShaderStringItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    dynamic_string m_value;
    const vogl::dynamic_string &(vogl_shader_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeShaderItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeShaderItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_shader_state &state);
    virtual ~vogleditor_stateTreeShaderItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }
    virtual state_tree_type getStateType() const
    {
        return vogleditor_stateTreeItem::cSHADER;
    }

    vogl_shader_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_shader_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    void add_diffable_child(vogleditor_stateTreeShaderDiffableItem *pItem);

    void set_diff_base_state(const vogl_shader_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeShaderDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

private:
    vogl_shader_state *m_pState;
    const vogl_shader_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeShaderDiffableItem *> m_diffableItems;
};
#endif // VOGLEDITOR_STATETREESHADERITEM_H
