#include "vogleditor_qsettings.h"
#include "vogl_common.h"
#include "vogl_file_utils.h"

// declared as extern in header
vogleditor_qsettings g_settings;

static const unsigned int VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1 = 1;
static const unsigned int VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION = VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1;

static const char *g_SETTINGS_FILE = "./vogleditor_settings.json";

vogleditor_qsettings::vogleditor_qsettings()
    : m_file_format_version(VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1)
{
    m_defaults.tab_page = 0;

    m_defaults.trim_large_trace_prompt_size = 200;

    m_defaults.window_position_left = 0;
    m_defaults.window_position_top = 0;
    m_defaults.window_size_width = 1024;
    m_defaults.window_size_height = 768;

    m_defaults.state_render_nest_list
        << "glBegin/glEnd";

    m_defaults.debug_marker_name_list
        << "glPushDebugGroup/glPopDebugGroup"
        << "glPushGroupMarkerEXT/glPopGroupMarkerEXT";

    m_defaults.nest_options_name_list
        << "glBegin/glEnd"
        << "glNewList/glEndList"
        << "glPushMatrix/glPopMatrix"
        << "glPushName/glPopName"
        << "glPushAttrib/glPopAttrib"
        << "glPushClientAttrib/glPopClientAttrib";

    // State/Render
    m_defaults.state_render_name = "State/Render groups";
    m_defaults.state_render_stat = false;
    m_defaults.state_render_used = true;

    // Debug marker
    for (int i = 0, cnt = m_defaults.debug_marker_name_list.count(); i < cnt; i++)
    {
        m_defaults.debug_marker_stat_list << true;
        m_defaults.debug_marker_used_list << true;
    }
    m_defaults.debug_marker_stat_list[1] = false; // glPush/PopGroupMarkerEXT
    m_defaults.debug_marker_used_list[1] = false; // disable

    m_defaults.debug_marker_option_name_labl = "Use text argument as label";
    m_defaults.debug_marker_option_name_stat = false;
    m_defaults.debug_marker_option_name_used = true;

    m_defaults.debug_marker_option_omit_labl = "Hide terminating API call";
    m_defaults.debug_marker_option_omit_stat = false;
    m_defaults.debug_marker_option_omit_used = true;

    // Nest options
    m_defaults.groupbox_nest_options_name = "Nest options";
    m_defaults.groupbox_nest_options_stat = true;
    m_defaults.groupbox_nest_options_used = true;
    for (int i = 0, cnt = m_defaults.nest_options_name_list.count(); i < cnt; i++)
    {
        m_defaults.nest_options_stat_list << false;
        m_defaults.nest_options_used_list << true;
    }
    m_defaults.nest_options_stat_list[0] = true; // glBegin/End

    m_settings = m_defaults;

    m_active_state_render_nest_list = active_state_render_nest_list();

    update_group_active_lists();
}

dynamic_string vogleditor_qsettings::get_settings_path()
{
    dynamic_string settingsPath;
    const char *xdgConfigHome = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdgConfigHome != NULL && strlen(xdgConfigHome) != 0)
    {
        settingsPath = xdgConfigHome;
        settingsPath += "/vogleditor/";
        if (vogl::file_utils::does_dir_exist(settingsPath.c_str()) == false)
        {
            if (vogl::file_utils::create_directories_from_full_path(settingsPath) == false)
            {
                VOGL_ASSERT(!"Failed to create directories for settings file.");
            }
        }
    }
    else if (home != NULL && strlen(home) != 0)
    {
        settingsPath += home;
        settingsPath += "/.config/vogleditor/";
        if (vogl::file_utils::does_dir_exist(settingsPath.c_str()) == false)
        {
            if (vogl::file_utils::create_directories_from_full_path(settingsPath) == false)
            {
                VOGL_ASSERT(!"Failed to create directories for settings file.");
            }
        }
    }
    else
    {
        // the settings file will end up in the current working directory
    }

    settingsPath += g_SETTINGS_FILE;
    return settingsPath;
}

QString vogleditor_qsettings::to_string()
{
    json_document settingsDoc;
    if (this->to_json(settingsDoc) == false)
    {
        return "";
    }

    dynamic_string strSettings;
    settingsDoc.serialize(strSettings);

    QString qstrSettings(strSettings.c_str());
    return qstrSettings;
}

