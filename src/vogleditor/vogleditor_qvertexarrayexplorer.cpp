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

#include "vogl_warnings.h"
#include "vogleditor_qvertexarrayexplorer.h"


GCC_DIAGNOSTIC_PUSH()
GCC_DIAGNOSTIC_IGNORED(packed)
#include "ui_vogleditor_qvertexarrayexplorer.h"
GCC_DIAGNOSTIC_POP()

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_vao_state.h"

Q_DECLARE_METATYPE(vogl_vao_state*);

vogleditor_QVertexArrayExplorer::vogleditor_QVertexArrayExplorer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vogleditor_QVertexArrayExplorer)
{
    ui->setupUi(this);

    ui->indexTypeComboBox->addItem("GL_UNSIGNED_BYTE");
    ui->indexTypeComboBox->addItem("GL_UNSIGNED_SHORT");
    ui->indexTypeComboBox->addItem("GL_UNSIGNED_INT");
}

vogleditor_QVertexArrayExplorer::~vogleditor_QVertexArrayExplorer()
{
    delete ui;
}

void vogleditor_QVertexArrayExplorer::clear()
{
    m_sharing_contexts.clear();
    ui->vertexArrayComboBox->clear();
    ui->vertexTableWidget->clear();
}

uint vogleditor_QVertexArrayExplorer::set_vertexarray_objects(vogl_context_snapshot* pContext, vogl::vector<vogl_context_snapshot*> sharingContexts)
{
    clear();

    m_sharing_contexts = sharingContexts;

    uint vaoCount = 0;
    vogl_gl_object_state_ptr_vec vaoObjects;
    pContext->get_all_objects_of_category(cGLSTVertexArray, vaoObjects);
    vaoCount = vaoObjects.size();

    for (vogl_gl_object_state_ptr_vec::iterator iter = vaoObjects.begin(); iter != vaoObjects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTVertexArray)
        {
            vogl_vao_state* pState = static_cast<vogl_vao_state*>(*iter);

            QString valueStr;

            valueStr = valueStr.sprintf("VertexArray %" PRIu64 " - %u attributes", pState->get_snapshot_handle(), pState->get_vertex_attrib_count());

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
        vogl_vao_state* pState = ui->vertexArrayComboBox->itemData(i).value<vogl_vao_state*>();
        if (pState->get_snapshot_handle() == vertexArrayHandle)
        {
            ui->vertexArrayComboBox->setCurrentIndex(i);
            return true;
        }
    }

    return false;
}

