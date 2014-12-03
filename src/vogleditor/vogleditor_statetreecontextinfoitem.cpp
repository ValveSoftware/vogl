#include "vogleditor_statetreecontextinfoitem.h"

vogleditor_stateTreeContextInfoBoolItem::vogleditor_stateTreeContextInfoBoolItem(QString name, bool (vogl_context_info::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_context_info &info)
    : vogleditor_stateTreeContextInfoDiffableItem(name, "", parent),
      m_pState(&info),
      m_pFunc(func)
{
    bool val = (info.*func)();
    setValue(getValueFromBools(&val, 1));
}

bool vogleditor_stateTreeContextInfoBoolItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeContextInfoBoolItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    bool val = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromBools(&val, 1);
}

//=============================================================================

vogleditor_stateTreeContextInfoUIntItem::vogleditor_stateTreeContextInfoUIntItem(QString name, uint (vogl_context_info::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_context_info &info)
    : vogleditor_stateTreeContextInfoDiffableItem(name, "", parent),
      m_pState(&info),
      m_pFunc(func)
{
    uint val = (info.*func)();
    setValue(getValueFromUints(&val, 1));
}

bool vogleditor_stateTreeContextInfoUIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;
    else
        return (m_pState->*m_pFunc)() != (m_pDiffBaseState->*m_pFunc)();
}

QString vogleditor_stateTreeContextInfoUIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    uint val = (m_pDiffBaseState->*m_pFunc)();
    return getValueFromUints(&val, 1);
}

//=============================================================================

vogleditor_stateTreeContextInfoStringItem::vogleditor_stateTreeContextInfoStringItem(QString name, const vogl::dynamic_string &(vogl_context_info::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_context_info &info)
    : vogleditor_stateTreeContextInfoDiffableItem(name, "", parent),
      m_pState(&info),
      m_pFunc(func)
{
    m_value = (info.*func)();
    setValue(m_value.c_str());
}

bool vogleditor_stateTreeContextInfoStringItem::hasChanged() const
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

QString vogleditor_stateTreeContextInfoStringItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    vogl::dynamic_string val = (m_pDiffBaseState->*m_pFunc)();
    return QString(val.c_str());
}

//=============================================================================

bool vogleditor_stateTreeContextInfoItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    for (vogleditor_stateTreeContextInfoDiffableItem *const *iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
    {
        if ((*iter)->hasChanged())
        {
            return true;
        }
    }

    return false;
}

vogleditor_stateTreeContextInfoItem::vogleditor_stateTreeContextInfoItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_context_info &info)
    : vogleditor_stateTreeItem(name, value, parent),
      m_pState(&info),
      m_pDiffBaseState(NULL)
{
    QString tmp;

    if (info.is_valid())
    {
        tmp = "valid";
    }
    else
    {
        tmp = "Never made current";
    }

    this->setValue(tmp);

    if (info.is_valid())
    {
        {
            vogleditor_stateTreeContextInfoBoolItem *pItem = new vogleditor_stateTreeContextInfoBoolItem("Forward compatible", &vogl_context_info::is_forward_compatible, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoBoolItem *pItem = new vogleditor_stateTreeContextInfoBoolItem("Core profile", &vogl_context_info::is_core_profile, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoBoolItem *pItem = new vogleditor_stateTreeContextInfoBoolItem("Compatibility Profile", &vogl_context_info::is_compatibility_profile, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoBoolItem *pItem = new vogleditor_stateTreeContextInfoBoolItem("Debug context", &vogl_context_info::is_debug_context, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }

        {
            vogleditor_stateTreeContextInfoStringItem *pItem = new vogleditor_stateTreeContextInfoStringItem("GL_RENDERER", &vogl_context_info::get_renderer_str, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoStringItem *pItem = new vogleditor_stateTreeContextInfoStringItem("GL_VENDOR", &vogl_context_info::get_vendor_str, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoStringItem *pItem = new vogleditor_stateTreeContextInfoStringItem("GL_VERSION", &vogl_context_info::get_version_str, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoStringItem *pItem = new vogleditor_stateTreeContextInfoStringItem("GL_SHADING_LANGUAGE_VERSION", &vogl_context_info::get_glsl_version_str, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }

        vogleditor_stateTreeItem *pExtNode = new vogleditor_stateTreeItem("GL_EXTENSIONS", "", this);
        this->appendChild(pExtNode);
        const dynamic_string_array &extList = info.get_extensions();
        for (uint e = 0; e < extList.size(); e++)
        {
            vogleditor_stateTreeContextInfoExtensionItem *pItem = new vogleditor_stateTreeContextInfoExtensionItem(tmp.sprintf("%d", e), extList[e].c_str(), pExtNode, info);
            m_diffableItems.push_back(pItem);
            pExtNode->appendChild(pItem);
        }
        pExtNode->setValue(tmp.sprintf("[%d]", pExtNode->childCount()));

        {
            vogleditor_stateTreeContextInfoUIntItem *pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_PROGRAM_ENV_PARAMETERS_ARB GL_VERTEX_PROGRAM_ARB", &vogl_context_info::get_max_arb_vertex_program_env_params, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoUIntItem *pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_PROGRAM_ENV_PARAMETERS_ARB GL_FRAGMENT_PROGRAM_ARB", &vogl_context_info::get_max_arb_fragment_program_env_params, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoUIntItem *pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_PROGRAM_MATRICES_ARB", &vogl_context_info::get_max_arb_program_matrices, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoUIntItem *pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_COMBINED_TEXTURE_COORDS", &vogl_context_info::get_max_combined_texture_coords, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }
        {
            vogleditor_stateTreeContextInfoUIntItem *pItem = new vogleditor_stateTreeContextInfoUIntItem("GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS", &vogl_context_info::get_max_transform_feedback_separate_attribs, this, info);
            m_diffableItems.push_back(pItem);
            this->appendChild(pItem);
        }

        //       this->appendChild(new vogleditor_stateTreeItem("GL_MAX_DRAW_BUFFERS", STR_INT(info.get_max_draw_buffers()), this));
        //       this->appendChild(new vogleditor_stateTreeItem("GL_MAX_LIGHTS", STR_INT(info.get_max_lights()), this));
        //       this->appendChild(new vogleditor_stateTreeItem("GL_MAX_TEXTURE_COORDS", STR_INT(info.get_max_texture_coords()), this));
        //       this->appendChild(new vogleditor_stateTreeItem("GL_MAX_TEXTURE_IMAGE_UNITS", STR_INT(info.get_max_texture_image_units()), this));
        //       this->appendChild(new vogleditor_stateTreeItem("GL_MAX_TEXTURE_UNITS", STR_INT(info.get_max_texture_units()), this));
        //       this->appendChild(new vogleditor_stateTreeItem("GL_MAX_UNIFORM_BUFFER_BINDINGS", STR_INT(info.get_max_uniform_buffer_bindings()), this));
        //       this->appendChild(new vogleditor_stateTreeItem("GL_MAX_VERTEX_ATTRIBS", STR_INT(info.get_max_vertex_attribs()), this));
    }
}
