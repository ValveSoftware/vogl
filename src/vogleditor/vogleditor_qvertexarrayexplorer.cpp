/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

#include <QVBoxLayout>

#include "vogl_warnings.h"
#include "vogleditor_qvertexarrayexplorer.h"

GCC_DIAGNOSTIC_PUSH()
GCC_DIAGNOSTIC_IGNORED(packed)
#include "ui_vogleditor_qvertexarrayexplorer.h"
GCC_DIAGNOSTIC_POP()

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_vao_state.h"

#include "vogleditor_qvertexvisualizer.h"

Q_DECLARE_METATYPE(vogl_vao_state *);

vogleditor_QVertexArrayExplorer::vogleditor_QVertexArrayExplorer(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::vogleditor_QVertexArrayExplorer),
      m_pVaoState(NULL),
      m_numUiUpdatesInProgress(0),
      m_pVaoElementArray(NULL),
      m_bUserSetOptions(false),
      m_currentCallElementCount(0),
      m_currentCallElementTypeIndex(0),
      m_currentCallElementByteOffset(0),
      m_currentCallElementBaseVertex(0),
      m_currentCallElementIndices(NULL),
      m_currentCallInstanceCount(0),
      m_currentCallBaseInstance(0)
{
    ui->setupUi(this);

    beginUpdate();
    ui->elementTypeComboBox->addItem("GL_UNSIGNED_BYTE");
    ui->elementTypeComboBox->addItem("GL_UNSIGNED_SHORT");
    ui->elementTypeComboBox->addItem("GL_UNSIGNED_INT");
    endUpdate();

    ui->baseVertexSpinBox->setMinimum(INT_MIN);
    ui->baseVertexSpinBox->setMaximum(INT_MAX);

    ui->countSpinBox->setMinimum(0);
    ui->countSpinBox->setMaximum(INT_MAX);

    ui->byteOffsetSpinBox->setMinimum(0);
    ui->byteOffsetSpinBox->setMaximum(INT_MAX);

    ui->instanceCountSpinBox->setMinimum(0);
    ui->instanceCountSpinBox->setMaximum(INT_MAX);

    ui->baseInstanceSpinBox->setMinimum(0);
    ui->baseInstanceSpinBox->setMaximum(INT_MAX);

}

vogleditor_QVertexArrayExplorer::~vogleditor_QVertexArrayExplorer()
{
    delete ui;
}

void vogleditor_QVertexArrayExplorer::clear()
{
    m_sharing_contexts.clear();
    m_attrib_buffers.clear();
    m_pVaoState = NULL;
    m_pVaoElementArray = NULL;

    m_bUserSetOptions = false;
    m_currentCallElementCount = 0;
    m_currentCallElementTypeIndex = 0;
    m_currentCallElementByteOffset = 0;
    m_currentCallElementBaseVertex = 0;
    m_currentCallElementIndices = NULL;
    m_currentCallInstanceCount = 0;
    m_currentCallBaseInstance = 0;

    ui->vertexArrayComboBox->clear();
    ui->vertexTableWidget->clear();
}

uint vogleditor_QVertexArrayExplorer::set_vertexarray_objects(vogl_context_snapshot *pContext, vogl::vector<vogl_context_snapshot *> sharingContexts)
{
    m_sharing_contexts.clear();
    m_attrib_buffers.clear();
    m_pVaoState = NULL;
    m_pVaoElementArray = NULL;

    ui->vertexArrayComboBox->clear();
    ui->vertexTableWidget->clear();

    m_sharing_contexts = sharingContexts;

    uint32_t vaoCount = 0;
    vogl_gl_object_state_ptr_vec vaoObjects;
    pContext->get_all_objects_of_category(cGLSTVertexArray, vaoObjects);
    vaoCount = vaoObjects.size();

    GLuint64 vertexArrayBinding = pContext->get_general_state().get_value<GLuint64>(GL_VERTEX_ARRAY_BINDING);

    for (vogl_gl_object_state_ptr_vec::iterator iter = vaoObjects.begin(); iter != vaoObjects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTVertexArray)
        {
            vogl_vao_state *pState = static_cast<vogl_vao_state *>(*iter);

            QString valueStr;

            uint numAttribs = 0;

            for (uint i = 0; i < pState->get_vertex_attrib_count(); i++)
            {
                if (pState->get_vertex_attrib_desc(i).m_array_binding != 0 && pState->get_vertex_attrib_desc(i).m_enabled == true)
                {
                    numAttribs++;
                }
            }

            if (vertexArrayBinding == pState->get_snapshot_handle())
            {
                valueStr = valueStr.sprintf("VertexArray %" PRIu64 " (current) - %u active attributes", pState->get_snapshot_handle(), numAttribs);
            }
            else
            {
                valueStr = valueStr.sprintf("VertexArray %" PRIu64 " - %u active attributes", pState->get_snapshot_handle(), numAttribs);
            }

            ui->vertexArrayComboBox->addItem(valueStr, QVariant::fromValue(pState));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in vogleditor_QVertexArrayExplorer");
        }
    }

    return vaoCount;
}

