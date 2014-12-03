#include "vogleditor_statetreerenderbufferitem.h"

#include "vogl_renderbuffer_state.h"

vogleditor_stateTreeRenderbufferIntItem::vogleditor_stateTreeRenderbufferIntItem(QString name, GLenum enumName, vogleditor_stateTreeItem *parent, const vogl_renderbuffer_state &state)
    : vogleditor_stateTreeRenderbufferDiffableItem(name, "", parent),
      m_enum(enumName),
      m_pState(&state)
{
    int val = 0;
    if (m_pState->get_desc().get_int(m_enum, &val))
    {
        setValue(val);
    }
}

bool vogleditor_stateTreeRenderbufferIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    int curVal = 0;
    int baseVal = 0;

    if (m_pState->get_desc().get_int(m_enum, &curVal) == false)
    {
        return true;
    }

    if (m_pDiffBaseState->get_desc().get_int(m_enum, &baseVal) == false)
    {
        return true;
    }

    return (curVal != baseVal);
}

QString vogleditor_stateTreeRenderbufferIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int baseVal = 0;
    if (m_pDiffBaseState->get_desc().get_int(m_enum, &baseVal) == false)
    {
        return "non-existent";
    }

    return getValueFromInts(&baseVal, 1);
}

//=============================================================================

vogleditor_stateTreeRenderbufferEnumItem::vogleditor_stateTreeRenderbufferEnumItem(QString name, GLenum enumName, vogleditor_stateTreeItem *parent, const vogl_renderbuffer_state &state)
    : vogleditor_stateTreeRenderbufferDiffableItem(name, "", parent),
      m_enum(enumName),
      m_pState(&state)
{
    int val = 0;
    if (m_pState->get_desc().get_int(m_enum, &val))
    {
        setValue(enum_to_string(val));
    }
}

bool vogleditor_stateTreeRenderbufferEnumItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    int curVal = 0;
    int baseVal = 0;

    if (m_pState->get_desc().get_int(m_enum, &curVal) == false)
    {
        return true;
    }

    if (m_pDiffBaseState->get_desc().get_int(m_enum, &baseVal) == false)
    {
        return true;
    }

    return (curVal != baseVal);
}

QString vogleditor_stateTreeRenderbufferEnumItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int baseVal = 0;
    if (m_pDiffBaseState->get_desc().get_int(m_enum, &baseVal) == false)
    {
        return "non-existent";
    }

    return getValueFromEnums(&baseVal, 1);
}

//=============================================================================

vogleditor_stateTreeRenderbufferItem::vogleditor_stateTreeRenderbufferItem(QString name, GLuint64 handle, vogleditor_stateTreeItem *parent, const vogl_renderbuffer_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_handle(handle),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    const vogl_renderbuffer_desc &rDesc = m_pState->get_desc();
    int val = 0;
#define GET_INT(name)                                                                                                           \
    if (rDesc.get_int(name, &val, 1))                                                                                           \
    {                                                                                                                           \
        vogleditor_stateTreeRenderbufferIntItem *pItem = new vogleditor_stateTreeRenderbufferIntItem(#name, name, this, state); \
        this->appendChild(pItem);                                                                                               \
        m_diffableItems.push_back(pItem);                                                                                       \
    }
#define GET_ENUM(name)                                                                                                            \
    if (rDesc.get_int(name, &val, 1))                                                                                             \
    {                                                                                                                             \
        vogleditor_stateTreeRenderbufferEnumItem *pItem = new vogleditor_stateTreeRenderbufferEnumItem(#name, name, this, state); \
        this->appendChild(pItem);                                                                                                 \
        m_diffableItems.push_back(pItem);                                                                                         \
    }
    GET_INT(GL_RENDERBUFFER_WIDTH);
    GET_INT(GL_RENDERBUFFER_HEIGHT);
    GET_INT(GL_RENDERBUFFER_SAMPLES);
    GET_ENUM(GL_RENDERBUFFER_INTERNAL_FORMAT);
    GET_INT(GL_RENDERBUFFER_RED_SIZE);
    GET_INT(GL_RENDERBUFFER_GREEN_SIZE);
    GET_INT(GL_RENDERBUFFER_BLUE_SIZE);
    GET_INT(GL_RENDERBUFFER_ALPHA_SIZE);
    GET_INT(GL_RENDERBUFFER_DEPTH_SIZE);
    GET_INT(GL_RENDERBUFFER_STENCIL_SIZE);
#undef GET_INT
#undef GET_ENUM

    //        { vogleditor_stateTreeContextInfoUIntItem* pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_PROGRAM_ENV_PARAMETERS_ARB GL_VERTEX_PROGRAM_ARB", &vogl_context_info::get_max_arb_vertex_program_env_params, this, info); m_diffableItems.push_back(pItem); this->appendChild(pItem); }
    //        { vogleditor_stateTreeContextInfoUIntItem* pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_PROGRAM_ENV_PARAMETERS_ARB GL_FRAGMENT_PROGRAM_ARB", &vogl_context_info::get_max_arb_fragment_program_env_params, this, info); m_diffableItems.push_back(pItem); this->appendChild(pItem); }
    //        { vogleditor_stateTreeContextInfoUIntItem* pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_PROGRAM_MATRICES_ARB", &vogl_context_info::get_max_arb_program_matrices, this, info); m_diffableItems.push_back(pItem); this->appendChild(pItem); }
    //        { vogleditor_stateTreeContextInfoUIntItem* pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_COMBINED_TEXTURE_COORDS", &vogl_context_info::get_max_combined_texture_coords, this, info); m_diffableItems.push_back(pItem); this->appendChild(pItem); }
    //        { vogleditor_stateTreeContextInfoUIntItem* pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS", &vogl_context_info::get_max_transform_feedback_separate_attribs, this, info); m_diffableItems.push_back(pItem); this->appendChild(pItem); }
}
