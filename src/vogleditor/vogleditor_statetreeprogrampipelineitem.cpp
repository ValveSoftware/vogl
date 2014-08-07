#include "vogleditor_statetreeprogrampipelineitem.h"

#include "vogl_sso_state.h"

vogleditor_stateTreeProgramPipelineIntItem::vogleditor_stateTreeProgramPipelineIntItem(QString name, GLenum enumName, vogleditor_stateTreeItem* parent, const vogl_sso_state& state)
    : vogleditor_stateTreeProgramPipelineDiffableItem(name, "", parent),
      m_enum(enumName),
      m_pState(&state)
{
    if (m_pState->get_shader_int(m_enum))
    {
        setValue(m_pState->get_shader_int(m_enum));
    }
}

bool vogleditor_stateTreeProgramPipelineIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    int curVal = m_pState->get_shader_int(m_enum);
    int baseVal = m_pState->get_shader_int(m_enum);

    if (!curVal || !baseVal)
        return true;

    return (curVal != baseVal);
}

QString vogleditor_stateTreeProgramPipelineIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int baseVal = m_pDiffBaseState->get_shader_int(m_enum);
    if (!baseVal)
    {
        return "non-existent";
    }

    return getValueFromInts(&baseVal, 1);
}

//=============================================================================

vogleditor_stateTreeProgramPipelineEnumItem::vogleditor_stateTreeProgramPipelineEnumItem(QString name, GLenum enumName, vogleditor_stateTreeItem* parent, const vogl_sso_state& state)
    : vogleditor_stateTreeProgramPipelineDiffableItem(name, "", parent),
      m_enum(enumName),
      m_pState(&state)
{
    int val = m_pState->get_shader_int(m_enum);
    if (val)
    {
        setValue(enum_to_string(val));
    }
}

bool vogleditor_stateTreeProgramPipelineEnumItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    int curVal = m_pState->get_shader_int(m_enum);
    int baseVal = m_pDiffBaseState->get_shader_int(m_enum);

    if (!curVal || !baseVal)
        return true;

    return (curVal != baseVal);
}

QString vogleditor_stateTreeProgramPipelineEnumItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int baseVal = m_pDiffBaseState->get_shader_int(m_enum);
    if (!baseVal)
        return "non-existent";

    return getValueFromEnums(&baseVal, 1);
}

//=============================================================================
vogleditor_stateTreeProgramPipelineItem::vogleditor_stateTreeProgramPipelineItem(QString name, GLuint64 handle, vogleditor_stateTreeItem* parent, const vogl_sso_state& state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_handle(handle),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    // TODO : Implement this
}
