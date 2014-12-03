#include "vogleditor_statetreeshaderitem.h"
#include "vogl_shader_state.h"

//=============================================================================

vogleditor_stateTreeShaderBoolItem::vogleditor_stateTreeShaderBoolItem(QString name, bool (vogl_shader_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_shader_state &state)
    : vogleditor_stateTreeShaderDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    bool val = (state.*func)();
    setValue(val ? "GL_TRUE" : "GL_FALSE");
}

bool vogleditor_stateTreeShaderBoolItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeShaderBoolItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    bool value = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromBools(&value, 1);
}

//=============================================================================

vogleditor_stateTreeShaderLogItem::vogleditor_stateTreeShaderLogItem(QString name, const vogl::dynamic_string &(vogl_shader_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_shader_state &state)
    : vogleditor_stateTreeShaderDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    uint val = (state.*func)().size();
    setValue(val);

    if (val > 0)
    {
        vogleditor_stateTreeShaderItem *pShaderItem = static_cast<vogleditor_stateTreeShaderItem *>(this->parent());
        pShaderItem->add_diffable_child(new vogleditor_stateTreeShaderStringItem("Info Log", func, this->parent(), state));
    }
}

bool vogleditor_stateTreeShaderLogItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeShaderLogItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint value = (m_pDiffBaseState->*m_pFunc)().size();
    return getValueFromUints(&value, 1);
}

//=============================================================================
vogleditor_stateTreeShaderSourceLengthItem::vogleditor_stateTreeShaderSourceLengthItem(QString name, vogleditor_stateTreeItem *parent, const vogl_shader_state &state)
    : vogleditor_stateTreeShaderDiffableItem(name, "", parent, state)
{
    unsigned int length = state.get_source().size();
    setValue(length);
}

bool vogleditor_stateTreeShaderSourceLengthItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }
    else
    {
        return m_pState->get_source().size() != m_pDiffBaseState->get_source().size();
    }
}

QString vogleditor_stateTreeShaderSourceLengthItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint value = m_pDiffBaseState->get_source().size();
    return getValueFromUints(&value, 1);
}

//=============================================================================

vogleditor_stateTreeShaderStringItem::vogleditor_stateTreeShaderStringItem(QString name, const vogl::dynamic_string &(vogl_shader_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_shader_state &state)
    : vogleditor_stateTreeShaderDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    m_value = (state.*func)();
    setValue(m_value.c_str());
}

bool vogleditor_stateTreeShaderStringItem::hasChanged() const
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

QString vogleditor_stateTreeShaderStringItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    vogl::dynamic_string value = (m_pDiffBaseState->*m_pFunc)();
    return QString(value.c_str());
}

//=============================================================================

vogleditor_stateTreeShaderItem::vogleditor_stateTreeShaderItem(QString name, QString value, vogleditor_stateTreeItem *parentNode, vogl_shader_state &state)
    : vogleditor_stateTreeItem(name, value, parentNode),
      m_pState(&state)
{
    QString tmp;

    {
        vogleditor_stateTreeShaderBoolItem *pItem = new vogleditor_stateTreeShaderBoolItem("GL_DELETE_STATUS", &vogl_shader_state::get_marked_for_deletion, this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }
    {
        vogleditor_stateTreeShaderBoolItem *pItem = new vogleditor_stateTreeShaderBoolItem("GL_COMPILE_STATUS", &vogl_shader_state::get_compile_status, this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }

    {
        vogleditor_stateTreeShaderLogItem *pItem = new vogleditor_stateTreeShaderLogItem("GL_INFO_LOG_LENGTH", &vogl_shader_state::get_info_log, this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }

    {
        vogleditor_stateTreeShaderSourceLengthItem *pItem = new vogleditor_stateTreeShaderSourceLengthItem("GL_SHADER_SOURCE_LENGTH", this, state);
        m_diffableItems.push_back(pItem);
        this->appendChild(pItem);
    }
}

void vogleditor_stateTreeShaderItem::add_diffable_child(vogleditor_stateTreeShaderDiffableItem *pItem)
{
    m_diffableItems.push_back(pItem);
    appendChild(pItem);
}
