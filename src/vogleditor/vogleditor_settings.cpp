#include "vogleditor_settings.h"
#include "vogl_common.h"
#include "vogl_file_utils.h"

// declared as extern in header
vogleditor_settings g_settings;

static const unsigned int VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1 = 1;
static const unsigned int VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION = VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1;

vogleditor_settings::vogleditor_settings()
    : m_file_format_version(VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1)
{
    m_defaults.trim_large_trace_prompt_size = 200;
    m_defaults.window_position_left = 0;
    m_defaults.window_position_top = 0;
    m_defaults.window_size_width = 1024;
    m_defaults.window_size_height = 768;

    m_settings = m_defaults;
}

dynamic_string vogleditor_settings::get_settings_path(const char *settingsFilename)
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

    settingsPath += settingsFilename;
    return settingsPath;
}

QString vogleditor_settings::to_string()
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

bool vogleditor_settings::from_json(const json_document &doc)
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

    return true;
}

bool vogleditor_settings::load(const char *settingsFile)
{
    bool bLoaded = false;
    json_document settingsDoc;
    dynamic_string path = get_settings_path(settingsFile);
    if (settingsDoc.deserialize_file(path.c_str()))
    {
        bLoaded = this->from_json(settingsDoc);
    }

    return bLoaded;
}

bool vogleditor_settings::from_string(const char *settingsStr)
{
    bool bResult = false;
    json_document doc;
    if (doc.deserialize(settingsStr))
    {
        bResult = this->from_json(doc);
    }

    return bResult;
}

bool vogleditor_settings::to_json(json_document &doc)
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

    return true;
}

bool vogleditor_settings::save(const char *settingsFile)
{
    json_document settingsDoc;
    if (this->to_json(settingsDoc) == false)
    {
        return false;
    }

    dynamic_string path = get_settings_path(settingsFile);
    bool bSavedSuccessfully = settingsDoc.serialize_to_file(path.c_str());
    return bSavedSuccessfully;
}
