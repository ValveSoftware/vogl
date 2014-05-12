#ifndef VOGLEDITOR_QVERTEXARRAYEXPLORER_H
#define VOGLEDITOR_QVERTEXARRAYEXPLORER_H

#include <QWidget>
#include "vogl_core.h"
class vogl_context_snapshot;
class vogl_gl_object_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;


namespace Ui {
class vogleditor_QVertexArrayExplorer;
}

class vogleditor_QVertexArrayExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QVertexArrayExplorer(QWidget *parent = 0);
    ~vogleditor_QVertexArrayExplorer();

    void clear();

    uint set_vertexarray_objects(vogl_context_snapshot* pContext, vogl::vector<vogl_context_snapshot*> sharingContexts);

    bool set_active_vertexarray(unsigned long long vertexArrayHandle);

private slots:
    void on_vertexArrayComboBox_currentIndexChanged(int index);

    void on_indexTypeComboBox_currentIndexChanged(int index);

private:
    Ui::vogleditor_QVertexArrayExplorer *ui;
    vogl::vector<vogl_context_snapshot*> m_sharing_contexts;

};

#endif // VOGLEDITOR_QVERTEXARRAYEXPLORER_H