bool vogleditor_QVertexArrayExplorer::set_active_vertexarray(unsigned long long vertexArrayHandle)
{
    for (int i = 0; i < ui->vertexArrayComboBox->count(); i++)
    {
        vogl_vao_state *pState = ui->vertexArrayComboBox->itemData(i).value<vogl_vao_state *>();
        if (pState->get_snapshot_handle() == vertexArrayHandle)
        {
            ui->vertexArrayComboBox->setCurrentIndex(i);
            return true;
        }
    }

    return false;
}

void vogleditor_QVertexArrayExplorer::set_element_array_options(uint32_t count, GLenum type, uint32_t byteOffset, int32_t baseVertex, vogl::uint8_vec *pClientSideElements, uint32_t instanceCount, uint32_t baseInstance, GLenum drawMode)
{
    // determine type index in combo box, or use whatever the current setting is if a known type is supplied
    uint32_t typeIndex = ui->elementTypeComboBox->currentIndex();
    if (type == GL_UNSIGNED_BYTE)
    {
        typeIndex = 0;
    }
    else if (type == GL_UNSIGNED_SHORT)
    {
        typeIndex = 1;
    }
    else if (type == GL_UNSIGNED_INT)
    {
        typeIndex = 2;
    }

    m_currentCallElementTypeIndex = typeIndex;
    m_currentCallElementCount = count;
    m_currentCallElementBaseVertex = baseVertex;
    m_currentCallElementIndices = pClientSideElements;
    m_currentCallElementByteOffset = byteOffset;
    m_currentCallInstanceCount = instanceCount;
    m_currentCallBaseInstance = baseInstance;
    m_currentCallDrawMode = drawMode;

    beginUpdate();
    ui->countSpinBox->setValue(m_currentCallElementCount);
    ui->elementTypeComboBox->setCurrentIndex(m_currentCallElementTypeIndex);
    ui->byteOffsetSpinBox->setValue(m_currentCallElementByteOffset);
    ui->baseVertexSpinBox->setValue(m_currentCallElementBaseVertex);
    ui->instanceCountSpinBox->setValue(m_currentCallInstanceCount);
    ui->baseInstanceSpinBox->setValue(m_currentCallBaseInstance);
    endUpdate(true);

    m_bUserSetOptions = true;
}

void vogleditor_QVertexArrayExplorer::auto_update_element_count()
{
    // default to the element array attached to the selected VAO.
    const uint8_vec *pElementArray = (m_pVaoElementArray != NULL) ? m_pVaoElementArray : m_currentCallElementIndices;

    // Determine how many indices and vertices to display
    if (pElementArray != NULL)
    {
        int maxIndexCount = 0;
        if (ui->elementTypeComboBox->currentIndex() == 0)
        {
            // GL_UNSIGNED_BYTE
            maxIndexCount = pElementArray->size();
        }
        else if (ui->elementTypeComboBox->currentIndex() == 1)
        {
            // GL_UNSIGNED_SHORT
            maxIndexCount = pElementArray->size() / 2;
        }
        else if (ui->elementTypeComboBox->currentIndex() == 2)
        {
            // GL_UNSIGNED_INT
            maxIndexCount = pElementArray->size() / 4;
        }
        else
        {
            // invalid option, so bail
            VOGL_ASSERT(!"Invalid index from elementTypeComboBox in the QVertexArrayExplorer");
            return;
        }

        ui->countSpinBox->setValue(maxIndexCount);
    }
}

