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

#ifndef VOGLEDITOR_QTEXTUREVIEWER_H
#define VOGLEDITOR_QTEXTUREVIEWER_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QPainter;
class QPaintEvent;
QT_END_NAMESPACE

#include <QBrush>
#include <QPen>

#include "vogl_core.h"
#include "vogl_map.h"
#include "vogl_image.h"
#include "vogl_ktx_texture.h"

typedef enum ChannelSelectionOptions
{
    VOGL_CSO_RGBA = 0,
    VOGL_CSO_RGB,
    VOGL_CSO_R,
    VOGL_CSO_G,
    VOGL_CSO_B,
    VOGL_CSO_A,
    VOGL_CSO_ONE_MINUS_RGBA,
    VOGL_CSO_ONE_MINUS_RGB,
    VOGL_CSO_ONE_MINUS_R,
    VOGL_CSO_ONE_MINUS_G,
    VOGL_CSO_ONE_MINUS_B,
    VOGL_CSO_ONE_MINUS_A,
    VOGL_CSO_ONE_OVER_RGBA,
    VOGL_CSO_ONE_OVER_RGB,
    VOGL_CSO_ONE_OVER_R,
    VOGL_CSO_ONE_OVER_G,
    VOGL_CSO_ONE_OVER_B,
    VOGL_CSO_ONE_OVER_A,
} ChannelSelectionOption;

class QTextureViewer : public QWidget
{
    Q_OBJECT
public:
    explicit QTextureViewer(QWidget *parent = 0);
    void paint(QPainter *painter, QPaintEvent *event);

    void setTexture(const vogl::ktx_texture *pTexture, uint baseMipLevel, uint maxMipLevel);

    void setChannelSelectionOption(ChannelSelectionOption channels)
    {
        if (m_channelSelection != channels)
        {
            delete_pixmaps();
            m_channelSelection = channels;
            repaint();
        }
    }

    void deleteTexture()
    {
        for (vogl::vector<vogl::image_u8 *>::iterator it = m_image.begin(); it < m_image.end(); ++it)
        {
            vogl::image_u8 *img = *it;
            img->clear();
            vogl_delete(img);
        }
        m_image.clear();
    }

    void clear()
    {
        m_draw_enabled = false;
        m_pKtxTexture = NULL;
        deleteTexture();
    }

    inline QColor getBackgroundColor() const
    {
        return m_background.color();
    }

    inline void setBackgroundColor(QBrush color)
    {
        m_draw_enabled = true;
        m_background = color;
        delete_pixmaps();
        repaint();
    }

    double getZoomFactor() const
    {
        return m_zoomFactor;
    }
    void setZoomFactor(double zoomFactor)
    {
        if (m_zoomFactor != zoomFactor)
        {
            m_zoomFactor = zoomFactor;
            repaint();
        }
    }

    bool getInverted() const
    {
        return m_bInvert;
    }
    void setInverted(bool bInvert)
    {
        if (m_bInvert != bInvert)
        {
            m_bInvert = bInvert;
            repaint();
        }
    }

    uint get_preferred_height() const
    {
        uint height = 0;
        if (m_pKtxTexture != NULL)
        {
            height += (m_pKtxTexture->get_height() * m_zoomFactor);

            // is it a cubemap?
            if (m_pKtxTexture->get_num_faces() == 6)
            {
                height *= 3;
            }
        }
        return height;
    }

    void setArrayElement(uint arrayElementIndex)
    {
        m_arrayIndex = arrayElementIndex;
        delete_pixmaps();
        repaint();
    }

    void setSliceIndex(uint sliceIndex)
    {
        m_sliceIndex = sliceIndex;
        delete_pixmaps();
        repaint();
    }

private:
    bool m_draw_enabled;
    QBrush m_background;
    QPen m_outlinePen;
    ChannelSelectionOption m_channelSelection;
    double m_zoomFactor;
    bool m_bInvert;

    vogl::map<uint, QPixmap> m_pixmaps;
    vogl::map<uint, vogl::color_quad_u8 *> m_pixmapData;

    const vogl::ktx_texture *m_pKtxTexture;
    vogl::vector<vogl::image_u8 *> m_image;
    uint m_baseMipLevel;
    uint m_maxMipLevel;
    uint m_arrayIndex;
    uint m_sliceIndex;

    void delete_pixmaps()
    {
        m_pixmaps.clear();
        for (vogl::map<uint, vogl::color_quad_u8 *>::iterator iter = m_pixmapData.begin(); iter != m_pixmapData.end(); iter++)
        {
            delete[] iter->second;
            iter->second = NULL;
        }
        m_pixmapData.clear();
    }

    void adjustChannels(ChannelSelectionOption selection, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a);

protected:
    void paintEvent(QPaintEvent *event);

signals:

public
slots:
};

#endif // VOGLEDITOR_QTEXTUREVIEWER_H
