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

#include <QtGui>
#include "vogleditor_qtextureviewer.h"
#include "vogl_buffer_stream.h"
#include "pxfmt.h"

QTextureViewer::QTextureViewer(QWidget *parent)
    : QWidget(parent),
      m_channelSelection(VOGL_CSO_RGBA),
      m_zoomFactor(1),
      m_bInvert(false),
      m_pKtxTexture(NULL),
      m_baseMipLevel(0),
      m_maxMipLevel(0),
      m_arrayIndex(0)
{
    m_background = QBrush(QColor(0, 0, 0));
    m_outlinePen = QPen(Qt::black);
    m_outlinePen.setWidth(1);
}

void QTextureViewer::setTexture(const vogl::ktx_texture *pTexture, uint baseMipLevel, uint maxMipLevel)
{
    pxfmt_sized_format src_pxfmt;
    pxfmt_sized_format dest_pxfmt = PXFMT_RGBA8_UNORM;
    bool has_red;
    bool has_green;
    bool has_blue;
    bool has_alpha;
    bool has_depth;
    bool has_stencil;
    bool has_large_components;
    bool is_floating_point;
    bool is_integer;
    bool is_compressed;
    uint bytes_per_pixel;
    uint bytes_per_compressed_block;
    uint block_size;

    deleteTexture();
    if (!pTexture->is_valid())
        return;

    if (pTexture->is_compressed())
    {
        src_pxfmt = validate_internal_format(pTexture->get_ogl_internal_fmt());
        if (src_pxfmt == PXFMT_INVALID)
        {
            vogl_error_printf("validate_internal_format() returned an unsupported KTX compressed format: 0x%X\n",
                              pTexture->get_ogl_internal_fmt());
            return;
        }
    }
    else
    {
        src_pxfmt = validate_format_type_combo(pTexture->get_ogl_fmt(),
                                               pTexture->get_ogl_type());
        if (src_pxfmt == PXFMT_INVALID)
        {
            vogl_error_printf("validate_format_type_combo() returned an unsupported KTX format/type: 0x%X 0x%X\n",
                              pTexture->get_ogl_fmt(), pTexture->get_ogl_type());
            return;
        }
    }

    // Get information about the source:
    query_pxfmt_sized_format(src_pxfmt, &has_red, &has_green, &has_blue, &has_alpha,
                             &has_depth, &has_stencil, &has_large_components, &is_floating_point,
                             &is_integer, &is_compressed, &bytes_per_pixel,
                             &bytes_per_compressed_block, &block_size);

    for (uint mip_level = 0; mip_level < pTexture->get_num_mips(); mip_level++)
    {
        uint mip_width = vogl::math::maximum<uint>(1U, pTexture->get_width() >> mip_level);
        uint mip_height = vogl::math::maximum<uint>(1U, pTexture->get_height() >> mip_level);
        uint mip_depth = vogl::math::maximum<uint>(1U, pTexture->get_depth() >> mip_level);
        for (uint array_index = 0; array_index < pTexture->get_array_size(); array_index++)
        {
            for (uint face_index = 0; face_index < pTexture->get_num_faces(); face_index++)
            {
                for (uint zslice_index = 0; zslice_index < mip_depth; zslice_index++)
                {
                    pxfmt_conversion_status status;

                    uint image_index = pTexture->get_image_index(mip_level, array_index,
                                                                 face_index, zslice_index);
                    if (image_index >= pTexture->get_num_images())
                        continue;

                    const vogl::uint8_vec &image_data = pTexture->get_image_data(image_index);
                    if (image_data.is_empty())
                        continue;

                    size_t src_size = bytes_per_pixel * mip_width * mip_height;
                    uint dest_size = mip_width * sizeof(uint) * mip_height;
                    vogl::image_u8 *dest_image = vogl_new(vogl::image_u8, mip_width, mip_height);
                    if (image_index >= m_image.size())
                    {
                        m_image.resize(image_index + 1);
                    }
                    m_image[image_index] = dest_image;

                    if (is_compressed)
                    {
                        // FIXME/TBD/TODO: CONSIDER MERGING THIS FUNCTION BACK
                        // INTO THE NEXT FUNCTION (i.e. have one function for
                        // both compressed and non-compressed src data):
                        status = pxfmt_decompress_pixels(dest_image->get_ptr(), image_data.get_ptr(),
                                                         mip_width, mip_height,
                                                         dest_pxfmt, src_pxfmt, dest_size, src_size);
                    }
                    else
                    {
                        status = pxfmt_convert_pixels(dest_image->get_ptr(), image_data.get_ptr(),
                                                      mip_width, mip_height,
                                                      dest_pxfmt, src_pxfmt, dest_size, src_size);
                    }

                    if (status != PXFMT_CONVERSION_SUCCESS)
                    {
                        vogl_error_printf("pxfmt_convert_pixels() returned a non-success status of %d!\n", status);
                        return;
                    }

                    dest_image->flip_y();
                } // zslice
            }     // face
        }         // array
    }             // mip_level

    delete_pixmaps();
    m_draw_enabled = true;
    m_pKtxTexture = pTexture;
    m_baseMipLevel = baseMipLevel;
    m_maxMipLevel = maxMipLevel;
}

