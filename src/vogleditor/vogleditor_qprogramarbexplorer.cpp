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
#include "vogleditor_qprogramarbexplorer.h"

GCC_DIAGNOSTIC_PUSH()
GCC_DIAGNOSTIC_IGNORED(packed)
#include "ui_vogleditor_qprogramarbexplorer.h"
GCC_DIAGNOSTIC_POP()

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_arb_program_state.h"

Q_DECLARE_METATYPE(vogl_arb_program_state *);

vogleditor_QProgramArbExplorer::vogleditor_QProgramArbExplorer(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::vogleditor_QProgramArbExplorer),
      m_pProgramEnvState(NULL),
      m_maxEnvParameters(0),
      m_maxLocalParameters(0)
{
    ui->setupUi(this);

    ui->saveShaderButton->setEnabled(false);
    ui->uniformTableWidget->setColumnCount(3);
    ui->uniformTableWidget->setHorizontalHeaderLabels(QStringList() << "Name"
                                                                    << "Value"
                                                                    << "Type");
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(0, 100);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(2, 100);
}

vogleditor_QProgramArbExplorer::~vogleditor_QProgramArbExplorer()
{
    delete ui;
}

void vogleditor_QProgramArbExplorer::clear()
{
    ui->programListBox->clear();
    ui->shaderTextEdit->clear();
    m_objects.clear();
    m_pProgramEnvState = NULL;
}

uint vogleditor_QProgramArbExplorer::set_program_objects(vogl::vector<vogl_context_snapshot *> sharingContexts)
{
    clear();

    m_pProgramEnvState = &(sharingContexts[0]->get_arb_program_environment_state());
    m_maxEnvParameters = sharingContexts[0]->get_context_info().get_max_arb_vertex_program_env_params();
    m_maxLocalParameters = (uint)sharingContexts[0]->get_general_state().get_value<int>(GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB);

    uint programCount = 0;
    for (uint c = 0; c < sharingContexts.size(); c++)
    {
        vogl_gl_object_state_ptr_vec programObjects;
        sharingContexts[c]->get_all_objects_of_category(cGLSTARBProgram, programObjects);

        programCount += add_program_objects(programObjects);
    }

    return programCount;
}

uint vogleditor_QProgramArbExplorer::add_program_objects(vogl_gl_object_state_ptr_vec objects)
{
    m_objects.append(objects);

    for (vogl_gl_object_state_ptr_vec::iterator iter = objects.begin(); iter != objects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTARBProgram)
        {
            vogl_arb_program_state *pState = static_cast<vogl_arb_program_state *>(*iter);

            QString valueStr;
            valueStr = valueStr.sprintf("Program %" PRIu64 " - %s", pState->get_snapshot_handle(), get_gl_enums().find_name(pState->get_target()));

            ui->programListBox->addItem(valueStr, QVariant::fromValue(pState));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in vogleditor_QProgramArbExplorer");
        }
    }

    return objects.size();
}

bool vogleditor_QProgramArbExplorer::set_active_program(unsigned long long programHandle)
{
    bool bActivated = false;
    int index = 0;
    for (vogl_gl_object_state_ptr_vec::iterator iter = m_objects.begin(); iter != m_objects.end(); iter++)
    {
        vogl_arb_program_state *pState = static_cast<vogl_arb_program_state *>(*iter);
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

void vogleditor_QProgramArbExplorer::on_programListBox_currentIndexChanged(int index)
{
    int count = ui->programListBox->count();
    if (index >= 0 && index < count)
    {
        vogl_arb_program_state *pObjState = ui->programListBox->itemData(index).value<vogl_arb_program_state *>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL object type in vogleditor_QProgramArbExplorer");
            return;
        }

        // the program strings may not be NULL terminated, so create the string this way to ensure it don't read past the end
        QString programString = QString::fromLocal8Bit((const char *)pObjState->get_program_string().get_const_ptr(), pObjState->get_program_string_size());
        ui->shaderTextEdit->setText(programString);

        update_uniforms_for_source(programString);

        ui->saveShaderButton->setEnabled(false);
    }
}

void vogleditor_QProgramArbExplorer::update_uniforms_for_source(const QString &programSource)
{
    vogl_arb_program_state *pObjState = ui->programListBox->itemData(ui->programListBox->currentIndex()).value<vogl_arb_program_state *>();

    // clear the table
    ui->uniformTableWidget->setRowCount(0);

    uint programEnvIndex = vogl_arb_program_environment_state::cVertexTarget;
    if (pObjState->get_target() == GL_FRAGMENT_PROGRAM_ARB)
    {
        programEnvIndex = vogl_arb_program_environment_state::cFragmentTarget;
    }

    const vec4F_vec &envParams = m_pProgramEnvState->get_env_params(programEnvIndex);
    uint rowIndex = 0;
    for (uint i = 0; i < m_maxEnvParameters; i++)
    {
        QString varName = QString("program.env[%1]").arg(i);
        if (programSource.contains(varName))
        {
            ui->uniformTableWidget->insertRow(rowIndex);
            ui->uniformTableWidget->setItem(rowIndex, 0, new QTableWidgetItem(varName));
            ui->uniformTableWidget->setItem(rowIndex, 1, new QTableWidgetItem(QString("{ %1, %2, %3, %4 }").arg(envParams[i][0]).arg(envParams[i][1]).arg(envParams[i][2]).arg(envParams[i][3])));
            ui->uniformTableWidget->setItem(rowIndex, 2, new QTableWidgetItem("float4"));

            rowIndex++;
        }
    }

    const vec4F_vec &localParams = pObjState->get_program_local_params();
    for (uint i = 0; i < m_maxLocalParameters; i++)
    {
        QString varName = QString("program.local[%1]").arg(i);
        if (programSource.contains(varName))
        {
            ui->uniformTableWidget->insertRow(rowIndex);
            ui->uniformTableWidget->setItem(rowIndex, 0, new QTableWidgetItem(varName));
            ui->uniformTableWidget->setItem(rowIndex, 1, new QTableWidgetItem(QString("{ %1, %2, %3, %4 }").arg(localParams[i][0]).arg(localParams[i][1]).arg(localParams[i][2]).arg(localParams[i][3])));
            ui->uniformTableWidget->setItem(rowIndex, 2, new QTableWidgetItem("float4"));

            rowIndex++;
        }
    }

    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    uint tmpWidth = ui->uniformTableWidget->horizontalHeader()->sectionSize(0);
    ui->uniformTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->uniformTableWidget->horizontalHeader()->resizeSection(0, tmpWidth);

    ui->saveShaderButton->setEnabled(false);
}

void vogleditor_QProgramArbExplorer::on_shaderTextEdit_textChanged()
{
    ui->saveShaderButton->setEnabled(true);
}

void vogleditor_QProgramArbExplorer::on_saveShaderButton_clicked()
{
    int index = ui->programListBox->currentIndex();
    if (index >= 0 && ui->programListBox->count() > 0)
    {
        vogl_arb_program_state *pProgramState = ui->programListBox->itemData(index).value<vogl_arb_program_state *>();
        if (pProgramState == NULL)
        {
            VOGL_ASSERT(!"NULL program object type in vogleditor_QProgramArbExplorer");
            return;
        }

        pProgramState->set_program_string(ui->shaderTextEdit->toPlainText().toStdString().c_str(), ui->shaderTextEdit->toPlainText().size());

        update_uniforms_for_source(ui->shaderTextEdit->toPlainText());

        ui->saveShaderButton->setEnabled(false);

        emit program_edited(pProgramState);
    }
}
