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

#include <QPainter>
#include <QPaintEvent>
#include "vogleditor_qtimelineview.h"
#include "vogleditor_qsettings.h"
#include "vogleditor_frameitem.h"
#include "vogleditor_groupitem.h"

vogleditor_QTimelineView::vogleditor_QTimelineView(QWidget *parent)
    : QWidget(parent),
      m_roundoff(cVOGL_TIMELINEOFFSET),
      m_curFrame(0),
      m_curApiCallNumber(0),
      m_pModel(NULL),
      m_zoom(1),
      m_scroll(0)
{
    QLinearGradient gradient(QPointF(0, 1), QPointF(0, 0));
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);

    gradient.setColorAt(0.0, Qt::white);
    gradient.setColorAt(1.0, QColor(0xa6, 0xce, 0x39));
    m_triangleBrushWhite = QBrush(gradient);

    gradient.setColorAt(0.0, Qt::black);
    gradient.setColorAt(1.0, QColor(0xa6, 0xce, 0x39));
    m_triangleBrushBlack = QBrush(gradient);

    QPalette Palette(palette());
    Palette.setColor(QPalette::Background, QColor(200, 200, 200));
    setAutoFillBackground(true);
    setPalette(Palette);
    m_trianglePen = QPen(Qt::black);
    m_trianglePen.setWidth(1);
    m_trianglePen.setJoinStyle(Qt::MiterJoin);
    m_trianglePen.setMiterLimit(3);
    m_textPen = QPen(Qt::white);
    m_textFont.setPixelSize(50);

    m_horizontalScale = 1;
    m_lineLength = 1;
}

vogleditor_QTimelineView::~vogleditor_QTimelineView()
{

}

void vogleditor_QTimelineView::paintEvent(QPaintEvent *event)
{
    QPainter painter;
    painter.begin(this);
    paint(&painter, event);
    painter.end();
}

void vogleditor_QTimelineView::mousePressEvent(QMouseEvent *event)
{
    if(event->buttons() & Qt::LeftButton)
    {
        m_mouseDragStartPos=event->x();
        m_mouseDragStartScroll=m_scroll;
    }
}

void vogleditor_QTimelineView::mouseMoveEvent(QMouseEvent *event)
{
    if(event->buttons() & Qt::LeftButton)
    {
        scrollToPx(m_mouseDragStartScroll+(m_mouseDragStartPos-event->x()));
    }
}

void vogleditor_QTimelineView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        float oldZoom=m_zoom;
        if (event->angleDelta().y()>0)
            m_zoom=m_zoom*(1.2f);
        else
            m_zoom=m_zoom/(1.2f);
        m_zoom = qMax(m_zoom, 1.0f);
        //Keep timeline stationary under the mouse:
        scrollToPx(((m_scroll+event->x())/oldZoom)*m_zoom-event->x());
    }else
    {
        scrollToPx(m_scroll-=event->angleDelta().y()/3);
    }
}


void vogleditor_QTimelineView::scrollToPx(int scroll)
{
    m_scroll=scroll;
    m_scroll=qMax(m_scroll, 0.0f);
    m_scroll=qMin(m_scroll, m_zoom*width()-width());
    update();
}

void vogleditor_QTimelineView::drawBaseTimeline(QPainter *painter, const QRect &rect, int gap)
{
    painter->save();

    // translate drawing to vertical center of rect
    painter->translate(0, rect.height() / 2);

    painter->setBrush(m_triangleBrushWhite);
    painter->setPen(m_trianglePen);

    // everything will have a small gap on the left and right sides
    painter->translate(gap, 0);

    // draw the actual timeline
    painter->drawLine(0, 0, m_lineLength, 0);

    painter->restore();
}

void vogleditor_QTimelineView::paint(QPainter *painter, QPaintEvent *event)
{
    int gap = 10;
    int arrowHeight = 10;
    int arrowTop = height() / 2 - gap - arrowHeight;
    int arrowHalfWidth = 3;
    m_lineLength = width()*m_zoom - 2 * gap;

    QPolygon triangle(3);
    triangle.setPoint(0, 0, arrowTop);
    triangle.setPoint(1, -arrowHalfWidth, arrowTop + arrowHeight);
    triangle.setPoint(2, arrowHalfWidth, arrowTop + arrowHeight);

    painter->translate(-m_scroll, 0);
    drawBaseTimeline(painter, event->rect(), gap);

    if (m_pModel == NULL)
    {
        return;
    }

    if (m_pModel->get_root_item() == NULL)
    {
        return;
    }

    // translate drawing to vertical center of rect
    // everything will have a small gap on the left and right sides
    painter->translate(gap, height() / 2);

    m_horizontalScale = (float)m_lineLength / (float)m_pModel->get_root_item()->getDuration();

    // we don't want to draw the root item, but all of its children
    int numChildren = m_pModel->get_root_item()->childCount();
    int i_height = height() / 2 - 2 * gap;

    painter->setBrush(m_triangleBrushWhite);
    painter->setPen(m_trianglePen);

    float minimumOffset = 0;
    vogleditor_timelineItem *rootItem = m_pModel->get_root_item();
    for (int c = 0; c < numChildren; c++)
    {
        vogleditor_timelineItem *pChild = rootItem->child(c);
        drawTimelineItem(painter, pChild, i_height, minimumOffset);
    }

    painter->setBrush(m_triangleBrushWhite);
    painter->setPen(m_trianglePen);

    bool bFoundFrame = false;
    bool bFoundApiCall = false;

    for (int c = 0; c < numChildren; c++)
    {
        vogleditor_timelineItem *pChild = rootItem->child(c);

        // draw current frame marker
        if (bFoundFrame == false && pChild->getFrameItem() != NULL && pChild->getFrameItem()->frameNumber() == m_curFrame)
        {
            painter->save();
            painter->setBrush(m_triangleBrushBlack);
            painter->translate(scalePositionHorizontally(pChild->getBeginTime()), 0);
            painter->drawPolygon(triangle);
            painter->restore();
            bFoundFrame = true;
        }

        // draw current api call marker
        bFoundApiCall = drawCurrentApiCallMarker(painter, triangle, pChild);

        if (bFoundFrame && bFoundApiCall)
            break;
    }
}

