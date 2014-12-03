#include "vogleditor_statetreeprogrampipelineitem.h"
#include "vogleditor_statetreeprogramitem.h"

//=============================================================================
vogleditor_stateTreeProgramPipelineUIntItem::vogleditor_stateTreeProgramPipelineUIntItem(QString name, uint (vogl_sso_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_sso_state &state)
    : vogleditor_stateTreeProgramPipelineDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    uint val = (state.*func)();
    setValue(val);
}

bool vogleditor_stateTreeProgramPipelineUIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeProgramPipelineUIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint value = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromUints(&value, 1);
}

//=============================================================================
vogleditor_stateTreeProgramPipelineUIntVecItem::vogleditor_stateTreeProgramPipelineUIntVecItem(QString name, uint (vogl_sso_state::*func)(uint) const, uint index, vogleditor_stateTreeItem *parent, const vogl_sso_state &state)
    : vogleditor_stateTreeProgramPipelineDiffableItem(name, "", parent, state),
      m_pFunc(func),
      m_index(index)
{
    uint val = (state.*func)(m_index);
    setValue(val);
}

bool vogleditor_stateTreeProgramPipelineUIntVecItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)(m_index) != (m_pDiffBaseState->*m_pFunc)(m_index);
}

QString vogleditor_stateTreeProgramPipelineUIntVecItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint value = (m_pDiffBaseState->*m_pFunc)(m_index);
    return getValueFromUints(&value, 1);
}

//=============================================================================
vogleditor_stateTreeProgramPipelineItem::vogleditor_stateTreeProgramPipelineItem(QString name, GLuint64 handle, vogleditor_stateTreeItem *parent, const vogl_sso_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_handle(handle),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    // TODO : Implement this
    this->appendChild(new vogleditor_stateTreeProgramPipelineUIntItem("GL_ACTIVE_PROGRAM", &vogl_sso_state::get_active_program, this, state));
    this->appendChild(new vogleditor_stateTreeProgramPipelineUIntItem("GL_INFO_LOG_LENGTH", &vogl_sso_state::get_info_log_length, this, state));
    // Ideally for Program Objs in state tree display, we could create stateTreeProgramItem objects, but need lots of plumbing to get there.
    // Currently only displaying Programs that have non-zero Prog Obj bound. This saves space and avoids any issue of querying for unsupported shader types on MESA.
    for (uint32_t i = 0; i < m_pState->cNumShaders; i++)
    {
        if (0 != m_pState->get_shader_program(i))
            this->appendChild(new vogleditor_stateTreeProgramPipelineUIntVecItem(enum_to_string(vogl_sso_state::gl_shader_type_mapping[i]), &vogl_sso_state::get_shader_program, i, this, state));
    }
}
