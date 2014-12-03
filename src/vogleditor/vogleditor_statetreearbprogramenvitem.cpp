#include "vogleditor_statetreearbprogramenvitem.h"

vogleditor_stateTreeArbProgramEnvParameterItem::vogleditor_stateTreeArbProgramEnvParameterItem(unsigned int paramIndex, vec4F_vec &envParams, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeItem(int_to_string(paramIndex), getValueFromFloats(envParams[paramIndex].get_ptr(), 4), parent),
      m_paramIndex(paramIndex),
      m_state(envParams[paramIndex]),
      m_pDiffBaseState(NULL)
{
}

bool vogleditor_stateTreeArbProgramEnvParameterItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }

    if (m_paramIndex >= m_pDiffBaseState->size())
    {
        // this index is not in the base state, so it must be new
        return true;
    }

    if (m_state != (*m_pDiffBaseState)[m_paramIndex])
        return true;

    return false;
}

QString vogleditor_stateTreeArbProgramEnvParameterItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    if (m_paramIndex >= m_pDiffBaseState->size())
    {
        // the current index is beyond the end of the base vector. This shouldn't be possible, but handle it anyway
        return "non-existent";
    }

    return getValueFromFloats((*m_pDiffBaseState)[m_paramIndex].get_ptr(), 4);
}

//=============================================================================
vogleditor_stateTreeArbProgramEnvItem::vogleditor_stateTreeArbProgramEnvItem(QString name, unsigned int index, vogleditor_stateTreeItem *parentNode, vogl_arb_program_environment_state &state)
    : vogleditor_stateTreeItem(name, "", parentNode),
      m_index(index),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    setValue(m_pState->get_cur_program(index));

    vec4F_vec envParams = state.get_env_params(index);
    for (uint i = 0; i < envParams.size(); i++)
    {
        this->appendChild(new vogleditor_stateTreeArbProgramEnvParameterItem(i, envParams, this));
    }
}

vogleditor_stateTreeArbProgramEnvItem::~vogleditor_stateTreeArbProgramEnvItem()
{
    m_pState = NULL;
    m_pDiffBaseState = NULL;
}

bool vogleditor_stateTreeArbProgramEnvItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    for (unsigned int i = 0; i < m_pDiffBaseState->cNumTargets; i++)
    {
        if (m_pDiffBaseState->get_target_enum(i) == m_pState->get_target_enum(m_index))
        {
            if (m_pDiffBaseState->get_cur_program(i) != m_pState->get_cur_program(m_index))
            {
                // the current program is different
                return true;
            }

            // Also mark as different if any parameters are different
            for (QList<vogleditor_stateTreeItem *>::const_iterator childIter = m_childItems.begin(); childIter != m_childItems.end(); childIter++)
            {
                const vogleditor_stateTreeArbProgramEnvParameterItem *pItem = static_cast<const vogleditor_stateTreeArbProgramEnvParameterItem *>(*childIter);
                if (pItem->hasChanged())
                    return true;
            }

            // the target was found, but we couldn't identify any differences
            return false;
        }
    }

    // didn't find the current enum in the base state, so this one must be new
    return true;
}

QString vogleditor_stateTreeArbProgramEnvItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    for (unsigned int i = 0; i < m_pDiffBaseState->cNumTargets; i++)
    {
        if (m_pDiffBaseState->get_target_enum(i) == m_pState->get_target_enum(m_index))
        {
            GLuint program = m_pDiffBaseState->get_cur_program(i);
            return getValueFromUints(&program, 1);
        }
    }

    // didn't find the current enum in the base state, so this one must be new
    return "non-existent";
}
