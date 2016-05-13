#ifndef VOGLEDITOR_QSETTINGS_H
#define VOGLEDITOR_QSETTINGS_H

#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include <QObject>
#include <QStringList>
#include <QVector>

class vogleditor_qsettings;
extern vogleditor_qsettings g_settings;

struct vogleditor_setting_struct
{
    int tab_page;

    int window_position_left;
    int window_position_top;
    int window_size_width;
    int window_size_height;
    unsigned int trim_large_trace_prompt_size;

    // State/Render groups checkbox
    QString state_render_name;          // checkbox label
    bool state_render_stat;             // checked status of checkbox
    bool state_render_used;             // enabled status of checkbox
    QStringList state_render_nest_list; // list of allowable nest options

    // Debug marker groups groupbox
    QStringList debug_marker_name_list;   // checkbox labels in groupbox
    QVector<bool> debug_marker_stat_list; // checked status of checkboxes
    QVector<bool> debug_marker_used_list; // enabled status of checkboxes
    // TODO: seems these could also be made into QStringList and QVector<bool>
    QString debug_marker_option_name_labl; // radiobutton labels in groupbox
    QString debug_marker_option_omit_labl;
    bool debug_marker_option_name_stat; // checked status of radiobuttons
    bool debug_marker_option_omit_stat;
    bool debug_marker_option_name_used; // enabled status of radiobuttons
    bool debug_marker_option_omit_used;

    // Nest options groupbox
    QString groupbox_nest_options_name;   // checkbox label of groupbox header
    bool groupbox_nest_options_stat;      // checked status of header checkbox
    bool groupbox_nest_options_used;      // enabled status of header checkbox
    QStringList nest_options_name_list;   // checkbox labels in groupbox
    QVector<bool> nest_options_stat_list; // checked status of checkboxes
    QVector<bool> nest_options_used_list; // enabled status of checkboxes
};

class vogleditor_qsettings : public QObject
{
    Q_OBJECT

public:
    vogleditor_qsettings();
    virtual ~vogleditor_qsettings()
    {
    }

    bool load();
    bool save();

    QString to_string();
    bool from_string(const char *settingsStr);

signals:
    void treeDisplayChanged();

public:
    int tab_page()
    {
        return m_settings.tab_page;
    }
    void set_tab_page(int page)
    {
        m_settings.tab_page = page;
    }

    // Startup
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

    // Groups

    // State/Render
    QString group_state_render_name()
    {
        return m_settings.state_render_name;
    }
    bool group_state_render_stat()
    {
        return m_settings.state_render_stat;
    }
    void set_group_state_render_stat(bool state_render_stat)
    {
        m_settings.state_render_stat = state_render_stat;
    }
    bool group_state_render_used()
    {
        return m_settings.state_render_used;
    }

    // Debug marker
    QStringList group_debug_marker_name_list()
    {
        return m_settings.debug_marker_name_list;
    }

    QVector<bool> group_debug_marker_stat_list()
    {
        return m_settings.debug_marker_stat_list;
    }
    void set_group_debug_marker_stat_list(QVector<bool> debug_marker_stat_list)
    {
        m_settings.debug_marker_stat_list = debug_marker_stat_list;
    }

    QVector<bool> group_debug_marker_used_list()
    {
        return m_settings.debug_marker_used_list;
    }
    void set_group_debug_marker_used_list(QVector<bool> debug_marker_used_list)
    {
        m_settings.debug_marker_used_list = debug_marker_used_list;
    }

    QString group_debug_marker_option_name_label()
    {
        return m_settings.debug_marker_option_name_labl;
    }
    bool group_debug_marker_option_name_stat()
    {
        return m_settings.debug_marker_option_name_stat;
    }
    bool group_debug_marker_option_name_used()
    {
        return m_settings.debug_marker_option_name_used;
    }

    QString group_debug_marker_option_omit_label()
    {
        return m_settings.debug_marker_option_omit_labl;
    }
    bool group_debug_marker_option_omit_stat()
    {
        return m_settings.debug_marker_option_omit_stat;
    }
    bool group_debug_marker_option_omit_used()
    {
        return m_settings.debug_marker_option_omit_used;
    }

