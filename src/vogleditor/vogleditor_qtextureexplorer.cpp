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
#include "vogleditor_qtextureexplorer.h"

GCC_DIAGNOSTIC_PUSH()
GCC_DIAGNOSTIC_IGNORED(packed)
#include "ui_vogleditor_qtextureexplorer.h"
GCC_DIAGNOSTIC_POP()

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_texture_state.h"
#include "vogl_renderbuffer_state.h"
#include <QColorDialog>

Q_DECLARE_METATYPE(vogl_gl_object_state *);

vogleditor_QTextureExplorer::vogleditor_QTextureExplorer(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::vogleditor_QTextureExplorer)
{
    ui->setupUi(this);

    ui->textureScrollArea->setWidget(&m_textureViewer);

    ui->channelSelectionBox->addItem("RGBA", VOGL_CSO_RGBA);
    ui->channelSelectionBox->addItem("RGB", VOGL_CSO_RGB);
    ui->channelSelectionBox->addItem("R", VOGL_CSO_R);
    ui->channelSelectionBox->addItem("G", VOGL_CSO_G);
    ui->channelSelectionBox->addItem("B", VOGL_CSO_B);
    ui->channelSelectionBox->addItem("A", VOGL_CSO_A);

    ui->channelSelectionBox->addItem("1 - RGBA", VOGL_CSO_ONE_MINUS_RGBA);
    ui->channelSelectionBox->addItem("1 - RGB", VOGL_CSO_ONE_MINUS_RGB);
    ui->channelSelectionBox->addItem("1 - R", VOGL_CSO_ONE_MINUS_R);
    ui->channelSelectionBox->addItem("1 - G", VOGL_CSO_ONE_MINUS_G);
    ui->channelSelectionBox->addItem("1 - B", VOGL_CSO_ONE_MINUS_B);
    ui->channelSelectionBox->addItem("1 - A", VOGL_CSO_ONE_MINUS_A);

    ui->channelSelectionBox->addItem("1 / RGBA", VOGL_CSO_ONE_OVER_RGBA);
    ui->channelSelectionBox->addItem("1 / RGB", VOGL_CSO_ONE_OVER_RGB);
    ui->channelSelectionBox->addItem("1 / R", VOGL_CSO_ONE_OVER_R);
    ui->channelSelectionBox->addItem("1 / G", VOGL_CSO_ONE_OVER_G);
    ui->channelSelectionBox->addItem("1 / B", VOGL_CSO_ONE_OVER_B);
    ui->channelSelectionBox->addItem("1 / A", VOGL_CSO_ONE_OVER_A);

    connect(ui->textureObjectListbox, SIGNAL(currentIndexChanged(int)), this, SLOT(selectedTextureIndexChanged(int)));
    connect(ui->channelSelectionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelSelectionChanged(int)));
    connect(ui->alphaBlendColorButton, SIGNAL(clicked()), this, SLOT(alphaBlendButtonClicked()));

    // set default channel so that it doesn't alpha blend
    ui->channelSelectionBox->setCurrentIndex(VOGL_CSO_RGB);

    ui->zoomSpinBox->setValue(m_textureViewer.getZoomFactor());
}

vogleditor_QTextureExplorer::~vogleditor_QTextureExplorer()
{
    clear();
    delete ui;
}

void vogleditor_QTextureExplorer::clear()
{
    m_objects.clear();

    ui->textureObjectListbox->clear();

    m_textureViewer.clear();
    m_textureViewer.repaint();
}

void vogleditor_QTextureExplorer::set_zoom_factor(double zoomFactor)
{
    ui->zoomSpinBox->setValue(zoomFactor);
}

unsigned int vogleditor_QTextureExplorer::get_preferred_height() const
{
    // texture viewer height, plus extra space for the explorer UI
    return m_textureViewer.get_preferred_height() + ui->textureObjectListbox->height() * 2 + 50;
}

uint vogleditor_QTextureExplorer::set_texture_objects(vogl::vector<vogl_context_snapshot *> sharingContexts)
{
    clear();

    uint textureCount = 0;

    for (uint c = 0; c < sharingContexts.size(); c++)
    {
        vogl_gl_object_state_ptr_vec textureObjects;
        sharingContexts[c]->get_all_objects_of_category(cGLSTTexture, textureObjects);

        textureCount += add_texture_objects(textureObjects);
    }

    return textureCount;
}

uint vogleditor_QTextureExplorer::set_renderbuffer_objects(vogl::vector<vogl_context_snapshot *> sharingContexts)
{
    clear();

    uint textureCount = 0;

    for (uint c = 0; c < sharingContexts.size(); c++)
    {
        vogl_gl_object_state_ptr_vec renderbufferObjects;
        sharingContexts[c]->get_all_objects_of_category(cGLSTRenderbuffer, renderbufferObjects);

        textureCount += add_texture_objects(renderbufferObjects);
    }

    return textureCount;
}

uint vogleditor_QTextureExplorer::set_texture_objects(vogl_gl_object_state_ptr_vec textureObjects)
{
    clear();

    return add_texture_objects(textureObjects);
}

