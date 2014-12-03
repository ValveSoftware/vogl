#include "vogleditor_statetreeprogramitem.h"
#include "vogleditor_statetreeshaderitem.h"

//=============================================================================

vogleditor_stateTreeProgramBoolItem::vogleditor_stateTreeProgramBoolItem(QString name, bool (vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state)
    : vogleditor_stateTreeProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    bool val = (state.*func)();
    setValue(val);
}

bool vogleditor_stateTreeProgramBoolItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeProgramBoolItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    bool value = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromBools(&value, 1);
}

//=============================================================================

vogleditor_stateTreeProgramUIntItem::vogleditor_stateTreeProgramUIntItem(QString name, uint (vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state)
    : vogleditor_stateTreeProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    uint val = (state.*func)();
    setValue(val);
}

bool vogleditor_stateTreeProgramUIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeProgramUIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint value = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromUints(&value, 1);
}

//=============================================================================

vogleditor_stateTreeProgramEnumItem::vogleditor_stateTreeProgramEnumItem(QString name, GLenum (vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state)
    : vogleditor_stateTreeProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    GLenum val = (state.*func)();
    setValue(enum_to_string(val));
}

bool vogleditor_stateTreeProgramEnumItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeProgramEnumItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int value = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromEnums(&value, 1);
}

//=============================================================================

vogleditor_stateTreeProgramLogItem::vogleditor_stateTreeProgramLogItem(QString name, const vogl::dynamic_string &(vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state)
    : vogleditor_stateTreeProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    uint val = (state.*func)().size();
    setValue(val);

    if (val > 0)
    {
        vogleditor_stateTreeProgramItem *pProgramItem = static_cast<vogleditor_stateTreeProgramItem *>(this->parent());
        pProgramItem->add_diffable_child(new vogleditor_stateTreeProgramStringItem("Info Log", func, this->parent(), state));
    }
}

bool vogleditor_stateTreeProgramLogItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeProgramLogItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint value = (m_pDiffBaseState->*m_pFunc)().size();
    return getValueFromUints(&value, 1);
}

//=============================================================================

vogleditor_stateTreeProgramStringItem::vogleditor_stateTreeProgramStringItem(QString name, const vogl::dynamic_string &(vogl_program_state::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_program_state &state)
    : vogleditor_stateTreeProgramDiffableItem(name, "", parent, state),
      m_pFunc(func)
{
    m_value = (state.*func)();
    setValue(m_value.c_str());
}

bool vogleditor_stateTreeProgramStringItem::hasChanged() const
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

QString vogleditor_stateTreeProgramStringItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    vogl::dynamic_string value = (m_pDiffBaseState->*m_pFunc)();
    return QString(value.c_str());
}

//=============================================================================

vogleditor_stateTreeProgramAttribItem::vogleditor_stateTreeProgramAttribItem(QString name, vogleditor_stateTreeItem *parent, const vogl_program_attrib_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    QString tmp;
    setValue(tmp.sprintf("Loc: %d, Size: %d, Type: %s", state.m_bound_location, state.m_size, enum_to_string(state.m_type).toStdString().c_str()));
}

bool vogleditor_stateTreeProgramAttribItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }
    else
    {
        return (m_pDiffBaseState->find(*m_pState) == cInvalidIndex);
    }
}

QString vogleditor_stateTreeProgramAttribItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    return "non-existent";
}

//=============================================================================

vogleditor_stateTreeProgramUniformItem::vogleditor_stateTreeProgramUniformItem(QString name, vogleditor_stateTreeItem *parent, const vogl_program_uniform_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    QString tmp;
    setValue(tmp.sprintf("Loc: %d, Size: %d, Type: %s", state.m_base_location, state.m_size, enum_to_string(state.m_type).toStdString().c_str()));
}

bool vogleditor_stateTreeProgramUniformItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
    {
        return false;
    }
    else
    {
        return (m_pDiffBaseState->find(*m_pState) == cInvalidIndex);
    }
}

QString vogleditor_stateTreeProgramUniformItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    return "non-existent";
}

//=============================================================================

