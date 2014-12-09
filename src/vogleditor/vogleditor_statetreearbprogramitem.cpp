#include "vogleditor_statetreearbprogramitem.h"
#include "vogl_arb_program_state.h"

//=============================================================================

vogleditor_stateTreeArbProgramBoolItem::vogleditor_stateTreeArbProgramBoolItem(QString name, bool (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state)
    : vogleditor_stateTreeArbProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    bool val = (state.*func)();
    setValue(getValueFromBools(&val, 1));
}

bool vogleditor_stateTreeArbProgramBoolItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeArbProgramBoolItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    bool val = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromBools(&val, 1);
}

//=============================================================================

vogleditor_stateTreeArbProgramUIntItem::vogleditor_stateTreeArbProgramUIntItem(QString name, uint (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state)
    : vogleditor_stateTreeArbProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    uint val = (state.*func)();
    setValue(getValueFromUints(&val, 1));
}

bool vogleditor_stateTreeArbProgramUIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeArbProgramUIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint val = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromUints(&val, 1);
}

//=============================================================================

vogleditor_stateTreeArbProgramIntItem::vogleditor_stateTreeArbProgramIntItem(QString name, GLint (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state)
    : vogleditor_stateTreeArbProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    GLint val = (state.*func)();
    setValue(getValueFromInts(&val, 1));
}

bool vogleditor_stateTreeArbProgramIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeArbProgramIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int val = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromInts(&val, 1);
}

//=============================================================================

vogleditor_stateTreeArbProgramEnumItem::vogleditor_stateTreeArbProgramEnumItem(QString name, GLenum (vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state)
    : vogleditor_stateTreeArbProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    GLenum val = (state.*func)();
    setValue(enum_to_string(val));
}

bool vogleditor_stateTreeArbProgramEnumItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeArbProgramEnumItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int val = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromEnums(&val, 1);
}

//=============================================================================

vogleditor_stateTreeArbProgramStringItem::vogleditor_stateTreeArbProgramStringItem(QString name, const uint8_vec &(vogl_arb_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state)
    : vogleditor_stateTreeArbProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    const uint8_vec &value = (state.*func)();
    setValue((const char *)value.get_const_ptr());
}

bool vogleditor_stateTreeArbProgramStringItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }
    else
    {
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
    }
}

QString vogleditor_stateTreeArbProgramStringItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    const uint8_vec &value = (m_pDiffBaseState->*m_pFunc)();
    return QString((const char *)value.get_const_ptr());
}

//=============================================================================

vogleditor_stateTreeArbProgramParamItem::vogleditor_stateTreeArbProgramParamItem(QString name, unsigned int paramIndex, vogleditor_stateTreeItem *parent, const vogl_arb_program_state &state)
    : vogleditor_stateTreeArbProgramDiffableItem(name, "", parent, state),
      m_paramIndex(paramIndex)
{
    QString tmp;
    const vec4F_vec &params = m_pState->get_program_local_params();
    setValue(tmp.sprintf("%f, %f, %f, %f", params[paramIndex][0], params[paramIndex][1], params[paramIndex][2], params[paramIndex][3]));
}

bool vogleditor_stateTreeArbProgramParamItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    const vec4F_vec &curParams = m_pState->get_program_local_params();
    if (m_paramIndex >= curParams.size())
    {
        // this should be an impossible case to get in.
        return true;
    }

    const vec4F_vec &baseParams = m_pDiffBaseState->get_program_local_params();
    if (m_paramIndex >= baseParams.size())
    {
        // this could be possible
        return true;
    }

    const vec4F &baseParam = baseParams[m_paramIndex];
    const vec4F &curParam = curParams[m_paramIndex];

    return (curParam[0] != baseParam[0] ||
            curParam[1] != baseParam[1] ||
            curParam[2] != baseParam[2] ||
            curParam[3] != baseParam[3]);
}

QString vogleditor_stateTreeArbProgramParamItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    const vec4F_vec &baseParams = m_pDiffBaseState->get_program_local_params();
    if (m_paramIndex >= baseParams.size())
    {
        // this could be possible
        return "non-existent";
    }

    const vec4F &baseParam = baseParams[m_paramIndex];
    return getValueFromFloats(baseParam.get_ptr(), 1);
}

//=============================================================================

vogleditor_stateTreeArbProgramItem::vogleditor_stateTreeArbProgramItem(QString name, QString value, vogleditor_stateTreeItem *parentNode, vogl_arb_program_state &state)
    : vogleditor_stateTreeItem(name, value, parentNode),
      m_pState(&state)
{
    QString tmp;

    setValue(tmp.sprintf("%d instructions", state.get_num_instructions()));

    {
        vogleditor_stateTreeArbProgramEnumItem *pItem = new vogleditor_stateTreeArbProgramEnumItem("GL_PROGRAM_FORMAT_ARB", &vogl_arb_program_state::get_program_format, this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }
    {
        vogleditor_stateTreeArbProgramIntItem *pItem = new vogleditor_stateTreeArbProgramIntItem("GL_PROGRAM_INSTRUCTIONS_ARB", &vogl_arb_program_state::get_num_instructions, this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }
    {
        vogleditor_stateTreeArbProgramBoolItem *pItem = new vogleditor_stateTreeArbProgramBoolItem("GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB", &vogl_arb_program_state::is_native, this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }
    {
        vogleditor_stateTreeArbProgramUIntItem *pItem = new vogleditor_stateTreeArbProgramUIntItem("GL_PROGRAM_LENGTH_ARB", &vogl_arb_program_state::get_program_string_size, this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }

    //   if (m_pState->get_program_string_size() > 0)
    //   {
    //      this->appendChild(new vogleditor_stateTreeArbProgramStringItem("GL_PROGRAM_STRING_ARB", &vogl_arb_program_state::get_program_string, this, state));
    //   }

    const vec4F_vec &params = m_pState->get_program_local_params();
    vogleditor_stateTreeItem *pParamsNode = new vogleditor_stateTreeItem("GL_PROGRAM_PARAMETERS_ARB", tmp.sprintf("[%d]", params.size()), this);
    this->appendChild(pParamsNode);

    for (uint i = 0; i < params.size(); i++)
    {
        vogleditor_stateTreeArbProgramParamItem *pItem = new vogleditor_stateTreeArbProgramParamItem(tmp.sprintf("%u", i), i, pParamsNode, state);
        m_diffableItems.push_back(pItem);
        pParamsNode->appendChild(pItem);
    }
}
