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
#include "vogleditor_qbufferexplorer.h"

GCC_DIAGNOSTIC_PUSH()
GCC_DIAGNOSTIC_IGNORED(packed)
#include "ui_vogleditor_qbufferexplorer.h"
GCC_DIAGNOSTIC_POP()

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_buffer_state.h"
#include "vogl_vector.h"

Q_DECLARE_METATYPE(vogl_buffer_state *);

vogleditor_QBufferExplorer::vogleditor_QBufferExplorer(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::vogleditor_QBufferExplorer)
{
    ui->setupUi(this);

    ui->bufferTableWidget->setColumnCount(2);
    ui->bufferTableWidget->setHorizontalHeaderLabels(QStringList() << "Byte Offset"
                                                                   << "Value");

    m_formatParams.push_back(vogleditor_buffer_format_params(1, 1, "0x%02hhx", "8-bit hex"));
    m_formatParams.push_back(vogleditor_buffer_format_params(2, 1, "0x%04hx", "16-bit hex"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 1, "0x%08x", "32-bit hex"));
    m_formatParams.push_back(vogleditor_buffer_format_params(8, 1, "0x%016" PRIx64, "64-bit hex"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 1, "%1.8g", "GL_FLOAT"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 2, "%1.8g", "GL_FLOAT_VEC2"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 3, "%1.8g", "GL_FLOAT_VEC3"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 4, "%1.8g", "GL_FLOAT_VEC4"));
    m_formatParams.push_back(vogleditor_buffer_format_params(8, 1, "%1.17g", "GL_DOUBLE"));
    m_formatParams.push_back(vogleditor_buffer_format_params(8, 2, "%1.17g", "GL_DOUBLE_VEC2"));
    m_formatParams.push_back(vogleditor_buffer_format_params(8, 3, "%1.17g", "GL_DOUBLE_VEC3"));
    m_formatParams.push_back(vogleditor_buffer_format_params(8, 4, "%1.17g", "GL_DOUBLE_VEC4"));
    m_formatParams.push_back(vogleditor_buffer_format_params(1, 1, "%hhd", "GL_BYTE"));
    m_formatParams.push_back(vogleditor_buffer_format_params(1, 1, "%hhu", "GL_UNSIGNED_BYTE"));
    m_formatParams.push_back(vogleditor_buffer_format_params(2, 1, "%hd", "GL_SHORT"));
    m_formatParams.push_back(vogleditor_buffer_format_params(2, 1, "%hu", "GL_UNSIGNED_SHORT"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 1, "%d", "GL_INT"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 2, "%d", "GL_INT_VEC2"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 3, "%d", "GL_INT_VEC3"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 4, "%d", "GL_INT_VEC4"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 1, "%u", "GL_UNSIGNED_INT"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 2, "%u", "GL_UNSIGNED_INT_VEC2"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 3, "%u", "GL_UNSIGNED_INT_VEC3"));
    m_formatParams.push_back(vogleditor_buffer_format_params(4, 4, "%u", "GL_UNSIGNED_INT_VEC4"));

    for (uint i = 0; i < m_formatParams.size(); i++)
    {
        ui->formatComboBox->addItem(m_formatParams[i].m_name);
    }
}

vogleditor_QBufferExplorer::~vogleditor_QBufferExplorer()
{
    delete ui;
}

void vogleditor_QBufferExplorer::clear()
{
    ui->bufferComboBox->clear();
    ui->bufferTableWidget->setRowCount(0);
}

uint vogleditor_QBufferExplorer::set_buffer_objects(vogl::vector<vogl_context_snapshot *> sharingContexts)
{
    clear();

    uint bufferCount = 0;
    for (uint c = 0; c < sharingContexts.size(); c++)
    {
        vogl_gl_object_state_ptr_vec bufferObjects;
        sharingContexts[c]->get_all_objects_of_category(cGLSTBuffer, bufferObjects);

        bufferCount += add_buffer_objects(bufferObjects);
    }

    return bufferCount;
}