uint vogleditor_QTextureExplorer::add_texture_objects(vogl_gl_object_state_ptr_vec textureObjects)
{
    uint textureCount = 0;

    for (vogl_gl_object_state_ptr_vec::iterator iter = textureObjects.begin(); iter != textureObjects.end(); iter++)
    {
        ++textureCount;
        m_objects.push_back(*iter);

        if ((*iter)->get_type() == cGLSTTexture)
        {
            vogl_texture_state *pTexState = static_cast<vogl_texture_state *>(*iter);

            QString valueStr;
            valueStr = valueStr.sprintf("Texture %" PRIu64 " - %s (%u samples) (%u x %u x %u) %s", pTexState->get_snapshot_handle(), get_gl_enums().find_name(pTexState->get_target()), pTexState->get_num_samples(), pTexState->get_texture().get_width(), pTexState->get_texture().get_height(), pTexState->get_texture().get_depth(), get_gl_enums().find_name(pTexState->get_texture().get_ogl_internal_fmt()));

            ui->textureObjectListbox->addItem(valueStr, QVariant::fromValue(*iter));
        }
        else if ((*iter)->get_type() == cGLSTRenderbuffer)
        {
            vogl_renderbuffer_state *pRbState = static_cast<vogl_renderbuffer_state *>(*iter);

            QString valueStr;
            valueStr = valueStr.sprintf("Renderbuffer %" PRIu64 " - %s (%u samples) (%u x %u) %s", pRbState->get_snapshot_handle(), "GL_RENDERBUFFER", pRbState->get_texture().get_num_samples(), pRbState->get_desc().get_int_or_default(GL_RENDERBUFFER_WIDTH), pRbState->get_desc().get_int_or_default(GL_RENDERBUFFER_HEIGHT), get_gl_enums().find_name(pRbState->get_desc().get_int_or_default(GL_RENDERBUFFER_INTERNAL_FORMAT)));

            ui->textureObjectListbox->addItem(valueStr, QVariant::fromValue(*iter));
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in TextureExplorer");
        }
    }

    return textureCount;
}

uint vogleditor_QTextureExplorer::add_texture_object(vogl_texture_state &textureState, vogl::dynamic_string bufferType)
{
    m_objects.push_back(&textureState);

    QString valueStr;
    valueStr = valueStr.sprintf("%s (%u x %u) %s", bufferType.c_str(), textureState.get_texture().get_width(), textureState.get_texture().get_height(), get_gl_enums().find_name(textureState.get_texture().get_ogl_internal_fmt()));

    ui->textureObjectListbox->addItem(valueStr, QVariant::fromValue((vogl_gl_object_state *)&textureState));
    return 1;
}

bool vogleditor_QTextureExplorer::set_active_texture(unsigned long long textureHandle)
{
    bool bDisplayedTexture = false;
    int index = 0;
    for (vogl_gl_object_state_ptr_vec::iterator iter = m_objects.begin(); iter != m_objects.end(); iter++)
    {
        vogl_texture_state *pTexState = static_cast<vogl_texture_state *>(*iter);
        if (pTexState->get_snapshot_handle() == textureHandle)
        {
            ui->textureObjectListbox->setCurrentIndex(index);
            bDisplayedTexture = true;
            break;
        }

        ++index;
    }
    return bDisplayedTexture;
}