QString formatBufferDataAsString(uint32_t index, const vogl_buffer_state& bufferState, const vogl_vertex_attrib_desc& attribDesc)
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

    // TODO: need to account for divisor
    uint32_t curAttributeDataIndex = stride*index;

    // make sure accessing this attribute's components will not read past the end of the buffer
    uint32_t bufferSize = bufferState.get_buffer_data().size();
    if (index > bufferSize ||
        curAttributeDataIndex > bufferSize ||
        curAttributeDataIndex + bytesPerAttribute > bufferSize)
    {
        return "Out of bounds";
    }

    // index into the buffer
    // TODO: need to account for divisor
    const uint8_t* pCurAttributeData = &(bufferState.get_buffer_data()[stride*index]);

    // print each component
    dynamic_string attributeValueString;
    if (attribDesc.m_size > 1)
        attributeValueString += "{ ";

    for (int i = 0; i < attribDesc.m_size; i++)
    {
        // print the data appropriately
        if (attribDesc.m_type == GL_BYTE)
        {
            int8_t* pData = (int8_t*)pCurAttributeData;
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
            uint8_t* pData = (uint8_t*)pCurAttributeData;
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
            int16_t* pData = (int16_t*)pCurAttributeData;
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
            uint16_t* pData = (uint16_t*)pCurAttributeData;
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
            int32_t* pData = (int32_t*)pCurAttributeData;
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
            uint32_t* pData = (uint32_t*)pCurAttributeData;
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
            float* pData = (float*)pCurAttributeData;
            attributeValueString.format_append("%.8g", *pData);
        }
        else if (attribDesc.m_type == GL_DOUBLE)
        {
            double* pData = (double*)pCurAttributeData;
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


void vogleditor_QVertexArrayExplorer::on_vertexArrayComboBox_currentIndexChanged(int index)
{
    if (index < 0 || index >= ui->vertexArrayComboBox->count())
    {
        return;
    }

    // get all the accessible buffers
    vogl_gl_object_state_ptr_vec bufferVec;
    for (uint i = 0; i < m_sharing_contexts.size(); i++)
    {
        vogl_gl_object_state_ptr_vec tmpBufferVec;
        m_sharing_contexts[i]->get_all_objects_of_category(cGLSTBuffer, tmpBufferVec);
        bufferVec.append(tmpBufferVec);
    }

    vogl_vao_state* pState = ui->vertexArrayComboBox->itemData(index).value<vogl_vao_state*>();

    // need a column for each attrib + 1 for the index buffer
    uint maxAttribCount = pState->get_vertex_attrib_count();
    uint attribCount = 0;

    vogl::vector<vogl_buffer_state*> buffers;
    buffers.resize(maxAttribCount);

    // count the attribs that have a buffer binding and look up those buffers
    for (uint i = 0; i < maxAttribCount; i++)
    {
        buffers[i] = NULL;

        GLuint array_binding = pState->get_vertex_attrib_desc(i).m_array_binding;
        if (array_binding != 0 && pState->get_vertex_attrib_desc(i).m_enabled)
        {
            for (uint b = 0; b < bufferVec.size(); b++)
            {
                if (bufferVec[b]->get_snapshot_handle() == array_binding)
                {
                    attribCount++;
                    buffers[i] = static_cast<vogl_buffer_state*>(bufferVec[b]);
                    break;
                }
            }
        }
    }

    // this will store the column labels
    QStringList headers;

    // identify if there is an index buffer
    uint indexBufferHandle = 0;
    vogl_buffer_state* pIndexBufferState = NULL;
    if (pState->get_vertex_attrib_count() > 0)
    {
        indexBufferHandle = pState->get_vertex_attrib_desc(0).m_element_array_binding;
        if (indexBufferHandle != 0)
        {
            for (uint b = 0; b < bufferVec.size(); b++)
            {
                if (bufferVec[b]->get_snapshot_handle() == indexBufferHandle)
                {
                    pIndexBufferState = static_cast<vogl_buffer_state*>(bufferVec[b]);
                    break;
                }
            }
        }

        headers << QString("Element Buffer %1").arg(indexBufferHandle);
    }

    // set appropriate number of columns (one for each attrib + one for the index buffer if it is available)
    ui->vertexTableWidget->setColumnCount(attribCount + ((indexBufferHandle > 0)? 1 : 0));

    for (uint b = 0; b < buffers.size(); b++)
    {
        if (buffers[b] != NULL)
        {
            headers << QString("Attrib %1 (Buffer %2)").arg(b).arg(buffers[b]->get_snapshot_handle());
        }
    }

    ui->vertexTableWidget->setHorizontalHeaderLabels(headers);

    uint indexCount = 0;

    // Determine how many vertices (ie, indices) to display
    if (pIndexBufferState == NULL)
    {
        // No index buffer, so assume 10 items
        indexCount = 10;
    }
    else
    {
        if (ui->indexTypeComboBox->currentIndex() == 0)
        {
            // GL_UNSIGNED_BYTE
            indexCount = pIndexBufferState->get_buffer_data().size();
        }
        else if (ui->indexTypeComboBox->currentIndex() == 1)
        {
            // GL_UNSIGNED_SHORT
            indexCount = pIndexBufferState->get_buffer_data().size() / 2;
        }
        else if (ui->indexTypeComboBox->currentIndex() == 2)
        {
            // GL_UNSIGNED_INT
            indexCount = pIndexBufferState->get_buffer_data().size() / 4;
        }
        else
        {
            // invalid option, so bail out
            return;
        }
    }

    // Populate each row of the table
    ui->vertexTableWidget->setRowCount(indexCount);
    for (uint i = 0; i < indexCount; i++)
    {
        uint32_t vertexIndex = i;
        if (pIndexBufferState == NULL)
        {
            ui->vertexTableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
        }
        else
        {
            if (ui->indexTypeComboBox->currentIndex() == 0)
            {
                // GL_UNSIGNED_BYTE
                vertexIndex = ((uint8_t*)pIndexBufferState->get_buffer_data().get_ptr())[i];
            }
            else if (ui->indexTypeComboBox->currentIndex() == 1)
            {
                // GL_UNSIGNED_SHORT
                vertexIndex = ((uint16_t*)pIndexBufferState->get_buffer_data().get_ptr())[i];
            }
            else if (ui->indexTypeComboBox->currentIndex() == 2)
            {
                // GL_UNSIGNED_INT
                vertexIndex = ((uint32_t*)pIndexBufferState->get_buffer_data().get_ptr())[i];
            }
            else
            {
                // invalid option, so bail out
                return;
            }

            ui->vertexTableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(vertexIndex)));
        }

        // populate attrib values
        if (attribCount > 0)
        {
            // column 0 is indices, so start at column 1
            uint column = 1;
            for (uint b = 0; b < buffers.size(); b++)
            {
                if (buffers[b] != NULL)
                {
                    ui->vertexTableWidget->setItem(i, column, new QTableWidgetItem(formatBufferDataAsString(vertexIndex, *(buffers[b]), pState->get_vertex_attrib_desc(b))));
                    column++;
                }
            }
        }
    }

    // resize columns to fit contents, but then reset back to interactive so the user can resize them manually
    for (int i = 0; i < ui->vertexTableWidget->columnCount(); i++)
    {
        ui->vertexTableWidget->horizontalHeader()->setResizeMode(i, QHeaderView::ResizeToContents);
        uint tmpWidth = ui->vertexTableWidget->horizontalHeader()->sectionSize(i);
        ui->vertexTableWidget->horizontalHeader()->setResizeMode(i, QHeaderView::Interactive);
        ui->vertexTableWidget->horizontalHeader()->resizeSection(i, tmpWidth);
    }
}

void vogleditor_QVertexArrayExplorer::on_indexTypeComboBox_currentIndexChanged(int index)
{
    VOGL_NOTE_UNUSED(index);
    on_vertexArrayComboBox_currentIndexChanged(ui->vertexArrayComboBox->currentIndex());
}
