#ifndef VOGLEDITOR_QPROGRAMEXPLORER_H
#define VOGLEDITOR_QPROGRAMEXPLORER_H

#include <QWidget>

#include "vogl_core.h"

class vogl_context_snapshot;
class vogl_gl_object_state;
class vogl_program_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;

namespace Ui
{
    class vogleditor_QProgramExplorer;
}

class vogleditor_QProgramExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QProgramExplorer(QWidget *parent = 0);
    ~vogleditor_QProgramExplorer();

    void clear();

    uint set_program_objects(vogl::vector<vogl_context_snapshot *> sharingContexts);

    bool set_active_program(unsigned long long programHandle);

private
slots:
    void on_programListBox_currentIndexChanged(int index);

    void on_shaderListBox_currentIndexChanged(int index);

    void on_shaderTextEdit_textChanged();

    void on_saveShaderButton_clicked();

private:
    enum uniform_table_columns
    {
        vogleditor_utc_location = 0,
        vogleditor_utc_name,
        vogleditor_utc_value,
        vogleditor_utc_type
    };

    Ui::vogleditor_QProgramExplorer *ui;
    vogl_gl_object_state_ptr_vec m_objects;
    vogl::vector<vogl_context_snapshot *> m_sharing_contexts;

    uint add_program_objects(vogl_gl_object_state_ptr_vec objects);

    void update_uniforms_for_program(vogl_program_state *pProgramState);

signals:
    void program_edited(vogl_program_state *pProgramState);
};

#endif // VOGLEDITOR_QPROGRAMEXPLORER_H