vogleditor_stateTreeProgramItem::vogleditor_stateTreeProgramItem(QString name, QString value, vogleditor_stateTreeItem *parentNode, vogl_program_state &state, const vogl_context_info &info)
    : vogleditor_stateTreeItem(name, value, parentNode),
      m_pState(&state)
{
    QString tmp;

    // basic info
    this->appendChild(new vogleditor_stateTreeProgramBoolItem("GL_LINK_STATUS", &vogl_program_state::get_link_status, this, state));
    if (info.supports_extension("GL_ARB_separate_shader_objects"))
        this->appendChild(new vogleditor_stateTreeProgramBoolItem("GL_PROGRAM_SEPARABLE", &vogl_program_state::get_separable, this, state));
    this->appendChild(new vogleditor_stateTreeProgramBoolItem("GL_DELETE_STATUS", &vogl_program_state::get_marked_for_deletion, this, state));
    this->appendChild(new vogleditor_stateTreeProgramBoolItem("GL_VALIDATE_STATUS", &vogl_program_state::get_verify_status, this, state));
    if (info.get_version() >= VOGL_GL_VERSION_3_1)
    {
        this->appendChild(new vogleditor_stateTreeProgramUIntItem("GL_ACTIVE_UNIFORM_BLOCKS", &vogl_program_state::get_num_active_uniform_blocks, this, state));
    }

    // program binary
    this->appendChild(new vogleditor_stateTreeItem("GL_PROGRAM_BINARY_RETRIEVABLE_HINT", "TODO", this));
    this->appendChild(new vogleditor_stateTreeProgramUIntItem("GL_PROGRAM_BINARY_LENGTH", &vogl_program_state::get_program_binary_size, this, state));
    this->appendChild(new vogleditor_stateTreeProgramEnumItem("GL_PROGRAM_BINARY_FORMAT", &vogl_program_state::get_program_binary_format, this, state));
    if (m_pState->get_program_binary().size() > 0)
    {
        this->appendChild(new vogleditor_stateTreeItem("Program Binary", "TODO: open in a new tab", this));
    }

    // info log
    this->appendChild(new vogleditor_stateTreeProgramLogItem("GL_INFO_LOG_LENGTH", &vogl_program_state::get_info_log, this, state));

    // linked shaders
    const vogl_unique_ptr<vogl_program_state> &linked_program = m_pState->get_link_time_snapshot();
    if (linked_program.get())
    {
        uint num_attached_shaders = linked_program->get_shaders().size();
        vogleditor_stateTreeItem *pLinkedShadersNode = new vogleditor_stateTreeItem("Linked Shaders", tmp.sprintf("[%u]", num_attached_shaders), this);
        this->appendChild(pLinkedShadersNode);

        for (uint i = 0; i < num_attached_shaders; i++)
        {
            vogl_shader_state &shader = const_cast<vogl_shader_state &>(linked_program->get_shaders()[i]);
            GLuint64 shaderId = shader.get_snapshot_handle();
            pLinkedShadersNode->appendChild(new vogleditor_stateTreeShaderItem(tmp.sprintf("%" PRIu64, shaderId), enum_to_string(shader.get_shader_type()), pLinkedShadersNode, shader));
        }
    }

    // attached shaders
    uint num_attached_shaders = m_pState->get_shaders().size();
    vogleditor_stateTreeItem *pAttachedShadersNode = new vogleditor_stateTreeItem("GL_ATTACHED_SHADERS", tmp.sprintf("[%u]", num_attached_shaders), this);
    this->appendChild(pAttachedShadersNode);
    for (uint i = 0; i < num_attached_shaders; i++)
    {
        vogl_shader_state &shader = const_cast<vogl_shader_state &>(m_pState->get_shaders()[i]);
        GLuint64 shaderId = shader.get_snapshot_handle();
        pAttachedShadersNode->appendChild(new vogleditor_stateTreeShaderItem(tmp.sprintf("%" PRIu64, shaderId), enum_to_string(shader.get_shader_type()), pAttachedShadersNode, shader));
    }

    // active attribs
    vogleditor_stateTreeItem *pAttribsNode = new vogleditor_stateTreeItem("GL_ACTIVE_ATTRIBUTES", tmp.sprintf("[%u]", m_pState->get_num_active_attribs()), this);
    this->appendChild(pAttribsNode);
    uint num_active_attributes = m_pState->get_attrib_state_vec().size();
    for (uint i = 0; i < num_active_attributes; i++)
    {
        const vogl_program_attrib_state &attrib = m_pState->get_attrib_state_vec()[i];
        vogleditor_stateTreeProgramAttribItem *pItem = new vogleditor_stateTreeProgramAttribItem(tmp.sprintf("%s", attrib.m_name.get_ptr()), pAttribsNode, attrib);
        m_attribItems.push_back(pItem);
        pAttribsNode->appendChild(pItem);
    }

    // uniforms
    vogleditor_stateTreeItem *pUniformsNode = new vogleditor_stateTreeItem("GL_ACTIVE_UNIFORMS", tmp.sprintf("[%u]", m_pState->get_num_active_uniforms()), this);
    this->appendChild(pUniformsNode);
    uint num_uniforms = m_pState->get_uniform_state_vec().size();
    for (uint i = 0; i < num_uniforms; i++)
    {
        const vogl_program_uniform_state &uniform = m_pState->get_uniform_state_vec()[i];
        //      pUniformsNode->appendChild(new vogleditor_stateTreeItem(QString(uniform.m_name.get_ptr()), tmp.sprintf("Loc: %d, Size: %d, Type: %s", uniform.m_base_location, uniform.m_size, enum_to_string(uniform.m_type).toStdString().c_str()), pUniformsNode));
        vogleditor_stateTreeProgramUniformItem *pItem = new vogleditor_stateTreeProgramUniformItem(QString(uniform.m_name.get_ptr()), pUniformsNode, uniform);
        m_uniformItems.push_back(pItem);
        pUniformsNode->appendChild(pItem);
    }

    // uniform blocks
}

void vogleditor_stateTreeProgramItem::add_diffable_child(vogleditor_stateTreeProgramDiffableItem *pItem)
{
    m_diffableItems.push_back(pItem);
    appendChild(pItem);
}
