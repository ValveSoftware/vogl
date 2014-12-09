#ifndef VOGLEDITOR_STATETREEARBPROGRAMENVITEM_H
#define VOGLEDITOR_STATETREEARBPROGRAMENVITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_arb_program_state.h"

class vogleditor_stateTreeArbProgramEnvParameterItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeArbProgramEnvParameterItem(unsigned int paramIndex, vec4F_vec &envParams, vogleditor_stateTreeItem *parent);
    virtual ~vogleditor_stateTreeArbProgramEnvParameterItem()
    {
    }

    virtual void set_diff_base_state(const vec4F_vec *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    unsigned int m_paramIndex;
    vec4F m_state;
    const vec4F_vec *m_pDiffBaseState;
};

class vogleditor_stateTreeArbProgramEnvItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeArbProgramEnvItem(QString name, unsigned int index, vogleditor_stateTreeItem *parent, vogl_arb_program_environment_state &state);
    virtual ~vogleditor_stateTreeArbProgramEnvItem();

    vogl_arb_program_environment_state *get_current_state() const
    {
        return m_pState;
    }
    const vogl_arb_program_environment_state *get_base_state() const
    {
        return m_pDiffBaseState;
    }

    virtual void set_diff_base_state(const vogl_arb_program_environment_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;

        for (QList<vogleditor_stateTreeItem *>::iterator iter = m_childItems.begin(); iter != m_childItems.end(); iter++)
        {
            vogleditor_stateTreeArbProgramEnvParameterItem *pItem = static_cast<vogleditor_stateTreeArbProgramEnvParameterItem *>(*iter);

            if (pBaseState != NULL)
            {
                pItem->set_diff_base_state(&(pBaseState->get_env_params(m_index)));
            }
            else
            {
                pItem->set_diff_base_state(NULL);
            }
        }
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    unsigned int m_index;
    vogl_arb_program_environment_state *m_pState;
    const vogl_arb_program_environment_state *m_pDiffBaseState;
};

#endif // VOGLEDITOR_STATETREEARBPROGRAMENVITEM_H
