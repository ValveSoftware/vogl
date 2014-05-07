#ifndef VOGLEDITOR_QBUFFEREXPLORER_H
#define VOGLEDITOR_QBUFFEREXPLORER_H

#include <QWidget>
#include "vogl_core.h"

class vogl_context_snapshot;
class vogl_gl_object_state;
class vogl_buffer_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;

namespace Ui {
class vogleditor_QBufferExplorer;
}

class vogleditor_QBufferExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QBufferExplorer(QWidget *parent = 0);
    ~vogleditor_QBufferExplorer();

    void clear();

    uint set_buffer_objects(vogl::vector<vogl_context_snapshot*> sharingContexts);

    bool set_active_buffer(unsigned long long bufferHandle);

private slots:
    void on_bufferComboBox_currentIndexChanged(int index);

    void on_formatComboBox_currentIndexChanged(int index);

private:
    Ui::vogleditor_QBufferExplorer *ui;

    struct vogleditor_buffer_format_params
    {
        unsigned char m_component_byte_count;
        unsigned char m_component_count;
        const char* m_string_formatter;
        const char* m_name;
    };

    vogl::vector<vogleditor_buffer_format_params> m_formatParams;
    void append_value(vogl::dynamic_string& rString, void* dataPtr, uint formatParamIndex);

    uint add_buffer_objects(vogl_gl_object_state_ptr_vec objects);
};

#endif // VOGLEDITOR_QBUFFEREXPLORER_H
