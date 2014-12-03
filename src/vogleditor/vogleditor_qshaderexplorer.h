#ifndef VOGLEDITOR_QSHADEREXPLORER_H
#define VOGLEDITOR_QSHADEREXPLORER_H

#include <QWidget>

#include "vogl_core.h"

class vogl_context_snapshot;
class vogl_gl_object_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;

namespace Ui
{
    class vogleditor_QShaderExplorer;
}

class vogleditor_QShaderExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QShaderExplorer(QWidget *parent = 0);
    ~vogleditor_QShaderExplorer();

    void clear();

    uint set_shader_objects(vogl::vector<vogl_context_snapshot *> sharingContexts);

    bool set_active_shader(unsigned long long shaderHandle);

private
slots:
    void on_shaderListbox_currentIndexChanged(int index);

private:
    Ui::vogleditor_QShaderExplorer *ui;
    vogl_gl_object_state_ptr_vec m_objects;

    uint add_shader_objects(vogl_gl_object_state_ptr_vec objects);
};

#endif // VOGLEDITOR_QSHADEREXPLORER_H