uint vogleditor_QBufferExplorer::add_buffer_objects(vogl_gl_object_state_ptr_vec objects)
{
    for (vogl_gl_object_state_ptr_vec::iterator iter = objects.begin(); iter != objects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTBuffer)
        {
            vogl_buffer_state *pState = static_cast<vogl_buffer_state *>(*iter);

            QString valueStr;

            valueStr = valueStr.sprintf("Buffer %" PRIu64 " (%u bytes) - %s", pState->get_snapshot_handle(), pState->get_buffer_data().size_in_bytes(), get_gl_enums().find_gl_name(pState->get_target()));

            int usage = 0;
            if (pState->get_params().get<int>(GL_BUFFER_USAGE, 0, &usage, 1))
            {
                valueStr += " - ";
                valueStr += get_gl_enums().find_gl_name(usage);
            }

            if (pState->get_is_mapped())
            {
                valueStr += " (";
                valueStr += get_gl_enums().find_gl_name(pState->get_map_access());
                valueStr += ")";
            }

            ui->bufferComboBox->addItem(valueStr, QVariant::fromValue(pState));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in vogleditor_QBufferExplorer");
        }
    }

    return objects.size();
}

bool vogleditor_QBufferExplorer::set_active_buffer(unsigned long long bufferHandle)
{
    for (int i = 0; i < ui->bufferComboBox->count(); i++)
    {
        vogl_buffer_state *pState = ui->bufferComboBox->itemData(i).value<vogl_buffer_state *>();
        if (pState->get_snapshot_handle() == bufferHandle)
        {
            ui->bufferComboBox->setCurrentIndex(i);
            return true;
        }
    }

    return false;
}

void vogleditor_QBufferExplorer::append_value(vogl::dynamic_string &rString, void *dataPtr, uint formatParamIndex)
{
    switch (formatParamIndex)
    {
        case 0:  // 8-bit hex
        case 13: // unsigned byte
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(uint8_t *)dataPtr);
            break;
        }
        case 1:  // 16-bit hex
        case 15: // unsigned short
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(uint16_t *)dataPtr);
            break;
        }
        case 2:  // 32-bit hex
        case 20: // uint
        case 21: // uint vec2
        case 22: // uint vec3
        case 23: // uint vec4
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(uint32_t *)dataPtr);
            break;
        }
        case 3: // 64-bit hex
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(uint64_t *)dataPtr);
            break;
        }
        case 12: // byte
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(int8_t *)dataPtr);
            break;
        }
        case 14: // short
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(int16_t *)dataPtr);
            break;
        }
        case 4: // float
        case 5: // float vec2
        case 6: // float vec3
        case 7: // float vec4
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(float *)dataPtr);
            break;
        }

        case 8:  // double
        case 9:  // double vec2
        case 10: // double vec3
        case 11: // double vec4
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(double *)dataPtr);
            break;
        }
        case 16: // int
        case 17: // int vec2
        case 18: // int vec3
        case 19: // int vec4
        {
            rString = rString.format_append(m_formatParams[formatParamIndex].m_string_formatter, *(int32_t *)dataPtr);
            break;
        }
        default:
            VOGL_ASSERT(!"Unsupported buffer data format");
    }
}

void vogleditor_QBufferExplorer::on_bufferComboBox_currentIndexChanged(int index)
{
    ui->bufferTableWidget->setRowCount(0);

    int count = ui->bufferComboBox->count();
    if (index >= 0 && index < count)
    {
        vogl_buffer_state *pObjState = ui->bufferComboBox->itemData(index).value<vogl_buffer_state *>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL buffer object type in vogleditor_QBufferExplorer");
            return;
        }

        const uint8_vec &data = pObjState->get_buffer_data();

        vogleditor_buffer_format_params format = m_formatParams[ui->formatComboBox->currentIndex()];
        uint numComponents = format.m_component_count;
        uint numElements = data.size() / (format.m_component_byte_count * numComponents);
        ui->bufferTableWidget->setRowCount(numElements);

        const uint8_t *dataPtr = data.get_const_ptr();

        for (uint32_t i = 0; i < numElements; i++)
        {
            dynamic_string value;

            for (uint32_t c = 0; c < numComponents; c++)
            {
                append_value(value, (void *)dataPtr, ui->formatComboBox->currentIndex());

                // increment to next component
                dataPtr += format.m_component_byte_count;

                if (c < numComponents - 1)
                    value += ", ";
            }

            ui->bufferTableWidget->setItem(i, 0, new QTableWidgetItem(QString("%1").arg(i * format.m_component_byte_count * numComponents)));
            ui->bufferTableWidget->setItem(i, 1, new QTableWidgetItem(value.c_str()));
        }
    }
}

void vogleditor_QBufferExplorer::on_formatComboBox_currentIndexChanged(int index)
{
    VOGL_NOTE_UNUSED(index);
    on_bufferComboBox_currentIndexChanged(ui->bufferComboBox->currentIndex());
}
