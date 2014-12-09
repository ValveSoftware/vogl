#ifndef VOGLEDITOR_QPROGRAMARBEXPLORER_H
#define VOGLEDITOR_QPROGRAMARBEXPLORER_H

#include <QWidget>

#include "vogl_core.h"

class vogl_context_snapshot;
class vogl_gl_object_state;
class vogl_arb_program_state;
class vogl_arb_program_environment_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;

namespace Ui
{
    class vogleditor_QProgramArbExplorer;
}

class vogleditor_QProgramArbExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QProgramArbExplorer(QWidget *parent = 0);
    ~vogleditor_QProgramArbExplorer();

    void clear();

    uint set_program_objects(vogl::vector<vogl_context_snapshot *> sharingContexts);

    bool set_active_program(unsigned long long programHandle);

private
slots:
    void on_programListBox_currentIndexChanged(int index);

    void on_shaderTextEdit_textChanged();

    void on_saveShaderButton_clicked();

private:
    Ui::vogleditor_QProgramArbExplorer *ui;
    vogl_gl_object_state_ptr_vec m_objects;
    vogl_arb_program_environment_state *m_pProgramEnvState;
    uint m_maxEnvParameters;
    uint m_maxLocalParameters;

    uint add_program_objects(vogl_gl_object_state_ptr_vec objects);
    void update_uniforms_for_source(const QString &programSource);

signals:
    void program_edited(vogl_arb_program_state *pNewProgramState);
};

#endif // VOGLEDITOR_QPROGRAMARBEXPLORER_H