void vogleditor_QTextureExplorer::selectedTextureIndexChanged(int index)
{
    int count = ui->textureObjectListbox->count();
    if (index >= 0 && index < count)
    {
        vogl_gl_object_state *pObjState = ui->textureObjectListbox->itemData(index).value<vogl_gl_object_state *>();
        if (pObjState == NULL)
        {
            VOGL_ASSERT(!"NULL object type in TextureExplorer");
            return;
        }

        vogl_texture_state *pTexState = NULL;

        if (pObjState->get_type() == cGLSTTexture)
        {
            pTexState = static_cast<vogl_texture_state *>(pObjState);
        }
        else if (pObjState->get_type() == cGLSTRenderbuffer)
        {
            vogl_renderbuffer_state *pRbState = static_cast<vogl_renderbuffer_state *>(pObjState);
            if (pRbState != NULL)
            {
                pTexState = &(pRbState->get_texture());
            }
        }
        else
        {
            VOGL_ASSERT(!"Unhandled object type in TextureExplorer");
        }

        if (pTexState != NULL)
        {
            // Update array element spin box
            uint maxArrayElement = pTexState->get_texture().get_array_size();
            if (maxArrayElement <= 1)
            {
                ui->arrayElementSpinBox->setEnabled(false);
                ui->arrayElementSpinBox->setMaximum(0);

                // simulate that the value has changed to select the first (only) element
                on_arrayElementSpinBox_valueChanged(0);
            }
            else
            {
                ui->arrayElementSpinBox->setEnabled(true);

                int arrayElement = ui->arrayElementSpinBox->value();
                ui->arrayElementSpinBox->setMaximum(maxArrayElement - 1);

                // if the value is still at the same element after setting the max, then the
                // valueChanged signal will not be emitted, so we'll need to simulate that
                // in order to update and display the correct array element.
                if (ui->arrayElementSpinBox->value() == (int)arrayElement)
                {
                    on_arrayElementSpinBox_valueChanged(arrayElement);
                }
            }

            // Update slice spin box
            uint maxSlice = pTexState->get_texture().get_depth();
            if (maxSlice <= 1)
            {
                ui->sliceSpinBox->setEnabled(false);
                ui->sliceSpinBox->setMaximum(0);

                // simulate that the value has changed to select the first (only) element
                on_sliceSpinBox_valueChanged(0);
            }
            else
            {
                ui->sliceSpinBox->setEnabled(true);

                int slice = ui->sliceSpinBox->value();
                ui->sliceSpinBox->setMaximum(maxSlice - 1);

                // if the value is still at the same slice after setting the max, then the
                // valueChanged signal will not be emitted, so we'll need to simulate that
                // in order to update and display the correct slice.
                if (ui->sliceSpinBox->value() == (int)slice)
                {
                    on_sliceSpinBox_valueChanged(slice);
                }
            }

            // Update sample spin box
            uint maxSample = pTexState->get_num_samples();
            if (maxSample <= 1)
            {
                ui->sampleSpinBox->setEnabled(false);
                ui->sampleSpinBox->setMaximum(0);

                // simulate that the value has changed to select the first (only) sample
                on_sampleSpinBox_valueChanged(0);
            }
            else
            {
                ui->sampleSpinBox->setEnabled(true);

                int sample = ui->sampleSpinBox->value();
                ui->sampleSpinBox->setMaximum(maxSample - 1);

                // if the value is still at the same sample after setting the max, then the
                // valueChanged signal will not be emitted, so we'll need to simulate that
                // in order to update and display the new texture.
                if (ui->sampleSpinBox->value() == (int)sample)
                {
                    on_sampleSpinBox_valueChanged(sample);
                }
            }
        }
    }
}

void vogleditor_QTextureExplorer::channelSelectionChanged(int index)
{
    ChannelSelectionOption channelSelection = (ChannelSelectionOption)ui->channelSelectionBox->itemData(index).toInt();
    m_textureViewer.setChannelSelectionOption(channelSelection);
    bool bAlphaBlending = (channelSelection == VOGL_CSO_RGBA ||
                           channelSelection == VOGL_CSO_ONE_MINUS_RGBA ||
                           channelSelection == VOGL_CSO_ONE_OVER_RGBA);

    ui->alphaBlendColorButton->setEnabled(bAlphaBlending);
}

void vogleditor_QTextureExplorer::alphaBlendButtonClicked()
{
    QColor newColor = QColorDialog::getColor(m_textureViewer.getBackgroundColor(), this, "Select an alpha blend color");
    m_textureViewer.setBackgroundColor(QBrush(newColor));
}

void vogleditor_QTextureExplorer::on_zoomSpinBox_valueChanged(double zoomFactor)
{
    m_textureViewer.setZoomFactor(zoomFactor);
    emit zoomFactorChanged(zoomFactor);
}

void vogleditor_QTextureExplorer::on_pushButton_toggled(bool checked)
{
    m_textureViewer.setInverted(checked);
}

void vogleditor_QTextureExplorer::on_arrayElementSpinBox_valueChanged(int index)
{
    m_textureViewer.setArrayElement(index);
}

void vogleditor_QTextureExplorer::on_sampleSpinBox_valueChanged(int sample)
{
    vogl_gl_object_state *pObjState = ui->textureObjectListbox->itemData(ui->textureObjectListbox->currentIndex()).value<vogl_gl_object_state *>();
    if (pObjState == NULL)
    {
        VOGL_ASSERT(!"NULL object type in TextureExplorer");
        return;
    }

    vogl_texture_state *pTexState = NULL;

    if (pObjState->get_type() == cGLSTTexture)
    {
        pTexState = static_cast<vogl_texture_state *>(pObjState);
    }
    else if (pObjState->get_type() == cGLSTRenderbuffer)
    {
        vogl_renderbuffer_state *pRbState = static_cast<vogl_renderbuffer_state *>(pObjState);
        if (pRbState != NULL)
        {
            pTexState = &(pRbState->get_texture());
        }
    }
    else
    {
        VOGL_ASSERT(!"Unhandled object type in TextureExplorer");
    }

    if (pTexState != NULL)
    {
        const vogl::ktx_texture *pKTXTexture = &(pTexState->get_texture(sample));

        uint baseLevel = pTexState->get_params().get_value<uint>(GL_TEXTURE_BASE_LEVEL);
        uint maxLevel = pTexState->get_params().get_value<uint>(GL_TEXTURE_MAX_LEVEL);
        m_textureViewer.setTexture(pKTXTexture, baseLevel, maxLevel);
        m_textureViewer.repaint();
    }
}

void vogleditor_QTextureExplorer::on_sliceSpinBox_valueChanged(int slice)
{
    m_textureViewer.setSliceIndex(slice);
}
