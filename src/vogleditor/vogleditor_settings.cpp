#include "vogleditor_settings.h"
#include "vogl_common.h"
#include "vogl_file_utils.h"

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

dynamic_string vogleditor_settings::get_settings_path(const char* settingsFilename)
{
    dynamic_string settingsPath;
    const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
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
        if (vogl::file_utils::does_dir_exist(settingsPath.c_str())== false)
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

bool vogleditor_settings::load(const char* settingsFile)
{
    bool bLoaded = false;
    json_document settingsDoc;
    dynamic_string path = get_settings_path(settingsFile);
    if (settingsDoc.deserialize_file(path.c_str()))
    {
        // validate metadata
        json_node* pMetadata = settingsDoc.get_root()->find_child_object("metadata");
        if (pMetadata == NULL)
        {
            return false;
        }

        const json_value& rFormatVersion = pMetadata->find_value("vogleditor_settings_file_format_version");
        if (!rFormatVersion.is_valid() || rFormatVersion.as_uint32() != VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION_1)
        {
            return false;
        }

        m_file_format_version = rFormatVersion.as_uint32(VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION);

        // validate that settings node exists
        json_node* pSettingsNode = settingsDoc.get_root()->find_child_object("settings");
        if (pSettingsNode == NULL)
        {
            return false;
        }

        // if so, consider the file successfully loaded
        bLoaded = true;

        // all settings should be considered optional, if they are not in the file, then the default value is used
        m_settings.trim_large_trace_prompt_size = pSettingsNode->value_as_uint32("trim_large_trace_prompt_size", m_defaults.trim_large_trace_prompt_size);

        m_settings.window_position_left = pSettingsNode->value_as_int("window_position_left", m_defaults.window_position_left);
        m_settings.window_position_top = pSettingsNode->value_as_int("window_position_top", m_defaults.window_position_top);
        m_settings.window_size_width = pSettingsNode->value_as_int("window_size_width", m_defaults.window_size_width);
        m_settings.window_size_height = pSettingsNode->value_as_int("window_size_height", m_defaults.window_size_height);
    }

    return bLoaded;
}

bool vogleditor_settings::save(const char* settingsFile)
{
    json_document settingsDoc;
    json_node& metadata = settingsDoc.get_root()->add_object("metadata");
    metadata.add_key_value("vogleditor_settings_file_format_version", to_hex_string(VOGLEDITOR_SETTINGS_FILE_FORMAT_VERSION));

    // settings
    json_node& settings = settingsDoc.get_root()->add_object("settings");
    settings.add_key_value("trim_large_trace_prompt_size", m_settings.trim_large_trace_prompt_size);

    settings.add_key_value("window_position_left", m_settings.window_position_left);
    settings.add_key_value("window_position_top", m_settings.window_position_top);
    settings.add_key_value("window_size_width", m_settings.window_size_width);
    settings.add_key_value("window_size_height", m_settings.window_size_height);

    dynamic_string path = get_settings_path(settingsFile);
    bool bSavedSuccessfully = settingsDoc.serialize_to_file(path.c_str());
    return bSavedSuccessfully;
}