    void set_group_debug_marker_option_name_stat(bool stat)
    {
        m_settings.debug_marker_option_name_stat = stat;
    }
    void set_group_debug_marker_option_name_used(bool used)
    {
        m_settings.debug_marker_option_name_used = used;
    }
    void set_group_debug_marker_option_omit_stat(bool stat)
    {
        m_settings.debug_marker_option_omit_stat = stat;
    }
    void set_group_debug_marker_option_omit_used(bool used)
    {
        m_settings.debug_marker_option_omit_used = used;
    }

    bool group_debug_marker_in_use()
    {
        return group_debug_marker_stat_list().contains(true);
    }

    // Nest options

    // Groupbox
    QString groupbox_nest_options_name()
    {
        return m_settings.groupbox_nest_options_name;
    }
    bool groupbox_nest_options_stat()
    {
        return m_settings.groupbox_nest_options_stat;
    }
    void set_groupbox_nest_options_stat(bool b_val)
    {
        m_settings.groupbox_nest_options_stat = b_val;
    }
    bool groupbox_nest_options_used()
    {
        return m_settings.groupbox_nest_options_used;
    }

    // Checkboxes
    QStringList group_nest_options_name_list()
    {
        return m_settings.nest_options_name_list;
    }
    QVector<bool> group_nest_options_stat_list()
    {
        return m_settings.nest_options_stat_list;
    }
    void set_group_nest_options_stat_list(QVector<bool> nest_options_stat_list)
    {
        m_settings.nest_options_stat_list = nest_options_stat_list;
    }
    QVector<bool> group_nest_options_used_list()
    {
        return m_settings.nest_options_used_list;
    }
    void set_group_nest_options_used_list(QVector<bool> nest_options_used_list)
    {
        m_settings.nest_options_used_list = nest_options_used_list;
    }

    void update_group_active_lists()
    {
        m_active_debug_marker_list = active_debug_marker_list();
        m_active_nest_options_list = active_nest_options_list();
        emit treeDisplayChanged();
    }

    bool is_selected_state_render_nest(QString str)
    {
        // Use allowed nest calls with State/Render groups if selected under
        // Nest options (even though nest call item is disabled)
        return m_active_state_render_nest_list.contains(str) && m_settings.state_render_stat && m_active_nest_options_list.contains(str);
    }
    bool is_active_debug_marker(QString str)
    {
        return m_active_debug_marker_list.contains(str);
    }
    bool is_active_nest_options(QString str)
    {
        return m_settings.groupbox_nest_options_stat && m_active_nest_options_list.contains(str);
    }

private:
    const QStringList active_state_render_nest_list() const
    {
        QStringList activeList;
        for (int i = 0; i < m_settings.state_render_nest_list.count(); i++)
        {
            activeList << m_settings.state_render_nest_list[i].split("/");
        }
        return activeList;
    }
    const QStringList active_debug_marker_list() const
    {

        QStringList activeList;
        for (int i = 0; i < m_settings.debug_marker_name_list.count(); i++)
        {
            if (m_settings.debug_marker_stat_list[i])
            {
                activeList << m_settings.debug_marker_name_list[i].split("/");
            }
        }
        return activeList;
    }
    const QStringList active_nest_options_list() const
    {

        QStringList activeList;
        for (int i = 0; i < m_settings.nest_options_name_list.count(); i++)
        {
            if (m_settings.nest_options_stat_list[i])
            {
                activeList << m_settings.nest_options_name_list[i].split("/");
            }
        }
        return activeList;
    }

private:
    unsigned int m_file_format_version;
    vogleditor_setting_struct m_settings;
    vogleditor_setting_struct m_defaults;

    QStringList m_active_state_render_nest_list;
    QStringList m_active_debug_marker_list;
    QStringList m_active_nest_options_list;

    vogl::dynamic_string get_settings_path();
    bool to_json(vogl::json_document &doc);
    bool from_json(const vogl::json_document &doc);
};

#endif // VOGLEDITOR_QSETTINGS_H
