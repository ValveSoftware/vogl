#ifndef VOGLEDITOR_QPROGRAMEXPLORER_H
#define VOGLEDITOR_QPROGRAMEXPLORER_H

#include <QWidget>

#include "vogl_core.h"

class vogl_gl_object_state;
class vogl_program_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;

namespace Ui {
class vogleditor_QProgramExplorer;
}

class vogleditor_QProgramExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QProgramExplorer(QWidget *parent = 0);
    ~vogleditor_QProgramExplorer();

    void clear();

    void set_program_objects(vogl_gl_object_state_ptr_vec objects);

    bool set_active_program(unsigned long long programHandle);

private slots:
    void on_programListBox_currentIndexChanged(int index);

    void on_shaderListBox_currentIndexChanged(int index);

    void on_shaderTextEdit_textChanged();

    void on_saveShaderButton_clicked();

private:
    Ui::vogleditor_QProgramExplorer *ui;
    vogl_gl_object_state_ptr_vec m_objects;

signals:
    void program_edited(vogl_program_state* pNewProgramState);

};

#endif // VOGLEDITOR_QPROGRAMEXPLORER_H
