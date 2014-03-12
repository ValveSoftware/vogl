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
#include "vogleditor_qtimelineview.h"
#include "vogleditor_frameitem.h"

vogleditor_QTimelineView::vogleditor_QTimelineView(QWidget *parent) :
   QWidget(parent),
   m_curFrame(0),
   m_curApiCallNumber(0),
   m_pModel(NULL),
   m_pPixmap(NULL)
{
   QLinearGradient gradient(QPointF(50, -20), QPointF(80, 20));
   gradient.setColorAt(0.0, Qt::white);
   gradient.setColorAt(1.0, QColor(0xa6, 0xce, 0x39));

   m_background = QBrush(QColor(200,200,200));//QBrush(parent->palette().brush(parent->backgroundRole()));
   m_triangleBrush = QBrush(gradient);
   m_trianglePen = QPen(Qt::black);
   m_trianglePen.setWidth(1);
   m_textPen = QPen(Qt::white);
   m_textFont.setPixelSize(50);

   m_horizontalScale = 1;
   m_lineLength = 1;
}

vogleditor_QTimelineView::~vogleditor_QTimelineView()
{
    deletePixmap();
}

void vogleditor_QTimelineView::paintEvent(QPaintEvent *event)
{
    QPainter painter;
    painter.begin(this);
    paint(&painter, event);
    painter.end();
}

void vogleditor_QTimelineView::drawBaseTimeline(QPainter* painter, const QRect& rect, int gap)
{
    painter->save();

    // fill entire background with background color
    painter->fillRect(rect, m_background);

    // translate drawing to vertical center of rect
    painter->translate(0, rect.height()/2);

    painter->setBrush(m_triangleBrush);
    painter->setPen(m_trianglePen);

    // everything will have a small gap on the left and right sides
    painter->translate(gap, 0);

    // draw the actual timeline
    int lineLength = rect.width()-2*gap;
    painter->drawLine(0,0, lineLength, 0);

    painter->restore();
}

void vogleditor_QTimelineView::paint(QPainter *painter, QPaintEvent *event)
{
    int gap = 10;
    int arrowHeight = 10;
    int arrowTop = event->rect().height()/2-gap-arrowHeight;
    int arrowHalfWidth = 3;
     m_lineLength = event->rect().width()-2*gap;

    QPolygon triangle(3);
    triangle.setPoint(0, 0, arrowTop);
    triangle.setPoint(1, -arrowHalfWidth, arrowTop+arrowHeight);
    triangle.setPoint(2, arrowHalfWidth, arrowTop+arrowHeight);

    drawBaseTimeline(painter, event->rect(), gap);

    if (m_pModel == NULL)
    {
        return;
    }

    if (m_pModel->get_root_item() == NULL)
    {
        return;
    }

    if (m_pPixmap != NULL)
    {
        if (m_pPixmap->height() != event->rect().height() ||
            m_pPixmap->width() != event->rect().width())
        {
            deletePixmap();
        }
    }

    if (m_pPixmap == NULL)
    {
        m_pPixmap = new QPixmap(event->rect().width(), event->rect().height());
        QPainter pixmapPainter(m_pPixmap);
        drawBaseTimeline(&pixmapPainter, event->rect(), gap);

        // translate drawing to vertical center of rect
        // everything will have a small gap on the left and right sides
        pixmapPainter.translate(gap, event->rect().height()/2);

        if (m_pModel->get_root_item()->getBrush() == NULL)
        {
            m_pModel->get_root_item()->setBrush(&m_triangleBrush);
        }

        m_horizontalScale = (float)m_lineLength / (float)m_pModel->get_root_item()->getDuration();

        // we don't want to draw the root item, but all of its children
        int numChildren = m_pModel->get_root_item()->childCount();
        int height = event->rect().height()/2-2*gap;

        pixmapPainter.setBrush(m_triangleBrush);
        pixmapPainter.setPen(m_trianglePen);

        for (int c = 0; c < numChildren; c++)
        {
            vogleditor_timelineItem* pChild = m_pModel->get_root_item()->child(c);
            drawTimelineItem(&pixmapPainter, pChild, height);
        }
    }

    painter->drawPixmap(event->rect(), *m_pPixmap, event->rect());

    // translate drawing to vertical center of rect
    // everything will have a small gap on the left and right sides
    painter->translate(gap, event->rect().height()/2);

    painter->setBrush(m_triangleBrush);
    painter->setPen(m_trianglePen);

    int numChildren = m_pModel->get_root_item()->childCount();
    for (int c = 0; c < numChildren; c++)
    {
        vogleditor_timelineItem* pChild = m_pModel->get_root_item()->child(c);

        // draw current frame marker
        if (pChild->getFrameItem() != NULL && pChild->getFrameItem()->frameNumber() == m_curFrame)
        {
            painter->save();
            painter->translate(scalePositionHorizontally(pChild->getBeginTime()), 0);
            painter->drawPolygon(triangle);
            painter->restore();
        }

        // draw current api call marker
        if (pChild->getApiCallItem() != NULL && pChild->getApiCallItem()->globalCallIndex() == m_curApiCallNumber)
        {
            painter->save();
            painter->translate(scalePositionHorizontally(pChild->getBeginTime()), 0);
            painter->drawPolygon(triangle);
            painter->restore();
        }
    }
}

float vogleditor_QTimelineView::scaleDurationHorizontally(float value)
{
   float scaled = value * m_horizontalScale;
   if (scaled <= 0)
   {
      scaled = 1;
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

void vogleditor_QTimelineView::drawTimelineItem(QPainter* painter, vogleditor_timelineItem *pItem, int height)
{
   float duration = pItem->getDuration();
   if (duration < 0)
   {
      return;
   }

   painter->save();
   if (pItem->isMarker())
   {
      painter->setBrush(m_triangleBrush);
      painter->setPen(m_trianglePen);

      float offset = scalePositionHorizontally(pItem->getBeginTime());
      painter->drawLine(QLineF(offset, -height, offset, height));
   }
   else
   {
      float durationRatio = duration / m_maxItemDuration;
      int intensity = std::min(255, (int)(durationRatio * 255.0f));
      //   painter->setBrush(*(pItem->getBrush()));
      QColor color(intensity, 255-intensity, 0);
      painter->setBrush(QBrush(color));
      painter->setPen(color);
      QRectF rect;
      rect.setLeft(scalePositionHorizontally(pItem->getBeginTime()));
      rect.setTop(-height/2);
      rect.setWidth(scaleDurationHorizontally(duration));
      rect.setHeight(height);
      painter->drawRect(rect);

      // now draw all children
      int numChildren = pItem->childCount();
      for (int c = 0; c < numChildren; c++)
      {
         drawTimelineItem(painter, pItem->child(c), height-1);
      }
   }

   painter->restore();
}
