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
#include "vogleditor_qshaderexplorer.h"

GCC_DIAGNOSTIC_PUSH()
GCC_DIAGNOSTIC_IGNORED(packed)
#include "ui_vogleditor_qshaderexplorer.h"
GCC_DIAGNOSTIC_POP()

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_shader_state.h"

Q_DECLARE_METATYPE(vogl_shader_state *);

vogleditor_QShaderExplorer::vogleditor_QShaderExplorer(QWidget *parent)
    : QWidget(parent),
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
    m_objects.clear();
}

uint vogleditor_QShaderExplorer::set_shader_objects(vogl::vector<vogl_context_snapshot *> sharingContexts)
{
    clear();

    uint shaderCount = 0;
    for (uint c = 0; c < sharingContexts.size(); c++)
    {
        vogl_gl_object_state_ptr_vec shaderObjects;
        sharingContexts[c]->get_all_objects_of_category(cGLSTShader, shaderObjects);

        shaderCount += add_shader_objects(shaderObjects);
    }

    return shaderCount;
}

uint vogleditor_QShaderExplorer::add_shader_objects(vogl_gl_object_state_ptr_vec objects)
{
    m_objects.append(objects);

    for (vogl_gl_object_state_ptr_vec::iterator iter = objects.begin(); iter != objects.end(); iter++)
    {
        if ((*iter)->get_type() == cGLSTShader)
        {
            vogl_shader_state *pState = static_cast<vogl_shader_state *>(*iter);

            QString valueStr;
            valueStr = valueStr.sprintf("Shader %" PRIu64 " - %s", pState->get_snapshot_handle(), get_gl_enums().find_gl_name(pState->get_shader_type()));

            ui->shaderListbox->addItem(valueStr, QVariant::fromValue(pState));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in vogleditor_QShaderExplorer");
        }
    }

    return objects.size();
}

bool vogleditor_QShaderExplorer::set_active_shader(unsigned long long shaderHandle)
{
    bool bActivated = false;
    int index = 0;
    for (vogl_gl_object_state_ptr_vec::iterator iter = m_objects.begin(); iter != m_objects.end(); iter++)
    {
        vogl_shader_state *pState = static_cast<vogl_shader_state *>(*iter);
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
        vogl_shader_state *pObjState = ui->shaderListbox->itemData(index).value<vogl_shader_state *>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL shader object type in vogleditor_QShaderExplorer");
            return;
        }

        ui->shaderTextEdit->setText(pObjState->get_source().c_str());
    }
}