void QTextureViewer::paintEvent(QPaintEvent *event)
{
    QPainter painter;
    painter.begin(this);

    if (m_draw_enabled == false)
    {
        // clear the viewer
        painter.fillRect(event->rect(), QWidget::palette().color(QWidget::backgroundRole()));
    }
    else
    {
        paint(&painter, event);
    }
    painter.end();
}

void QTextureViewer::paint(QPainter *painter, QPaintEvent *event)
{
    VOGL_NOTE_UNUSED(event);
    if (m_pKtxTexture == NULL)
    {
        return;
    }

    if (m_image.is_empty() || !m_image[0]->is_valid() || !m_pKtxTexture->get_num_mips())
    {
        return;
    }

    painter->save();

    const uint border = 25;
    const uint minDimension = 10;
    uint texWidth = m_pKtxTexture->get_width();
    uint texHeight = m_pKtxTexture->get_height();

    uint drawWidth = vogl::math::maximum<uint>(minDimension, texWidth);
    uint drawHeight = vogl::math::maximum<uint>(minDimension, texHeight);

    // offset by 1 so that there is room to draw a border around the biggest mip level
    painter->translate(1, 1);

    // apply zoom factor using scaling
    painter->scale(m_zoomFactor, m_zoomFactor);

    painter->setPen(m_outlinePen);

    uint numMips = m_pKtxTexture->get_num_mips();
    uint maxMip = vogl::math::minimum(numMips, m_maxMipLevel);

    maxMip = vogl::math::minimum(maxMip, m_pKtxTexture->get_num_mips() - 1);

    uint mipWidth = 0;
    uint mipHeight = 0;
    uint mipDepth;

    drawWidth = drawWidth >> m_baseMipLevel;
    drawHeight = drawHeight >> m_baseMipLevel;

    if (m_pKtxTexture->get_num_faces() == 6)
    {
        // adjust draw dimensions
        drawWidth *= 4;
        drawHeight *= 3;
    }

    uint minimumWidth = 0;
    uint minimumHeight = drawHeight + border;

    for (uint mip = m_baseMipLevel; mip <= maxMip; mip++)
    {
        uint imageIndex = m_pKtxTexture->get_image_index(mip, m_arrayIndex, 0, m_sliceIndex);

        mipDepth = vogl::math::maximum<uint>(1U, m_pKtxTexture->get_depth() >> mip);
        if (mipDepth <= m_sliceIndex)
            break;
        if (m_pixmaps.contains(mip) == false && m_image[imageIndex] && m_image[imageIndex]->is_valid())
        {
            QWidget *pParent = (QWidget *)this->parent();
            QCursor origCursor = pParent->cursor();
            pParent->setCursor(Qt::WaitCursor);

            vogl::color_quad_u8 *pTmpPixels = NULL;
            if (m_pKtxTexture->get_num_faces() == 6)
            {
                vogl::image_u8 *image = m_image[imageIndex];

                mipWidth = image->get_width();
                mipHeight = image->get_height();

                unsigned char tmp = 0;
                unsigned int pixelsSize = image->get_total_pixels();

                // store face dimensions
                uint faceWidth = mipWidth;
                uint faceHeight = mipHeight;

                uint cubeCrossStride = faceWidth * 4;

                // adjust mip dimensions
                mipWidth *= 4;
                mipHeight *= 3;

                pTmpPixels = new vogl::color_quad_u8[pixelsSize * 12];
                memset(pTmpPixels, 0, pixelsSize * 12 * sizeof(vogl::color_quad_u8));

                // order is +X, -X, +Y, -Y, +Z, -Z
                uint faceRowOffset[6] = { faceHeight, faceHeight, 0, 2 * faceHeight, faceHeight, faceHeight };
                uint faceColOffset[6] = { 0, 2 * faceWidth, faceWidth, faceWidth, 3 * faceWidth, faceWidth };

                for (uint face = 0; face < m_pKtxTexture->get_num_faces(); face++)
                {
                    vogl::image_u8 *image2 = m_image[m_pKtxTexture->get_image_index(mip, m_arrayIndex, face, 0)];
                    vogl::color_quad_u8 *pPixels = image2->get_pixels();

                    // calculate write location to start of face
                    vogl::color_quad_u8 *writeLoc = pTmpPixels + cubeCrossStride * faceRowOffset[face] + faceColOffset[face];

                    for (uint h = 0; h < faceHeight; h++)
                    {
                        // copy row of face into cross
                        memcpy(writeLoc + h * cubeCrossStride, pPixels + h * faceWidth, faceWidth * sizeof(vogl::color_quad_u8));

                        // adjust color of each pixel
                        for (uint w = 0; w < faceWidth; w++)
                        {
                            vogl::color_quad_u8 *pPixel = writeLoc + h * cubeCrossStride + w;
                            adjustChannels(m_channelSelection, pPixel->r, pPixel->g, pPixel->b, pPixel->a);
                            tmp = pPixel->r;
                            pPixel->r = pPixel->b;
                            pPixel->b = tmp;
                        }
                    }
                }
            }
            else
            {
                vogl::image_u8 *image = m_image[imageIndex];
                vogl::color_quad_u8 *pPixels = image->get_pixels();

                mipWidth = image->get_width();
                mipHeight = image->get_height();

                unsigned char tmp = 0;
                unsigned int pixelsSize = image->get_total_pixels();

                pTmpPixels = new vogl::color_quad_u8[pixelsSize];
                memcpy(pTmpPixels, pPixels, pixelsSize * sizeof(vogl::color_quad_u8));
                for (uint i = 0; i < pixelsSize; i++)
                {
                    adjustChannels(m_channelSelection, pTmpPixels[i].r, pTmpPixels[i].g, pTmpPixels[i].b, pTmpPixels[i].a);
                    tmp = pTmpPixels[i].r;
                    pTmpPixels[i].r = pTmpPixels[i].b;
                    pTmpPixels[i].b = tmp;
                }
            }

            m_pixmaps.insert(mip, QPixmap::fromImage(QImage((unsigned char *)pTmpPixels, mipWidth, mipHeight, QImage::Format_ARGB32_Premultiplied)));
            m_pixmapData.insert(mip, pTmpPixels);
            pParent->setCursor(origCursor);
        }

        // make sure the rect is 1 pixel around the texture
        painter->drawRect(-1, -1, drawWidth + 1, drawHeight + 1);

        if (m_pixmaps.contains(mip))
        {
            int left = 0;
            int top = 0;
            if (m_bInvert)
            {
                // invert
                painter->scale(1, -1);
                top = -(int)drawHeight;
            }

            painter->drawPixmap(left, top, drawWidth, drawHeight, m_pixmaps[mip]);

            if (m_bInvert)
            {
                // restore inversion
                painter->scale(1, -1);
            }
        }
        painter->translate(drawWidth + border, drawHeight / 2);

        minimumWidth += drawWidth + border;

        drawWidth /= 2;
        drawHeight /= 2;
    }

    this->setMinimumSize(minimumWidth * m_zoomFactor, minimumHeight * m_zoomFactor);

    painter->restore();
}

