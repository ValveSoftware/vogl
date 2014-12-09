#ifndef VOGLEDITOR_STATETREEARBPROGRAMITEM_H
#define VOGLEDITOR_STATETREEARBPROGRAMITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_arb_program_state;

class vogleditor_stateTreeArbProgramDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeArbProgramDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pState(&state),
          m_pDiffBaseState(NULL)
    {
    }

    virtual void set_diff_base_state(const vogl_arb_program_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    const vogl_arb_program_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_arb_program_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    virtual bool hasChanged() const = 0;

protected:
    const vogl_arb_program_state *m_pState;
    const vogl_arb_program_state *m_pDiffBaseState;
};

class vogleditor_stateTreeArbProgramBoolItem : public vogleditor_stateTreeArbProgramDiffableItem
{
public:
    vogleditor_stateTreeArbProgramBoolItem(QString name, bool (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state);
    virtual ~vogleditor_stateTreeArbProgramBoolItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    bool (vogl_arb_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeArbProgramUIntItem : public vogleditor_stateTreeArbProgramDiffableItem
{
public:
    vogleditor_stateTreeArbProgramUIntItem(QString name, uint (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state);
    virtual ~vogleditor_stateTreeArbProgramUIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    uint (vogl_arb_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeArbProgramIntItem : public vogleditor_stateTreeArbProgramDiffableItem
{
public:
    vogleditor_stateTreeArbProgramIntItem(QString name, GLint (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state);
    virtual ~vogleditor_stateTreeArbProgramIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    GLint (vogl_arb_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeArbProgramEnumItem : public vogleditor_stateTreeArbProgramDiffableItem
{
public:
    vogleditor_stateTreeArbProgramEnumItem(QString name, GLenum (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state);
    virtual ~vogleditor_stateTreeArbProgramEnumItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    GLenum (vogl_arb_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeArbProgramStringItem : public vogleditor_stateTreeArbProgramDiffableItem
{
public:
    vogleditor_stateTreeArbProgramStringItem(QString name, const uint8_vec &(vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state);
    virtual ~vogleditor_stateTreeArbProgramStringItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const uint8_vec &(vogl_arb_program_state::*m_pFunc)(void) const;
};

class vogleditor_stateTreeArbProgramParamItem : public vogleditor_stateTreeArbProgramDiffableItem
{
public:
    vogleditor_stateTreeArbProgramParamItem(QString name, unsigned int paramIndex, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state);
    virtual ~vogleditor_stateTreeArbProgramParamItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    unsigned int m_paramIndex;
};

class vogleditor_stateTreeArbProgramItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeArbProgramItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_arb_program_state &state);
    virtual ~vogleditor_stateTreeArbProgramItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }
    virtual state_tree_type getStateType() const
    {
        return vogleditor_stateTreeItem::cPROGRAMARB;
    }

    vogl_arb_program_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_arb_program_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    void set_diff_base_state(const vogl_arb_program_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeArbProgramDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

private:
    vogl_arb_program_state *m_pState;
    const vogl_arb_program_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeArbProgramDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREEARBPROGRAMITEM_H
