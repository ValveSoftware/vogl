#ifndef VOGLEDITOR_QVERTEXARRAYEXPLORER_H
#define VOGLEDITOR_QVERTEXARRAYEXPLORER_H

#include <QWidget>
#include "vogl_core.h"
#include "gl_types.h"
#include "vogleditor_output.h"
#include <QVector3D>

class vogl_buffer_state;
class vogl_context_snapshot;
class vogl_vao_state;
class vogl_gl_object_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;
struct vogl_vertex_attrib_desc;

class vogleditor_QVertexVisualizer;

namespace Ui
{
    class vogleditor_QVertexArrayExplorer;
}

class vogleditor_QVertexArrayExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QVertexArrayExplorer(QWidget *parent = 0);
    ~vogleditor_QVertexArrayExplorer();

    void clear();

    uint set_vertexarray_objects(vogl_context_snapshot *pContext, vogl::vector<vogl_context_snapshot *> sharingContexts);

    bool set_active_vertexarray(unsigned long long vertexArrayHandle);

    // This is intended to only by called by an external object to set the UI.
    // Since these values are (ideally) set based on the current API call, we should not automatically update the UI after these are set.
    // However, the user may manually change the settings in the UI; that's fine.
    void set_element_array_options(uint32_t count, GLenum type, uint32_t byteOffset, int32_t baseVertex, vogl::uint8_vec *pClientSideElements, uint32_t instanceCount, uint32_t baseInstance, GLenum drawMode);

    void beginUpdate()
    {
        m_numUiUpdatesInProgress++;
    }
    void endUpdate(bool bAllowUpdate = false)
    {
        m_numUiUpdatesInProgress--;
        if (is_ui_update_in_progress() == false && bAllowUpdate)
        {
            update_vertex_array_table();
            update_instance_array_table();
        }
    }

private
slots:
    void on_vertexArrayComboBox_currentIndexChanged(int index);

    void on_elementTypeComboBox_currentIndexChanged(int index);

    void on_countSpinBox_valueChanged(int arg1);

    void on_byteOffsetSpinBox_valueChanged(int arg1);

    void on_baseVertexSpinBox_valueChanged(int arg1);

    void on_instanceCountSpinBox_valueChanged(int arg1);

    void on_baseInstanceSpinBox_valueChanged(int arg1);

private:
    Ui::vogleditor_QVertexArrayExplorer *ui;
    vogl_vao_state *m_pVaoState;
    vogl::vector<vogl_context_snapshot *> m_sharing_contexts;
    int m_numUiUpdatesInProgress;
    vogl::vector<vogl_buffer_state *> m_attrib_buffers;
    const vogl::uint8_vec *m_pVaoElementArray;

    // these members store the values set using set_element_array_options()
    bool m_bUserSetOptions;
    uint32_t m_currentCallElementCount;
    int m_currentCallElementTypeIndex;
    uint32_t m_currentCallElementByteOffset;
    int32_t m_currentCallElementBaseVertex;
    const vogl::uint8_vec *m_currentCallElementIndices;
    uint32_t m_currentCallInstanceCount;
    uint32_t m_currentCallBaseInstance;
    GLenum m_currentCallDrawMode;

    void auto_update_element_count();

    bool is_ui_update_in_progress()
    {
        return m_numUiUpdatesInProgress > 0;
    }

    void update_array_table_headers();
    void update_vertex_array_table();
    void update_instance_array_table();
    void update_vertex_array_visualizations();

    bool calculate_element_and_format_as_string(const vogl::uint8_vec *pElementArray, uint32_t index, int typeIndex, int byteOffset, int baseVertex, uint32_t &elementValue, QString &elementString);
    QString format_buffer_data_as_string(uint32_t index, const vogl_buffer_state &bufferState, const vogl_vertex_attrib_desc &attribDesc);

    vogl::vector<vogleditor_QVertexVisualizer*> m_vertexVisualizers;
    QVector3D buffer_data_to_QVector3D(uint32_t index, const vogl_buffer_state &bufferState, const vogl_vertex_attrib_desc &attribDesc);
};

#endif // VOGLEDITOR_QVERTEXARRAYEXPLORER_H
