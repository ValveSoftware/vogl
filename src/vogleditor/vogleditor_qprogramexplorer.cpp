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
#include "vogleditor_qprogramexplorer.h"

GCC_DIAGNOSTIC_PUSH()
GCC_DIAGNOSTIC_IGNORED(packed)
#include "ui_vogleditor_qprogramexplorer.h"
GCC_DIAGNOSTIC_POP()

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_program_state.h"

Q_DECLARE_METATYPE(vogl_program_state *);
Q_DECLARE_METATYPE(vogl_shader_state *);

vogleditor_QProgramExplorer::vogleditor_QProgramExplorer(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::vogleditor_QProgramExplorer)
{
    ui->setupUi(this);

    ui->saveShaderButton->setEnabled(false);

    ui->uniformTableWidget->setColumnCount(4);
    ui->uniformTableWidget->setHorizontalHeaderLabels(QStringList() << "Loc"
                                                                    << "Name"
                                                                    << "Value"
                                                                    << "Type");
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_location, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_name, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_value, QHeaderView::Stretch);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_type, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(vogleditor_utc_location, 50);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(vogleditor_utc_name, 100);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(vogleditor_utc_type, 100);
}

vogleditor_QProgramExplorer::~vogleditor_QProgramExplorer()
{
    delete ui;
}

void vogleditor_QProgramExplorer::clear()
{
    ui->programListBox->clear();
    ui->shaderListBox->clear();
    ui->shaderTextEdit->clear();
    m_objects.clear();
    m_sharing_contexts.clear();
}

uint vogleditor_QProgramExplorer::set_program_objects(vogl::vector<vogl_context_snapshot *> sharingContexts)
{
    clear();

    m_sharing_contexts = sharingContexts;

    uint programCount = 0;
    for (uint c = 0; c < sharingContexts.size(); c++)
    {
        vogl_gl_object_state_ptr_vec programObjects;
        sharingContexts[c]->get_all_objects_of_category(cGLSTProgram, programObjects);

        programCount += add_program_objects(programObjects);
    }

    return programCount;
}

uint vogleditor_QProgramExplorer::add_program_objects(vogl_gl_object_state_ptr_vec objects)
{
    m_objects.append(objects);

    GLuint64 curProgram = m_sharing_contexts[0]->get_general_state().get_value<GLuint64>(GL_CURRENT_PROGRAM);

    for (vogl_gl_object_state_ptr_vec::iterator iter = objects.begin(); iter != objects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTProgram)
        {
            vogl_program_state *pState = static_cast<vogl_program_state *>(*iter);

            QString valueStr;
            valueStr = valueStr.sprintf("Program %" PRIu64, pState->get_snapshot_handle());

            if (curProgram == pState->get_snapshot_handle())
            {
                valueStr += " (bound)";
            }

            ui->programListBox->addItem(valueStr, QVariant::fromValue(pState));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in vogleditor_QProgramExplorer");
        }
    }

    return objects.size();
}

bool vogleditor_QProgramExplorer::set_active_program(unsigned long long programHandle)
{
    bool bActivated = false;
    int index = 0;
    for (vogl_gl_object_state_ptr_vec::iterator iter = m_objects.begin(); iter != m_objects.end(); iter++)
    {
        vogl_program_state *pState = static_cast<vogl_program_state *>(*iter);
        if (pState->get_snapshot_handle() == programHandle)
        {
            ui->programListBox->setCurrentIndex(index);
            bActivated = true;
            break;
        }

        ++index;
    }
    return bActivated;
}