bool vogleditor_qsettings::from_json(const json_document &doc)
{
    // validate metadata
    const json_node *pMetadata = doc.get_root()->find_child_object("metadata");
    if (pMetadata == NULL)
    {
        return false;
    }

    const json_value &rFormatVersion = pMetadata->find_value("vogleditor_settings_file_format_version");
    if (!rFormatVersion.is_valid() || rFormatVersion.as_uint32() != VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1)
    {
        return false;
    }

    m_file_format_version = rFormatVersion.as_uint32(VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION);

    // validate that settings node exists
    const json_node *pSettingsNode = doc.get_root()->find_child_object("settings");
    if (pSettingsNode == NULL)
    {
        return false;
    }

    // all settings should be considered optional, if they are not in the json, then the current value is used (thus the value remains unchanged)
    m_settings.trim_large_trace_prompt_size = pSettingsNode->value_as_uint32("trim_large_trace_prompt_size", m_settings.trim_large_trace_prompt_size);

    m_settings.window_position_left = pSettingsNode->value_as_int("window_position_left", m_settings.window_position_left);
    m_settings.window_position_top = pSettingsNode->value_as_int("window_position_top", m_settings.window_position_top);
    m_settings.window_size_width = pSettingsNode->value_as_int("window_size_width", m_settings.window_size_width);
    m_settings.window_size_height = pSettingsNode->value_as_int("window_size_height", m_settings.window_size_height);

    // groups
    const json_node *pGroupsNode = doc.get_root()->find_child_object("groups");
    if (pGroupsNode == NULL)
    {
        return false;
    }

    // State/Render
    QByteArray pKey = m_settings.state_render_name.toLocal8Bit();
    m_settings.state_render_stat = pGroupsNode->value_as_bool(pKey.data(), m_settings.state_render_stat);

    // Debug marker list
    for (int i = 0, cnt = m_settings.debug_marker_name_list.count(); i < cnt; i++)
    {
        QByteArray pKey = m_settings.debug_marker_name_list[i].toLocal8Bit();
        m_settings.debug_marker_stat_list[i] = pGroupsNode->value_as_bool(pKey.data(), m_settings.debug_marker_stat_list[i]);
    }
    // Debug marker options
    pKey = m_settings.debug_marker_option_name_labl.toLocal8Bit();
    m_settings.debug_marker_option_name_stat = pGroupsNode->value_as_bool(pKey.data(), m_settings.debug_marker_option_name_stat);

    pKey = m_settings.debug_marker_option_omit_labl.toLocal8Bit();
    m_settings.debug_marker_option_omit_stat = pGroupsNode->value_as_bool(pKey.data(), m_settings.debug_marker_option_omit_stat);

    // Nest options list
    pKey = m_settings.groupbox_nest_options_name.toLocal8Bit();
    m_settings.groupbox_nest_options_stat = pGroupsNode->value_as_bool(pKey.data(), m_settings.groupbox_nest_options_stat);

    for (int i = 0, cnt = m_settings.nest_options_name_list.count(); i < cnt; i++)
    {
        QByteArray pKey = m_settings.nest_options_name_list[i].toLocal8Bit();
        m_settings.nest_options_stat_list[i] = pGroupsNode->value_as_bool(pKey.data(), m_settings.nest_options_stat_list[i]);
    }

    return true;
}

bool vogleditor_qsettings::load()
{
    bool bLoaded = false;
    json_document settingsDoc;
    dynamic_string path = get_settings_path();
    if (settingsDoc.deserialize_file(path.c_str()))
    {
        bLoaded = this->from_json(settingsDoc);
        if (bLoaded)
        {
            update_group_active_lists();
        }
    }

    return bLoaded;
}

bool vogleditor_qsettings::from_string(const char *settingsStr)
{
    bool bResult = false;
    json_document doc;
    if (doc.deserialize(settingsStr))
    {
        bResult = this->from_json(doc);
    }

    return bResult;
}

bool vogleditor_qsettings::to_json(json_document &doc)
{
    json_node &metadata = doc.get_root()->add_object("metadata");
    metadata.add_key_value("vogleditor_settings_file_format_version", to_hex_string(VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION));

    // settings
    json_node &settings = doc.get_root()->add_object("settings");
    settings.add_key_value("trim_large_trace_prompt_size", m_settings.trim_large_trace_prompt_size);

    settings.add_key_value("window_position_left", m_settings.window_position_left);
    settings.add_key_value("window_position_top", m_settings.window_position_top);
    settings.add_key_value("window_size_width", m_settings.window_size_width);
    settings.add_key_value("window_size_height", m_settings.window_size_height);

    // groups
    json_node &groups = doc.get_root()->add_object("groups");

    // State/Render
    QByteArray pKey = m_settings.state_render_name.toLocal8Bit();
    groups.add_key_value(pKey.data(), m_settings.state_render_stat);

    // Debug marker list
    for (int i = 0, cnt = m_settings.debug_marker_name_list.count(); i < cnt; i++)
    {
        const QByteArray pKey = m_settings.debug_marker_name_list[i].toLocal8Bit();
        groups.add_key_value(pKey.data(), m_settings.debug_marker_stat_list[i]);
    }
    // Debug marker options
    pKey = m_settings.debug_marker_option_name_labl.toLocal8Bit();
    groups.add_key_value(pKey.data(), m_settings.debug_marker_option_name_stat);

    pKey = m_settings.debug_marker_option_omit_labl.toLocal8Bit();
    groups.add_key_value(pKey.data(), m_settings.debug_marker_option_omit_stat);

    // Nest options list
    pKey = m_settings.groupbox_nest_options_name.toLocal8Bit();
    groups.add_key_value(pKey.data(), m_settings.groupbox_nest_options_stat);

    for (int i = 0, cnt = m_settings.nest_options_name_list.count(); i < cnt; i++)
    {
        const QByteArray pKey = m_settings.nest_options_name_list[i].toLocal8Bit();
        groups.add_key_value(pKey.data(), m_settings.nest_options_stat_list[i]);
    }

    return true;
}

bool vogleditor_qsettings::save()
{
    json_document settingsDoc;
    if (this->to_json(settingsDoc) == false)
    {
        return false;
    }

    dynamic_string path = get_settings_path();
    bool bSavedSuccessfully = settingsDoc.serialize_to_file(path.c_str());
    return bSavedSuccessfully;
}
