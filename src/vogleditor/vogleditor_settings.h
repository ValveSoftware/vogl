#ifndef VOGLEDITOR_SETTINGS_H
#define VOGLEDITOR_SETTINGS_H

#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include <QString>

class vogleditor_settings;
extern vogleditor_settings g_settings;

struct vogleditor_setting_struct
{
    int window_position_left;
    int window_position_top;
    int window_size_width;
    int window_size_height;
    unsigned int trim_large_trace_prompt_size;

    bool groups_state_render;
    bool groups_push_pop_markers;
    bool groups_nested_calls;
};

class vogleditor_settings
{
public:
    vogleditor_settings();
    virtual ~vogleditor_settings()
    {
    }

    bool load(const char *settingsFile);
    bool save(const char *settingsFile);

    QString to_string();
    bool from_string(const char *settingsStr);

    int window_position_left()
    {
        return m_settings.window_position_left;
    }
    int window_position_top()
    {
        return m_settings.window_position_top;
    }
    int window_size_width()
    {
        return m_settings.window_size_width;
    }
    int window_size_height()
    {
        return m_settings.window_size_height;
    }
    void set_window_position_left(int window_position_left)
    {
        m_settings.window_position_left = window_position_left;
    }
    void set_window_position_top(int window_position_top)
    {
        m_settings.window_position_top = window_position_top;
    }
    void set_window_size_width(int window_size_width)
    {
        m_settings.window_size_width = window_size_width;
    }
    void set_window_size_height(int window_size_height)
    {
        m_settings.window_size_height = window_size_height;
    }

    unsigned int trim_large_trace_prompt_size()
    {
        return m_settings.trim_large_trace_prompt_size;
    }
    void set_trim_large_trace_prompt_size(unsigned int trim_large_trace_prompt_size)
    {
        m_settings.trim_large_trace_prompt_size = trim_large_trace_prompt_size;
    }

    bool groups_state_render()
    {
        return m_settings.groups_state_render;
    }
    bool groups_push_pop_markers()
    {
        return m_settings.groups_push_pop_markers;
    }
    bool groups_nested_calls()
    {
        return m_settings.groups_nested_calls;
    }
    void set_groups_state_render(bool groups_state_render)
    {
        m_settings.groups_state_render = groups_state_render;
    }
    void set_groups_push_pop_markers(bool groups_push_pop_markers)
    {
        m_settings.groups_push_pop_markers = groups_push_pop_markers;
    }
    void set_groups_nested_calls(bool groups_nested_calls)
    {
        m_settings.groups_nested_calls = groups_nested_calls;
    }

private:
    unsigned int m_file_format_version;
    vogleditor_setting_struct m_settings;
    vogleditor_setting_struct m_defaults;

    vogl::dynamic_string get_settings_path(const char *settingsFilename);
    bool to_json(vogl::json_document &doc);
    bool from_json(const vogl::json_document &doc);
};

#endif // VOGLEDITOR_SETTINGS_H
