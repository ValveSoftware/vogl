#include "vogleditor_qshaderexplorer.h"
#include "ui_vogleditor_qshaderexplorer.h"

#include "vogl_gl_object.h"
#include "vogl_shader_state.h"

Q_DECLARE_METATYPE(vogl_shader_state*);

vogleditor_QShaderExplorer::vogleditor_QShaderExplorer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vogleditor_QShaderExplorer)
{
    ui->setupUi(this);
}

vogleditor_QShaderExplorer::~vogleditor_QShaderExplorer()
{
    delete ui;
}

void vogleditor_QShaderExplorer::clear()
{
    ui->shaderListbox->clear();
    ui->shaderTextEdit->clear();
}

void vogleditor_QShaderExplorer::set_shader_objects(vogl_gl_object_state_ptr_vec objects)
{
    clear();
    m_objects = objects;

    for (vogl_gl_object_state_ptr_vec::iterator iter = objects.begin(); iter != objects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTShader)
        {
            vogl_shader_state* pState = static_cast<vogl_shader_state*>(*iter);

            QString valueStr;
            valueStr = valueStr.sprintf("Shader %" PRIu64 " - %s", pState->get_snapshot_handle(), g_gl_enums.find_gl_name(pState->get_shader_type()));

            ui->shaderListbox->addItem(valueStr, QVariant::fromValue(pState));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in vogleditor_QShaderExplorer");
        }
    }
}


bool vogleditor_QShaderExplorer::set_active_shader(unsigned long long shaderHandle)
{
    bool bActivated = false;
    int index = 0;
    for (vogl_gl_object_state_ptr_vec::iterator iter = m_objects.begin(); iter != m_objects.end(); iter++)
    {
        vogl_shader_state* pState = static_cast<vogl_shader_state*>(*iter);
        if (pState->get_snapshot_handle() == shaderHandle)
        {
            ui->shaderListbox->setCurrentIndex(index);
            bActivated = true;
            break;
        }

        ++index;
    }
    return bActivated;
}

void vogleditor_QShaderExplorer::on_shaderListbox_currentIndexChanged(int index)
{
    ui->shaderTextEdit->clear();

    int count = ui->shaderListbox->count();
    if (index >= 0 && index < count)
    {
        vogl_shader_state* pObjState = ui->shaderListbox->itemData(index).value<vogl_shader_state*>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL shader object type in vogleditor_QShaderExplorer");
            return;
        }

        ui->shaderTextEdit->setText(pObjState->get_source().c_str());
    }
}