void vogleditor_QProgramExplorer::on_programListBox_currentIndexChanged(int index)
{
    ui->shaderListBox->clear();

    int count = ui->programListBox->count();
    if (index >= 0 && index < count)
    {
        vogl_program_state *pObjState = ui->programListBox->itemData(index).value<vogl_program_state *>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL object type in vogleditor_QProgramExplorer");
            return;
        }

        vogl_shader_state_vec &attachedShaderVec = pObjState->get_shaders();
        vogl_shader_state_vec &linkedShaderVec = pObjState->get_link_time_snapshot()->get_shaders();

        QString valueStr;
        for (vogl_shader_state_vec::iterator linkedIter = linkedShaderVec.begin(); linkedIter != linkedShaderVec.end(); linkedIter++)
        {
            vogl_shader_state *pShaderState = linkedIter;
            valueStr = valueStr.sprintf("Linked Shader %" PRIu64 " - %s", pShaderState->get_snapshot_handle(), get_gl_enums().find_gl_name(pShaderState->get_shader_type()));
            ui->shaderListBox->addItem(valueStr, QVariant::fromValue(pShaderState));
        }

        for (vogl_shader_state_vec::iterator attachedIter = attachedShaderVec.begin(); attachedIter != attachedShaderVec.end(); attachedIter++)
        {
            vogl_shader_state *pShaderState = attachedIter;
            valueStr = valueStr.sprintf("Attached Shader %" PRIu64 " - %s", pShaderState->get_snapshot_handle(), get_gl_enums().find_gl_name(pShaderState->get_shader_type()));
            ui->shaderListBox->addItem(valueStr, QVariant::fromValue(pShaderState));
        }

        update_uniforms_for_program(pObjState);
    }
}

void vogleditor_QProgramExplorer::on_shaderListBox_currentIndexChanged(int index)
{
    ui->shaderTextEdit->clear();

    int count = ui->shaderListBox->count();
    if (index >= 0 && index < count)
    {
        vogl_shader_state *pObjState = ui->shaderListBox->itemData(index).value<vogl_shader_state *>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL shader object type in vogleditor_QProgramExplorer");
        }
        else
        {
            ui->shaderTextEdit->setText(pObjState->get_source().c_str());
        }
    }

    ui->saveShaderButton->setEnabled(false);
}

void vogleditor_QProgramExplorer::on_shaderTextEdit_textChanged()
{
    ui->saveShaderButton->setEnabled(true);
}

void vogleditor_QProgramExplorer::on_saveShaderButton_clicked()
{
    int index = ui->shaderListBox->currentIndex();
    if (index >= 0 && ui->shaderListBox->count() > 0)
    {
        vogl_shader_state *pObjState = ui->shaderListBox->itemData(index).value<vogl_shader_state *>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL shader object type in vogleditor_QProgramExplorer");
            return;
        }

        pObjState->set_source(ui->shaderTextEdit->toPlainText().toStdString().c_str());

        ui->saveShaderButton->setEnabled(false);

        vogl_program_state *pProgramState = ui->programListBox->itemData(index).value<vogl_program_state *>();
        emit program_edited(pProgramState);
    }
}