void QTextureViewer::adjustChannels(ChannelSelectionOption selection, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a)
{
    switch (selection)
    {
        case VOGL_CSO_RGBA:
        {
            // premultiply alpha, then force it to 255
            float blendFactor = a / 255.0;
            r = r * blendFactor + (m_background.color().red() * (1.0 - blendFactor));
            g = g * blendFactor + (m_background.color().green() * (1.0 - blendFactor));
            b = b * blendFactor + (m_background.color().blue() * (1.0 - blendFactor));
            a = 255;
            break;
        }
        case VOGL_CSO_RGB:
        {
            // leave rgb, but force a to 255
            a = 255;
            break;
        }
        case VOGL_CSO_R:
        {
            // leave r, and force gb to r so that it appears in greyscale
            g = r;
            b = r;
            a = 255;
            break;
        }
        case VOGL_CSO_G:
        {
            // leave g, and force ab to g so that it appears in greyscale
            r = g;
            b = g;
            a = 255;
            break;
        }
        case VOGL_CSO_B:
        {
            // leave b, and force ag to b so that it appears in greyscale
            r = b;
            g = b;
            a = 255;
            break;
        }
        case VOGL_CSO_A:
        {
            // broadcast a to rgb and then force a to 255
            r = a;
            g = a;
            b = a;
            a = 255;
            break;
        }
        case VOGL_CSO_ONE_MINUS_RGBA:
        {
            r = 255 - r;
            g = 255 - g;
            b = 255 - b;
            a = 255 - a;
            adjustChannels(VOGL_CSO_RGBA, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_MINUS_RGB:
        {
            r = 255 - r;
            g = 255 - g;
            b = 255 - b;
            a = 255 - a;
            adjustChannels(VOGL_CSO_RGB, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_MINUS_R:
        {
            r = 255 - r;
            adjustChannels(VOGL_CSO_R, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_MINUS_G:
        {
            g = 255 - g;
            adjustChannels(VOGL_CSO_G, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_MINUS_B:
        {
            b = 255 - b;
            adjustChannels(VOGL_CSO_B, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_MINUS_A:
        {
            a = 255 - a;
            adjustChannels(VOGL_CSO_A, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_OVER_RGBA:
        {
            r = (r == 0) ? 255 : (255 / r);
            g = (g == 0) ? 255 : (255 / g);
            b = (b == 0) ? 255 : (255 / b);
            a = (a == 0) ? 255 : (255 / a);
            adjustChannels(VOGL_CSO_RGBA, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_OVER_RGB:
        {
            r = (r == 0) ? 255 : (255 / r);
            g = (g == 0) ? 255 : (255 / g);
            b = (b == 0) ? 255 : (255 / b);
            adjustChannels(VOGL_CSO_RGB, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_OVER_R:
        {
            r = (r == 0) ? 255 : (255 / r);
            adjustChannels(VOGL_CSO_R, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_OVER_G:
        {
            g = (g == 0) ? 255 : (255 / g);
            adjustChannels(VOGL_CSO_G, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_OVER_B:
        {
            b = (b == 0) ? 255 : (255 / b);
            adjustChannels(VOGL_CSO_B, r, g, b, a);
            break;
        }
        case VOGL_CSO_ONE_OVER_A:
        {
            a = (a == 0) ? 255 : (255 / a);
            adjustChannels(VOGL_CSO_A, r, g, b, a);
            break;
        }
    }
}