QString vogleditor_QVertexArrayExplorer::format_buffer_data_as_string(uint32_t index, const vogl_buffer_state &bufferState, const vogl_vertex_attrib_desc &attribDesc)
{
    /*
    attribDesc.m_size; // number of components
    attribDesc.m_type; // byte, unsigned byte, short, ushort, int, uint (accepted by VertexAttribPointer and VertexAttribIPointer);
                       // half_float, float, double, fixed, Int2101010Rev, Uint2101010Rev, & uint10f11f11fRev also accepted by glVertexAttribPointer;
                       // double is accepted by glVertexAttribLPointer only (no other types are allowed for that call)
    attribDesc.m_normalized; // indicates that fixed-point data should be normalized when they are accessed (otherwise leave as fixed-point)
    attribDesc.m_stride; // Specifies the byte offset between consecutive generic vertex attributes. If stride is 0, the generic vertex attributes are understood to be tightly packed in the array.
    attribDesc.m_pointer; // Specifies a offset of the first component of the first generic vertex attribute in the array in the data store of the buffer currently bound to the GL_ARRAY_BUFFER target.

    attribDesc.m_divisor; // Specify the number of instances that will pass between updates of the generic attribute at slot index.
                          // glVertexAttribDivisor modifies the rate at which generic vertex attributes advance when rendering multiple instances of primitives in a single draw call. If divisor is zero, the attribute at slot index advances once per vertex. If divisor is non-zero, the attribute advances once per divisor instances of the set(s) of vertices being rendered. An attribute is referred to as instanced if its GL_VERTEX_ATTRIB_ARRAY_DIVISOR value is non-zero.
    attribDesc.m_integer; // True if glVertexAttribIPointer is used; values should remain as integer when sent to shader.
*/

    uint32_t bytesPerComponent = vogl_get_gl_type_size(attribDesc.m_type);
    uint32_t bytesPerAttribute = bytesPerComponent * attribDesc.m_size;

    uint32_t stride = (attribDesc.m_stride != 0) ? attribDesc.m_stride : bytesPerAttribute;

    // account for divisor
    if (attribDesc.m_divisor != 0)
    {
        index /= attribDesc.m_divisor;
    }

    uint32_t curAttributeDataIndex = stride * index;

    // make sure accessing this attribute's components will not read past the end of the buffer
    uint32_t bufferSize = bufferState.get_buffer_data().size();
    if (index > bufferSize ||
        curAttributeDataIndex > bufferSize ||
        curAttributeDataIndex + bytesPerAttribute > bufferSize)
    {
        return "Out of bounds";
    }

    // index into the buffer
    const uint8_t *pCurAttributeData = &(bufferState.get_buffer_data()[curAttributeDataIndex]);

    // print each component
    dynamic_string attributeValueString;
    if (attribDesc.m_size > 1)
        attributeValueString += "{ ";

    for (int i = 0; i < attribDesc.m_size; i++)
    {
        // print the data appropriately
        if (attribDesc.m_type == GL_BYTE)
        {
            int8_t *pData = (int8_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
            {
                attributeValueString.format_append("%hhd", *pData);
            }
            else
            {
                float normalized = (float)*pData / (float)SCHAR_MAX;
                attributeValueString.format_append("%.8g", normalized);
            }
        }
        else if (attribDesc.m_type == GL_UNSIGNED_BYTE)
        {
            uint8_t *pData = (uint8_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
            {
                attributeValueString.format_append("%hhu", *pData);
            }
            else
            {
                float normalized = (float)*pData / (float)UCHAR_MAX;
                attributeValueString.format_append("%.8g", normalized);
            }
        }
        else if (attribDesc.m_type == GL_SHORT)
        {
            int16_t *pData = (int16_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
            {
                attributeValueString.format_append("%hd", *pData);
            }
            else
            {
                float normalized = (float)*pData / (float)SHRT_MAX;
                attributeValueString.format_append("%.8g", normalized);
            }
        }
        else if (attribDesc.m_type == GL_UNSIGNED_SHORT)
        {
            uint16_t *pData = (uint16_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
            {
                attributeValueString.format_append("%hu", *pData);
            }
            else
            {
                float normalized = (float)*pData / (float)USHRT_MAX;
                attributeValueString.format_append("%.8g", normalized);
            }
        }
        else if (attribDesc.m_type == GL_INT)
        {
            int32_t *pData = (int32_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
            {
                attributeValueString.format_append("%d", *pData);
            }
            else
            {
                float normalized = (float)*pData / (float)INT_MAX;
                attributeValueString.format_append("%.8g", normalized);
            }
        }
        else if (attribDesc.m_type == GL_UNSIGNED_INT)
        {
            uint32_t *pData = (uint32_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
            {
                attributeValueString.format_append("%u", *pData);
            }
            else
            {
                float normalized = (float)*pData / (float)UINT_MAX;
                attributeValueString.format_append("%.8g", normalized);
            }
        }
        else if (attribDesc.m_type == GL_FLOAT)
        {
            float *pData = (float *)pCurAttributeData;
            attributeValueString.format_append("%.8g", *pData);
        }
        else if (attribDesc.m_type == GL_DOUBLE)
        {
            double *pData = (double *)pCurAttributeData;
            attributeValueString.format_append("%.17g", *pData);
        }
        else
        {
            // half-float, fixed, int_2_10_10_10_REV, UINT_2_10_10_10_REV, UINT_10F_11F_11F_REV
            attributeValueString.append("unhandled format");
        }

        // add comma if needed
        if (i < attribDesc.m_size - 1)
            attributeValueString += ", ";

        pCurAttributeData += bytesPerComponent;
    }

    if (attribDesc.m_size > 1)
        attributeValueString += " }";

    return attributeValueString.c_str();
}

QVector3D vogleditor_QVertexArrayExplorer::buffer_data_to_QVector3D(uint32_t index, const vogl_buffer_state &bufferState, const vogl_vertex_attrib_desc &attribDesc)
{
    //Works like format_buffer_data_as_string but returns QVector3D

    uint32_t bytesPerComponent = vogl_get_gl_type_size(attribDesc.m_type);
    uint32_t bytesPerAttribute = bytesPerComponent * attribDesc.m_size;

    uint32_t stride = (attribDesc.m_stride != 0) ? attribDesc.m_stride : bytesPerAttribute;

    // account for divisor
    if (attribDesc.m_divisor != 0)
    {
        index /= attribDesc.m_divisor;
    }

    uint32_t curAttributeDataIndex = stride * index;

    // make sure accessing this attribute's components will not read past the end of the buffer
    uint32_t bufferSize = bufferState.get_buffer_data().size();
    if (index > bufferSize ||
        curAttributeDataIndex > bufferSize ||
        curAttributeDataIndex + bytesPerAttribute > bufferSize)
    {
        return QVector3D();
    }

    // index into the buffer
    const uint8_t *pCurAttributeData = &(bufferState.get_buffer_data()[curAttributeDataIndex]);

    float values[3];
    for (int i = 0; (i < attribDesc.m_size) && (i < 3); i++)
    {
        values[i]=0;
        // convert the data appropriately
        if (attribDesc.m_type == GL_BYTE)
        {
            int8_t *pData = (int8_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
                values[i]=(float)*pData;
            else
            {
                float normalized = (float)*pData / (float)SCHAR_MAX;
                values[i]=normalized;
            }
        }
        else if (attribDesc.m_type == GL_UNSIGNED_BYTE)
        {
            uint8_t *pData = (uint8_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
                values[i]=(float)*pData;
            else
            {
                float normalized = (float)*pData / (float)UCHAR_MAX;
                values[i]=normalized;
            }
        }
        else if (attribDesc.m_type == GL_SHORT)
        {
            int16_t *pData = (int16_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
                values[i]=(float)*pData;
            else
            {
                float normalized = (float)*pData / (float)SHRT_MAX;
                values[i]=normalized;
            }
        }
        else if (attribDesc.m_type == GL_UNSIGNED_SHORT)
        {
            uint16_t *pData = (uint16_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
                values[i]=(float)*pData;
            else
            {
                float normalized = (float)*pData / (float)USHRT_MAX;
                values[i]=normalized;
            }
        }
        else if (attribDesc.m_type == GL_INT)
        {
            int32_t *pData = (int32_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
                values[i]=(float)*pData;
            else
            {
                float normalized = (float)*pData / (float)INT_MAX;
                values[i]=normalized;
            }
        }
        else if (attribDesc.m_type == GL_UNSIGNED_INT)
        {
            uint32_t *pData = (uint32_t *)pCurAttributeData;
            if (!attribDesc.m_normalized)
                values[i]=(float)*pData;
            else
            {
                float normalized = (float)*pData / (float)UINT_MAX;
                values[i]=normalized;
            }
        }
        else if (attribDesc.m_type == GL_FLOAT)
        {
            float *pData = (float *)pCurAttributeData;
            values[i]=*pData;
        }
        else if (attribDesc.m_type == GL_DOUBLE)
        {
            double *pData = (double *)pCurAttributeData;
            values[i]=(float)*pData;
        }
//        else
//        {
            // half-float, fixed, int_2_10_10_10_REV, UINT_2_10_10_10_REV, UINT_10F_11F_11F_REV
//        }
        pCurAttributeData += bytesPerComponent;
    }

    QVector3D vertex;
    vertex.setX(values[0]);
    if (attribDesc.m_size>1)
        vertex.setY(values[1]);
    if (attribDesc.m_size>2)
        vertex.setZ(values[2]);
    return vertex;
//    return attributeValueString.c_str();

}

void vogleditor_QVertexArrayExplorer::on_vertexArrayComboBox_currentIndexChanged(int index)
{
    if (index < 0 || index >= ui->vertexArrayComboBox->count())
    {
        return;
    }

    m_pVaoState = ui->vertexArrayComboBox->itemData(index).value<vogl_vao_state *>();

    update_array_table_headers();

    if (m_bUserSetOptions)
    {
        beginUpdate();
        ui->countSpinBox->setValue(m_currentCallElementCount);
        ui->elementTypeComboBox->setCurrentIndex(m_currentCallElementTypeIndex);
        ui->baseVertexSpinBox->setValue(m_currentCallElementBaseVertex);

        int byteOffset = (m_pVaoElementArray != NULL) ? m_currentCallElementByteOffset : 0;
        ui->byteOffsetSpinBox->setValue(byteOffset);
        endUpdate();
    }
    else
    {
        // leave all options as they are, but set the count
        auto_update_element_count();
    }

    if (!is_ui_update_in_progress())
    {
        update_vertex_array_table();
        update_instance_array_table();
    }
}

bool vogleditor_QVertexArrayExplorer::calculate_element_and_format_as_string(const uint8_vec *pElementArray, uint32_t index, int typeIndex, int byteOffset, int baseVertex, uint32_t &elementValue, QString &elementString)
{
    if (pElementArray != NULL)
    {
        if (typeIndex == 0)
        {
            // GL_UNSIGNED_BYTE
            if (byteOffset + sizeof(uint8_t) * (index + 1) > pElementArray->size_in_bytes())
            {
                elementString = "Out of bounds";
                return false;
            }
            else
            {
                elementValue = ((uint8_t *)(pElementArray->get_ptr() + byteOffset))[index];
            }
        }
        else if (typeIndex == 1)
        {
            // GL_UNSIGNED_SHORT
            if (byteOffset + sizeof(uint16_t) * (index + 1) > pElementArray->size_in_bytes())
            {
                elementString = "Out of bounds";
                return false;
            }
            else
            {
                elementValue = ((uint16_t *)(pElementArray->get_ptr() + byteOffset))[index];
            }
        }
        else if (typeIndex == 2)
        {
            // GL_UNSIGNED_INT
            if (byteOffset + sizeof(uint32_t) * (index + 1) > pElementArray->size_in_bytes())
            {
                elementString = "Out of bounds";
                return false;
            }
            else
            {
                elementValue = ((uint32_t *)(pElementArray->get_ptr() + byteOffset))[index];
            }
        }
        else
        {
            VOGL_ASSERT(!"Invalid type index supplied to calculate_element_and_format_as_string");
            // invalid option, so bail out
            return false;
        }
    }
    else
    {
        elementValue = index;
    }

    // add baseVertex to the vertex index
    elementValue += baseVertex;

    elementString = QString::number(elementValue);
    return true;
}

void vogleditor_QVertexArrayExplorer::update_array_table_headers()
{
    // get all the accessible buffers
    vogl_gl_object_state_ptr_vec allBuffers;
    for (uint i = 0; i < m_sharing_contexts.size(); i++)
    {
        vogl_gl_object_state_ptr_vec tmpBufferVec;
        m_sharing_contexts[i]->get_all_objects_of_category(cGLSTBuffer, tmpBufferVec);
        allBuffers.append(tmpBufferVec);
    }

    uint maxAttribCount = m_pVaoState->get_vertex_attrib_count();
    m_attrib_buffers.resize(maxAttribCount);

    // count the attribs that have a buffer binding and look up those buffers
    for (uint i = 0; i < maxAttribCount; i++)
    {
        m_attrib_buffers[i] = NULL;

        GLuint array_binding = m_pVaoState->get_vertex_attrib_desc(i).m_array_binding;
        if (array_binding != 0 && m_pVaoState->get_vertex_attrib_desc(i).m_enabled)
        {
            for (uint b = 0; b < allBuffers.size(); b++)
            {
                if (allBuffers[b]->get_snapshot_handle() == array_binding)
                {
                    m_attrib_buffers[i] = static_cast<vogl_buffer_state *>(allBuffers[b]);
                    break;
                }
            }
        }
    }

    // identify if there is an index buffer
    m_pVaoElementArray = NULL;
    uint elementArrayBufferHandle = m_pVaoState->get_element_array_binding();
    if (elementArrayBufferHandle != 0)
    {
        for (uint b = 0; b < allBuffers.size(); b++)
        {
            if (allBuffers[b]->get_snapshot_handle() == elementArrayBufferHandle)
            {
                m_pVaoElementArray = &(static_cast<vogl_buffer_state *>(allBuffers[b])->get_buffer_data());
                break;
            }
        }
    }

    // this will store the column labels
    QStringList headers;
    QStringList instancedHeaders;

    if (m_pVaoElementArray != NULL)
    {
        headers << QString("Element Buffer %1").arg(elementArrayBufferHandle);
    }
    else if (m_currentCallElementIndices != NULL)
    {
        headers << QString("Indices (Client Array)");
    }
    else
    {
        headers << QString("Implied Indices");
    }

    instancedHeaders << QString("InstanceId");
    uint instancedCount = 0;
    for (uint b = 0; b < m_attrib_buffers.size(); b++)
    {
        if (m_attrib_buffers[b] != NULL)
        {
            // determine whether this attrib is per-vertex or per-instance
            if (m_pVaoState->get_vertex_attrib_desc(b).m_divisor == 0)
            {
                headers << QString("Attrib %1 (Buffer %2)").arg(b).arg(m_attrib_buffers[b]->get_snapshot_handle());
            }
            else
            {
                instancedHeaders << QString("Attrib %1 (Buffer %2)").arg(b).arg(m_attrib_buffers[b]->get_snapshot_handle());
                instancedCount++;
            }
        }
    }

    // set column headers
    ui->vertexTableWidget->setColumnCount(headers.size());
    ui->vertexTableWidget->setHorizontalHeaderLabels(headers);

    ui->instancedVertexTableWidget->setColumnCount(instancedHeaders.size());
    ui->instancedVertexTableWidget->setHorizontalHeaderLabels(instancedHeaders);

    if (instancedCount == 0)
    {
        // no instanced vertex data, so collapse the view
        QList<int> sizes;
        sizes << 1 << 1 << 0;
        ui->splitter->setSizes(sizes);
    }
    else
    {
        QList<int> sizes;
        sizes << 1 << 1 << 1;
        ui->splitter->setSizes(sizes);
    }
}

void vogleditor_QVertexArrayExplorer::update_vertex_array_table()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    // Identify the correct element array to reference
    const uint8_vec *pElementArray = m_pVaoElementArray;
    if (pElementArray == NULL)
    {
        if (m_currentCallElementIndices != NULL)
        {
            pElementArray = m_currentCallElementIndices;
        }
        else
        {
            // no element array means there will be implied indices (ie, in the case of glDrawArrays)
            pElementArray = NULL;
        }
    }

    // Populate each row of the table
    int32_t indexCount = ui->countSpinBox->value();
    ui->vertexTableWidget->setRowCount(indexCount);
    for (int i = 0; i < indexCount; i++)
    {
        uint32_t elementIndex = i;

        int byteOffset = ui->byteOffsetSpinBox->value();
        int baseVertex = ui->baseVertexSpinBox->value();

        QString elementString;

        bool bValidElement = calculate_element_and_format_as_string(pElementArray, elementIndex, ui->elementTypeComboBox->currentIndex(), byteOffset, baseVertex, elementIndex, elementString);

        // populated element index value
        ui->vertexTableWidget->setItem(i, 0, new QTableWidgetItem(elementString));

        // populate attrib values
        // column 0 contains element indices, so start at appropriate column
        uint column = 1;
        for (uint b = 0; b < m_attrib_buffers.size(); b++)
        {
            // make sure buffer is available and that divisor is 0 (which means it is NOT a per-instance attribute)
            if (m_attrib_buffers[b] != NULL && m_pVaoState->get_vertex_attrib_desc(b).m_divisor == 0)
            {
                if (bValidElement)
                {
                    ui->vertexTableWidget->setItem(i, column, new QTableWidgetItem(format_buffer_data_as_string(elementIndex, *(m_attrib_buffers[b]), m_pVaoState->get_vertex_attrib_desc(b))));
                }
                else
                {
                    ui->vertexTableWidget->setItem(i, column, new QTableWidgetItem("Out of bounds"));
                }
                column++;
            }
        }
    }

    // resize columns to fit contents, but then reset back to interactive so the user can resize them manually
    for (int i = 0; i < ui->vertexTableWidget->columnCount(); i++)
    {
        ui->vertexTableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        uint tmpWidth = ui->vertexTableWidget->horizontalHeader()->sectionSize(i);
        ui->vertexTableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
        ui->vertexTableWidget->horizontalHeader()->resizeSection(i, tmpWidth);
    }

    update_vertex_array_visualizations();

    QApplication::restoreOverrideCursor();
}


void vogleditor_QVertexArrayExplorer::update_vertex_array_visualizations()
{
    vogl::vector<uint> attrib_buffers_to_render;
    for (uint b = 0; b < m_attrib_buffers.size(); b++)
    {
        // make sure buffer is available and that divisor is 0 (which means it is NOT a per-instance attribute)
        if (m_attrib_buffers[b] != NULL && m_pVaoState->get_vertex_attrib_desc(b).m_divisor == 0)
        {
            attrib_buffers_to_render.push_back(b);
        }
    }
    //Ensure we have the correct number of vertexVisualizers.
    if (m_vertexVisualizers.size()<attrib_buffers_to_render.size())
    {
        for (uint b = m_vertexVisualizers.size(); b < attrib_buffers_to_render.size(); b++)
        {
            vogleditor_QVertexVisualizer *vis = new vogleditor_QVertexVisualizer(this);
            m_vertexVisualizers.push_back(vis);
            ui->vertexVisualizersLayout->addWidget(vis);
        }
    }
    else if (m_vertexVisualizers.size()>attrib_buffers_to_render.size())
    {
        for (uint b = m_vertexVisualizers.size(); b > attrib_buffers_to_render.size(); b--)
        {
            m_vertexVisualizers.at(b-1)->deleteLater();
            m_vertexVisualizers.pop_back();
        }
    }

    // Identify the correct element array to reference
    const uint8_vec *pElementArray = m_pVaoElementArray;
    if (pElementArray == NULL)
    {
        if (m_currentCallElementIndices != NULL)
        {
            pElementArray = m_currentCallElementIndices;
        }
        else
        {
            // no element array means there will be implied indices (ie, in the case of glDrawArrays)
            pElementArray = NULL;
        }
    }

    int32_t indexCount = ui->countSpinBox->value();
    int byteOffset = ui->byteOffsetSpinBox->value();
    int baseVertex = ui->baseVertexSpinBox->value();
    int elementType = ui->elementTypeComboBox->currentIndex();
    QString elementString;
    for (uint b = 0; b < attrib_buffers_to_render.size(); b++)
    {
        QString label = QString("Attrib %1 (Buffer %2)").arg(b).arg(m_attrib_buffers[attrib_buffers_to_render[b]]->get_snapshot_handle());
        m_vertexVisualizers.at(b)->setLabel(label);
        m_vertexVisualizers.at(b)->setDrawMode(m_currentCallDrawMode);
        //Add the vertex values
        QVector<QVector3D> vertices;
        for (int i = 0; i < indexCount; i++)
        {
            uint32_t elementIndex = i;
            //We should not have to format the element as a string in order to work out if it is valid:
            bool bValidElement = calculate_element_and_format_as_string(pElementArray, elementIndex, elementType, byteOffset, baseVertex, elementIndex, elementString);
            if (bValidElement)
            {
//                QString vertexstring = format_buffer_data_as_string(elementIndex, *(m_attrib_buffers[attrib_buffers_to_render[b]]), m_pVaoState->get_vertex_attrib_desc(b));
                QVector3D vertex = buffer_data_to_QVector3D(elementIndex, *(m_attrib_buffers[attrib_buffers_to_render[b]]), m_pVaoState->get_vertex_attrib_desc(b));
                vertices.push_back(vertex);
            }
            else
                break;
        }
        m_vertexVisualizers.at(b)->setVertices(vertices);
    }
}

void vogleditor_QVertexArrayExplorer::update_instance_array_table()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    // Populate each row of the table
    uint32_t instanceCount = ui->instanceCountSpinBox->value();
    ui->instancedVertexTableWidget->setRowCount(instanceCount);
    for (uint32_t i = 0; i < instanceCount; i++)
    {
        int baseInstance = ui->baseInstanceSpinBox->value();

        uint32_t instanceIndex = baseInstance + i;

        QString instanceString = QString::number(instanceIndex);

        // populated element index value
        ui->instancedVertexTableWidget->setItem(i, 0, new QTableWidgetItem(instanceString));

        // populate attrib values
        // column 0 contains instance Ids, so start at appropriate column
        uint column = 1;
        for (uint b = 0; b < m_attrib_buffers.size(); b++)
        {
            // make sure buffer is available and that divisor is NON-ZERO (which means it IS a per-instance attribute)
            if (m_attrib_buffers[b] != NULL && this->m_pVaoState->get_vertex_attrib_desc(b).m_divisor != 0)
            {
                ui->instancedVertexTableWidget->setItem(i, column, new QTableWidgetItem(format_buffer_data_as_string(instanceIndex, *(m_attrib_buffers[b]), m_pVaoState->get_vertex_attrib_desc(b))));
                column++;
            }
        }
    }

    // resize columns to fit contents, but then reset back to interactive so the user can resize them manually
    for (int i = 0; i < ui->instancedVertexTableWidget->columnCount(); i++)
    {
        ui->instancedVertexTableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        uint tmpWidth = ui->instancedVertexTableWidget->horizontalHeader()->sectionSize(i);
        ui->instancedVertexTableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
        ui->instancedVertexTableWidget->horizontalHeader()->resizeSection(i, tmpWidth);
    }

    QApplication::restoreOverrideCursor();
}

void vogleditor_QVertexArrayExplorer::on_elementTypeComboBox_currentIndexChanged(int index)
{
    VOGL_NOTE_UNUSED(index);

    if (!m_bUserSetOptions)
    {
        // since the type will affect the amount of memory read, update the element count
        // if the user has not specifically indicated one.
        beginUpdate();
        auto_update_element_count();
        endUpdate();
    }

    if (!is_ui_update_in_progress())
    {
        update_vertex_array_table();
    }
}

void vogleditor_QVertexArrayExplorer::on_countSpinBox_valueChanged(int arg1)
{
    VOGL_NOTE_UNUSED(arg1);

    if (!is_ui_update_in_progress())
    {
        update_vertex_array_table();
    }
}

void vogleditor_QVertexArrayExplorer::on_byteOffsetSpinBox_valueChanged(int arg1)
{
    VOGL_NOTE_UNUSED(arg1);

    if (!is_ui_update_in_progress())
    {
        update_vertex_array_table();
    }
}

void vogleditor_QVertexArrayExplorer::on_baseVertexSpinBox_valueChanged(int arg1)
{
    VOGL_NOTE_UNUSED(arg1);

    if (!is_ui_update_in_progress())
    {
        update_vertex_array_table();
    }
}

void vogleditor_QVertexArrayExplorer::on_instanceCountSpinBox_valueChanged(int arg1)
{
    VOGL_NOTE_UNUSED(arg1);

    if (!is_ui_update_in_progress())
    {
        update_instance_array_table();
    }
}

void vogleditor_QVertexArrayExplorer::on_baseInstanceSpinBox_valueChanged(int arg1)
{
    VOGL_NOTE_UNUSED(arg1);

    if (!is_ui_update_in_progress())
    {
        update_instance_array_table();
    }
}