void vogleditor_QProgramExplorer::update_uniforms_for_program(vogl_program_state *pProgramState)
{
    // clear the table
    ui->uniformTableWidget->setRowCount(0);

    const vogl_uniform_state_vec &uniformVec = pProgramState->get_uniform_state_vec();
    uint rowIndex = 0;
    for (uint i = 0; i < uniformVec.size(); i++)
    {
        QString varName = QString("%1").arg(uniformVec[i].m_name.c_str());

        // check if element is an array
        bool isArray = varName.endsWith("[0]");

        unsigned int size = uniformVec[i].m_size;

        for (uint uniformElementIndex = 0; uniformElementIndex < size; uniformElementIndex++)
        {
            if (isArray)
            {
                // chop off "[0]" and add appropriate index
                varName.truncate(varName.indexOf("["));
                varName += QString("[%1]").arg(uniformElementIndex);
            }

            ui->uniformTableWidget->insertRow(rowIndex);

            int baseLocation = uniformVec[i].m_base_location;
            if (baseLocation >= 0)
            {
                baseLocation += uniformElementIndex;
            }

            ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_location, new QTableWidgetItem(QString("%1").arg(baseLocation)));
            ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_name, new QTableWidgetItem(varName));
            ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_type, new QTableWidgetItem(get_gl_enums().find_gl_name(uniformVec[i].m_type)));

            switch (uniformVec[i].m_type)
            {
                case GL_DOUBLE:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("%1").arg(pData[0])));
                    break;
                }
                case GL_DOUBLE_VEC2:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 2];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2 }").arg(pData[0]).arg(pData[1])));
                    break;
                }
                case GL_DOUBLE_VEC3:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 3];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3 }").arg(pData[0]).arg(pData[1]).arg(pData[2])));
                    break;
                }
                case GL_DOUBLE_VEC4:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 4];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3, %4 }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3])));
                    break;
                }
                case GL_DOUBLE_MAT2:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 4];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2 }, { %3, %4 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3])));
                    break;
                }
                case GL_DOUBLE_MAT3:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 9];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3 }, { %4, %5, %6 }, { %7, %8, %9 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8])));
                    break;
                }
                case GL_DOUBLE_MAT4:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 16];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3, %4 }, { %5, %6, %7, %8 }, { %9, %10, %11, %12 }, { %13, %14, %15, %16 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8]).arg(pData[9]).arg(pData[10]).arg(pData[11]).arg(pData[12]).arg(pData[13]).arg(pData[14]).arg(pData[15])));
                    break;
                }
                case GL_DOUBLE_MAT2x3:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 6];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2 }, { %3, %4 }, { %5, %6 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5])));
                    break;
                }
                case GL_DOUBLE_MAT3x2:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 6];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3 }, { %4, %5, %6 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5])));
                    break;
                }
                case GL_DOUBLE_MAT2x4:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 8];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2 }, { %3, %4 }, { %5, %6 }, { %7, %8 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7])));
                    break;
                }
                case GL_DOUBLE_MAT4x2:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 8];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3, %4 }, { %5, %6, %7, %8 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7])));
                    break;
                }
                case GL_DOUBLE_MAT3x4:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 12];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3 }, { %4, %5, %6 }, { %7, %8, %9 }, { %10, %11, %12 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8]).arg(pData[9]).arg(pData[10]).arg(pData[11])));
                    break;
                }
                case GL_DOUBLE_MAT4x3:
                {
                    double *pData = &((double *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 12];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3, %4 }, { %5, %6, %7, %8 } { %9, %10, %11, %12 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8]).arg(pData[9]).arg(pData[10]).arg(pData[11])));
                    break;
                }

                case GL_FLOAT:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("%1").arg(pData[0])));
                    break;
                }
                case GL_FLOAT_VEC2:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 2];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2 }").arg(pData[0]).arg(pData[1])));
                    break;
                }
                case GL_FLOAT_VEC3:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 3];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3 }").arg(pData[0]).arg(pData[1]).arg(pData[2])));
                    break;
                }
                case GL_FLOAT_VEC4:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 4];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3, %4 }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3])));
                    break;
                }
                case GL_FLOAT_MAT2:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 4];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2 }, { %3, %4 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3])));
                    break;
                }
                case GL_FLOAT_MAT3:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 9];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3 }, { %4, %5, %6 }, { %7, %8, %9 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8])));
                    break;
                }
                case GL_FLOAT_MAT4:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 16];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3, %4 }, { %5, %6, %7, %8 }, { %9, %10, %11, %12 }, { %13, %14, %15, %16 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8]).arg(pData[9]).arg(pData[10]).arg(pData[11]).arg(pData[12]).arg(pData[13]).arg(pData[14]).arg(pData[15])));
                    break;
                }
                case GL_FLOAT_MAT2x3:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 6];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2 }, { %3, %4 }, { %5, %6 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5])));
                    break;
                }
                case GL_FLOAT_MAT3x2:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 6];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3 }, { %4, %5, %6 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5])));
                    break;
                }
                case GL_FLOAT_MAT2x4:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 8];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2 }, { %3, %4 }, { %5, %6 }, { %7, %8 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7])));
                    break;
                }
                case GL_FLOAT_MAT4x2:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 8];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3, %4 }, { %5, %6, %7, %8 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7])));
                    break;
                }
                case GL_FLOAT_MAT3x4:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 12];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3 }, { %4, %5, %6 }, { %7, %8, %9 }, { %10, %11, %12 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8]).arg(pData[9]).arg(pData[10]).arg(pData[11])));
                    break;
                }
                case GL_FLOAT_MAT4x3:
                {
                    float *pData = &((float *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 12];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ { %1, %2, %3, %4 }, { %5, %6, %7, %8 } { %9, %10, %11, %12 } }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3]).arg(pData[4]).arg(pData[5]).arg(pData[6]).arg(pData[7]).arg(pData[8]).arg(pData[9]).arg(pData[10]).arg(pData[11])));
                    break;
                }

                case GL_SAMPLER_1D:
                case GL_SAMPLER_2D:
                case GL_SAMPLER_3D:
                case GL_SAMPLER_CUBE:
                case GL_SAMPLER_1D_SHADOW:
                case GL_SAMPLER_2D_SHADOW:
                case GL_SAMPLER_1D_ARRAY:
                case GL_SAMPLER_2D_ARRAY:
                case GL_SAMPLER_1D_ARRAY_SHADOW:
                case GL_SAMPLER_2D_ARRAY_SHADOW:
                case GL_SAMPLER_2D_MULTISAMPLE:
                case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_SAMPLER_CUBE_SHADOW:
                case GL_SAMPLER_BUFFER:
                case GL_SAMPLER_2D_RECT:
                case GL_SAMPLER_2D_RECT_SHADOW:
                case GL_SAMPLER_CUBE_MAP_ARRAY:
                case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
                case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
                case GL_INT_SAMPLER_1D:
                case GL_INT_SAMPLER_2D:
                case GL_INT_SAMPLER_3D:
                case GL_INT_SAMPLER_CUBE:
                case GL_INT_SAMPLER_1D_ARRAY:
                case GL_INT_SAMPLER_2D_ARRAY:
                case GL_INT_SAMPLER_2D_MULTISAMPLE:
                case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_INT_SAMPLER_BUFFER:
                case GL_INT_SAMPLER_2D_RECT:
                case GL_UNSIGNED_INT_SAMPLER_1D:
                case GL_UNSIGNED_INT_SAMPLER_2D:
                case GL_UNSIGNED_INT_SAMPLER_3D:
                case GL_UNSIGNED_INT_SAMPLER_CUBE:
                case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
                case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_BUFFER:
                case GL_UNSIGNED_INT_SAMPLER_2D_RECT:

                case GL_IMAGE_1D:
                case GL_IMAGE_2D:
                case GL_IMAGE_3D:
                case GL_IMAGE_2D_RECT:
                case GL_IMAGE_CUBE:
                case GL_IMAGE_BUFFER:
                case GL_IMAGE_1D_ARRAY:
                case GL_IMAGE_2D_ARRAY:
                case GL_IMAGE_2D_MULTISAMPLE:
                case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
                case GL_INT_IMAGE_1D:
                case GL_INT_IMAGE_2D:
                case GL_INT_IMAGE_3D:
                case GL_INT_IMAGE_2D_RECT:
                case GL_INT_IMAGE_CUBE:
                case GL_INT_IMAGE_BUFFER:
                case GL_INT_IMAGE_1D_ARRAY:
                case GL_INT_IMAGE_2D_ARRAY:
                case GL_INT_IMAGE_2D_MULTISAMPLE:
                case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_1D:
                case GL_UNSIGNED_INT_IMAGE_2D:
                case GL_UNSIGNED_INT_IMAGE_3D:
                case GL_UNSIGNED_INT_IMAGE_2D_RECT:
                case GL_UNSIGNED_INT_IMAGE_CUBE:
                case GL_UNSIGNED_INT_IMAGE_BUFFER:
                case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
                case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:

                case GL_INT:
                case GL_BOOL:
                {
                    int *pData = &((int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("%1").arg(pData[0])));
                    break;
                }
                case GL_INT_VEC2:
                case GL_BOOL_VEC2:
                {
                    int *pData = &((int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 2];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2 }").arg(pData[0]).arg(pData[1])));
                    break;
                }
                case GL_INT_VEC3:
                case GL_BOOL_VEC3:
                {
                    int *pData = &((int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 3];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3 }").arg(pData[0]).arg(pData[1]).arg(pData[2])));
                    break;
                }
                case GL_INT_VEC4:
                case GL_BOOL_VEC4:
                {
                    int *pData = &((int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 4];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3, %4 }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3])));
                    break;
                }
                case GL_UNSIGNED_INT:
                case GL_UNSIGNED_INT_ATOMIC_COUNTER: // TODO: is this correct?
                {
                    unsigned int *pData = &((unsigned int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("%1").arg(pData[0])));
                    break;
                }
                case GL_UNSIGNED_INT_VEC2:
                {
                    unsigned int *pData = &((unsigned int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 2];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2 }").arg(pData[0]).arg(pData[1])));
                    break;
                }
                case GL_UNSIGNED_INT_VEC3:
                {
                    unsigned int *pData = &((unsigned int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 3];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3 }").arg(pData[0]).arg(pData[1]).arg(pData[2])));
                    break;
                }
                case GL_UNSIGNED_INT_VEC4:
                {
                    unsigned int *pData = &((unsigned int *)uniformVec[i].m_data.get_ptr())[uniformElementIndex * 4];
                    ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(QString("{ %1, %2, %3, %4 }").arg(pData[0]).arg(pData[1]).arg(pData[2]).arg(pData[3])));
                    break;
                }
                default:
                {
                    VOGL_ASSERT_ALWAYS;
                    vogl_warning_printf("Unknown uniform type 0x%04X\n", uniformVec[i].m_type);
                    continue;
                }
            } // end switch

            rowIndex++;
        } // end uniform element index
    }

    const vogl_uniform_block_state_vec &uniformBlockVec = pProgramState->get_uniform_block_state_vec();
    for (uint i = 0; i < uniformBlockVec.size(); i++)
    {
        QString varName = QString("%1").arg(uniformBlockVec[i].m_name.c_str());

        ui->uniformTableWidget->insertRow(rowIndex);

        QStringList references;
        int referenced_by = uniformBlockVec[i].m_referenced_by;

        if (referenced_by & cBufferReferencedByVertex)
            references.append("vertex");
        if (referenced_by & cBufferReferencedByTesselationControl)
            references.append("tesselation control");
        if (referenced_by & cBufferReferencedByTesselationEvaluation)
            references.append("tesselation evaluation");
        if (referenced_by & cBufferReferencedByGeometry)
            references.append("geometry");
        if (referenced_by & cBufferReferencedByFragment)
            references.append("fragment");
        if (referenced_by & cBufferReferencedByCompute)
            references.append("compute");

        ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_location, new QTableWidgetItem(QString("%1").arg(i)));
        ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_name, new QTableWidgetItem(varName));
        ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_value, new QTableWidgetItem(references.join(", ")));
        ui->uniformTableWidget->setItem(rowIndex, vogleditor_utc_type, new QTableWidgetItem(QString("%1 bytes").arg(uniformBlockVec[i].m_uniform_block_data_size)));

        rowIndex++;
    }

    // resize columns so that they are properly sized, but still allow the user to resize them more if needed
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_location, QHeaderView::ResizeToContents);
    uint tmpWidth = ui->uniformTableWidget->horizontalHeader()->sectionSize(vogleditor_utc_location);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_location, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(vogleditor_utc_location, tmpWidth);

    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_name, QHeaderView::ResizeToContents);
    tmpWidth = ui->uniformTableWidget->horizontalHeader()->sectionSize(vogleditor_utc_name);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_name, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(vogleditor_utc_name, tmpWidth);

    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_type, QHeaderView::ResizeToContents);
    tmpWidth = ui->uniformTableWidget->horizontalHeader()->sectionSize(vogleditor_utc_type);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(vogleditor_utc_type, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(vogleditor_utc_type, tmpWidth);

    ui->saveShaderButton->setEnabled(false);
}
