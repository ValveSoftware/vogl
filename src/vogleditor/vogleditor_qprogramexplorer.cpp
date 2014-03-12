#include "vogleditor_qprogramexplorer.h"
#include "ui_vogleditor_qprogramexplorer.h"

#include "vogl_gl_object.h"
#include "vogl_program_state.h"

Q_DECLARE_METATYPE(vogl_program_state*);
Q_DECLARE_METATYPE(vogl_shader_state*);

vogleditor_QProgramExplorer::vogleditor_QProgramExplorer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vogleditor_QProgramExplorer)
{
    ui->setupUi(this);

    ui->saveShaderButton->setEnabled(false);
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
}

void vogleditor_QProgramExplorer::set_program_objects(vogl_gl_object_state_ptr_vec objects)
{
    clear();
    m_objects = objects;

    for (vogl_gl_object_state_ptr_vec::iterator iter = objects.begin(); iter != objects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTProgram)
        {
            vogl_program_state* pState = static_cast<vogl_program_state*>(*iter);

            QString valueStr;
            valueStr = valueStr.sprintf("Program %" PRIu64, pState->get_snapshot_handle());

            ui->programListBox->addItem(valueStr, QVariant::fromValue(pState));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in vogleditor_QProgramExplorer");
        }
    }
}

bool vogleditor_QProgramExplorer::set_active_program(unsigned long long programHandle)
{
    bool bActivated = false;
    int index = 0;
    for (vogl_gl_object_state_ptr_vec::iterator iter = m_objects.begin(); iter != m_objects.end(); iter++)
    {
        vogl_program_state* pState = static_cast<vogl_program_state*>(*iter);
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
        vogl_program_state* pObjState = ui->programListBox->itemData(index).value<vogl_program_state*>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL object type in vogleditor_QProgramExplorer");
            return;
        }

        vogl_shader_state_vec& attachedShaderVec = pObjState->get_shaders();
        vogl_shader_state_vec& linkedShaderVec = pObjState->get_link_time_snapshot()->get_shaders();

        QString valueStr;
        for (vogl_shader_state_vec::iterator linkedIter = linkedShaderVec.begin(); linkedIter != linkedShaderVec.end(); linkedIter++)
        {
            vogl_shader_state* pShaderState = linkedIter;
            valueStr = valueStr.sprintf("Linked Shader %" PRIu64 " - %s", pShaderState->get_snapshot_handle(), g_gl_enums.find_gl_name(pShaderState->get_shader_type()));
            ui->shaderListBox->addItem(valueStr, QVariant::fromValue(pShaderState));
        }

        for (vogl_shader_state_vec::iterator attachedIter = attachedShaderVec.begin(); attachedIter != attachedShaderVec.end(); attachedIter++)
        {
            vogl_shader_state* pShaderState = attachedIter;
            valueStr = valueStr.sprintf("Attached Shader %" PRIu64 " - %s", pShaderState->get_snapshot_handle(), g_gl_enums.find_gl_name(pShaderState->get_shader_type()));
            ui->shaderListBox->addItem(valueStr, QVariant::fromValue(pShaderState));
        }
    }
}

void vogleditor_QProgramExplorer::on_shaderListBox_currentIndexChanged(int index)
{
    ui->shaderTextEdit->clear();

    int count = ui->shaderListBox->count();
    if (index >= 0 && index < count)
    {
        vogl_shader_state* pObjState = ui->shaderListBox->itemData(index).value<vogl_shader_state*>();
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
        vogl_shader_state* pObjState = ui->shaderListBox->itemData(index).value<vogl_shader_state*>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL shader object type in vogleditor_QProgramExplorer");
            return;
        }

        pObjState->set_source(ui->shaderTextEdit->toPlainText().toStdString().c_str());

        ui->saveShaderButton->setEnabled(false);

        vogl_program_state* pProgramState = ui->programListBox->itemData(index).value<vogl_program_state*>();
        emit program_edited(pProgramState);
    }
}
