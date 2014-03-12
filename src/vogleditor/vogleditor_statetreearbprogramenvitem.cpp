#include "vogleditor_statetreearbprogramenvitem.h"

#include "vogl_arb_program_state.h"

vogleditor_stateTreeArbProgramEnvItem::vogleditor_stateTreeArbProgramEnvItem(QString name, unsigned int index, vogleditor_stateTreeItem* parentNode, vogl_arb_program_environment_state& state)
   : vogleditor_stateTreeItem(name, "", parentNode),
     m_index(index),
     m_pState(&state),
     m_pDiffBaseState(NULL)
{
   setValue(m_pState->get_cur_program(index));
}

bool vogleditor_stateTreeArbProgramEnvItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    for (unsigned int i = 0; i < m_pDiffBaseState->cNumTargets; i++)
    {
        if (m_pDiffBaseState->get_target_enum(i) == m_pState->get_target_enum(m_index))
        {
            return m_pDiffBaseState->get_cur_program(i) != m_pState->get_cur_program(m_index);
        }
    }

    // didn't find the current enum in the base state, so this one must be new
    return true;
}