bool vogleditor_QTimelineView::drawCurrentApiCallMarker(QPainter *painter,
                                                        QPolygon &triangle,
                                                        vogleditor_timelineItem *pItem)
{
    bool bRetVal = false;

    if (pItem->isApiCallItem() || pItem->isGroupItem())
    {
        unsigned long long callNumber;
        if (pItem->isApiCallItem())
        {
            callNumber = pItem->getApiCallItem()->globalCallIndex();
        }
        else
        {
            callNumber = pItem->getGroupItem()->firstApiCallIndex();
        }

        if (callNumber == m_curApiCallNumber)
        {
            float xpos = scalePositionHorizontally(pItem->getBeginTime());
            xpos -= m_roundoff;

            painter->save();
            painter->translate(xpos, 0);
            painter->drawPolygon(triangle);
            painter->restore();
            bRetVal = true;
        }
        else
        {
            // check children
            int numChildren = pItem->childCount();
            for (int c = 0; c < numChildren; c++)
            {
                if ((bRetVal = drawCurrentApiCallMarker(painter, triangle, pItem->child(c))))
                {
                    break;
                }
            }
        }
    }
    return bRetVal;
}

float vogleditor_QTimelineView::scaleDurationHorizontally(float value)
{
    float scaled = value * m_horizontalScale;
    if (scaled <= m_horizontalScale)
    {
        scaled = m_horizontalScale;
    }

    return scaled;
}

float vogleditor_QTimelineView::scalePositionHorizontally(float value)
{
    float horizontalShift = m_pModel->get_root_item()->getBeginTime();
    float horizontalLength = m_pModel->get_root_item()->getDuration();
    float offset = ((value - horizontalShift) / horizontalLength) * m_lineLength;

    return offset;
}

void vogleditor_QTimelineView::drawTimelineItem(QPainter *painter, vogleditor_timelineItem *pItem, int height, float &minimumOffset)
{
    float duration = pItem->getDuration();
    if (duration < 0)
    {
        return;
    }
    if (pItem->isMarker()) // frame marker
    {
        painter->setBrush(m_triangleBrushWhite);
        painter->setPen(m_trianglePen);

        float offset = scalePositionHorizontally(pItem->getBeginTime());
        painter->drawLine(QLineF(offset, -height, offset, height));
    }
    else
    {
        // only draw if the item will extend beyond the minimum offset and it is on-screen
        float leftOffset = scalePositionHorizontally(pItem->getBeginTime());
        float scaledWidth = scaleDurationHorizontally(duration);
        float rightOffset = leftOffset + scaledWidth;
        if (minimumOffset < rightOffset && rightOffset>m_scroll && leftOffset<m_scroll+width() )
        {
            // Set brush fill color
            if (pItem->getBrush())
            {
                painter->setBrush(*(pItem->getBrush()));
            }
            else // create brush with color relative to time duration of apicall
            {
                float durationRatio = duration / m_maxItemDuration;
                int intensity = std::min(255, (int)(durationRatio * 255.0f));
                QColor color(intensity, 255 - intensity, 0);
                painter->setBrush(QBrush(color));
            }
            // don't draw boundary. It can add an extra pixel to rect width
            painter->setPen(Qt::NoPen);

            // Clamp the item so that it is 1 pixel wide.
            // This is intentionally being done before updating the minimum offset
            // so that small items after the current item will not be drawn
            if (scaledWidth < 1)
            {
                scaledWidth = 1;
            }

            // update minimum offset
            minimumOffset = leftOffset + scaledWidth;

            // draw the colored box that represents this item
            QRectF rect;
            rect.setLeft(leftOffset - m_roundoff);
            rect.setTop(-height / 2);
            rect.setWidth(scaledWidth + m_roundoff);
            rect.setHeight(height);
            painter->drawRect(rect);
        }

        // If only State/Render groups to display, we're done (if debug groups
        // are checked continue to draw individual apicalls)
        //
        // TODO: test replacing following if-block check with just:
        //       if (pItem->isGroupItem())
        if (g_settings.group_state_render_stat() && !g_settings.group_debug_marker_in_use())
        {
            return;
        }
        // now draw all children
        int numChildren = pItem->childCount();
        for (int c = 0; c < numChildren; c++)
        {
            drawTimelineItem(painter, pItem->child(c), height - 1, minimumOffset);
        }
    }

}
