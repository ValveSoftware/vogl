#ifndef VOGLEDITOR_STATETREEPROGRAMITEM_H
#define VOGLEDITOR_STATETREEPROGRAMITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_program_state.h"

class vogleditor_stateTreeProgramDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_program_state &state)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pState(&state),
          m_pDiffBaseState(NULL)
    {
    }

    virtual void set_diff_base_state(const vogl_program_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    const vogl_program_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_program_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    virtual bool hasChanged() const = 0;

protected:
    const vogl_program_state *m_pState;
    const vogl_program_state *m_pDiffBaseState;
};

class vogleditor_stateTreeProgramBoolItem : public vogleditor_stateTreeProgramDiffableItem
{
public:
    vogleditor_stateTreeProgramBoolItem(QString name, bool (vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state);
    virtual ~vogleditor_stateTreeProgramBoolItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    bool (vogl_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeProgramUIntItem : public vogleditor_stateTreeProgramDiffableItem
{
public:
    vogleditor_stateTreeProgramUIntItem(QString name, uint (vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state);
    virtual ~vogleditor_stateTreeProgramUIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    uint (vogl_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeProgramEnumItem : public vogleditor_stateTreeProgramDiffableItem
{
public:
    vogleditor_stateTreeProgramEnumItem(QString name, GLenum (vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state);
    virtual ~vogleditor_stateTreeProgramEnumItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    GLenum (vogl_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeProgramLogItem : public vogleditor_stateTreeProgramDiffableItem
{
public:
    vogleditor_stateTreeProgramLogItem(QString name, const vogl::dynamic_string &(vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state);
    virtual ~vogleditor_stateTreeProgramLogItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl::dynamic_string &(vogl_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeProgramStringItem : public vogleditor_stateTreeProgramDiffableItem
{
public:
    vogleditor_stateTreeProgramStringItem(QString name, const vogl::dynamic_string &(vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state);
    virtual ~vogleditor_stateTreeProgramStringItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    dynamic_string m_value;
    const vogl::dynamic_string &(vogl_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeProgramAttribItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramAttribItem(QString name, vogleditor_stateTreeItem *parent, const vogl_program_attrib_state &state);
    virtual ~vogleditor_stateTreeProgramAttribItem()
    {
        m_pState = NULL;
    }

    virtual void set_diff_base_state(const vogl_attrib_state_vec *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

    const dynamic_string &get_name() const
    {
        return m_pState->m_name;
    }

private:
    const vogl_program_attrib_state *m_pState;
    const vogl_attrib_state_vec *m_pDiffBaseState;
};

class vogleditor_stateTreeProgramUniformItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramUniformItem(QString name, vogleditor_stateTreeItem *parent, const vogl_program_uniform_state &state);
    virtual ~vogleditor_stateTreeProgramUniformItem()
    {
        m_pState = NULL;
    }

    virtual void set_diff_base_state(const vogl_uniform_state_vec *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

    const dynamic_string &get_name() const
    {
        return m_pState->m_name;
    }

private:
    const vogl_program_uniform_state *m_pState;
    const vogl_uniform_state_vec *m_pDiffBaseState;
};

class vogleditor_stateTreeProgramItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_program_state &state, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeProgramItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }
    virtual state_tree_type getStateType() const
    {
        return vogleditor_stateTreeItem::cPROGRAM;
    }

    vogl_program_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_program_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    void add_diffable_child(vogleditor_stateTreeProgramDiffableItem *pItem);

    void set_diff_base_state(const vogl_program_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeProgramDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }

        for (vogleditor_stateTreeProgramAttribItem **iter = m_attribItems.begin(); iter != m_attribItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(&(pBaseState->get_attrib_state_vec()));
        }

        for (vogleditor_stateTreeProgramUniformItem **iter = m_uniformItems.begin(); iter != m_uniformItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(&(pBaseState->get_uniform_state_vec()));
        }
    }

private:
    vogl_program_state *m_pState;
    const vogl_program_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeProgramDiffableItem *> m_diffableItems;
    vogl::vector<vogleditor_stateTreeProgramAttribItem *> m_attribItems;
    vogl::vector<vogleditor_stateTreeProgramUniformItem *> m_uniformItems;
};
#endif // VOGLEDITOR_STATETREEPROGRAMITEM_H
