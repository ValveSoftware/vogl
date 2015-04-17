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
#include <QScrollBar>
#include <QMouseEvent>
#include <QToolTip>
#include "vogleditor_qtimelineview.h"
#include "vogleditor_qsettings.h"
#include "vogleditor_frameitem.h"
#include "vogleditor_groupitem.h"

vogleditor_QTimelineView::vogleditor_QTimelineView(QWidget *parent)
    : QWidget(parent),
      m_roundoff(cVOGL_TIMELINEOFFSET),
      m_curFrameTime(-1),
      m_curApiCallTime(-1),
      m_zoom(1),
      m_scroll(0),
      m_pModel(NULL),
      m_pPixmap(NULL)
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

void vogleditor_QTimelineView::mouseReleaseEvent(QMouseEvent *event)
{
    if (qAbs(event->pos().x()-m_mouseDragStartPos)<3)
    {
        vogleditor_timelineItem *pItem = itemUnderPos(event->pos());
        if (pItem!=NULL && pItem->isApiCallItem())
            emit(timelineItemClicked(pItem->getApiCallItem()));
        if (pItem!=NULL && pItem->isGroupItem())
            emit(timelineItemClicked(pItem->getGroupItem()));
    }
}

void vogleditor_QTimelineView::mouseMoveEvent(QMouseEvent *event)
{
    if(event->buttons() & Qt::LeftButton)
    {
        scrollToPx(m_mouseDragStartScroll+(m_mouseDragStartPos-event->x()));
    }
}

void vogleditor_QTimelineView::resizeEvent(QResizeEvent *)
{
    m_scrollBar->setPageStep(width());

    if (m_pPixmap != NULL)
    {
        int pmHeight = m_pPixmap->height();
        int pmWidth = m_pPixmap->width();
        float widthPctDelta = (float)(width() - pmWidth) / (float)pmWidth;
        float heightPctDelta = (float)(height() - pmHeight) / (float)pmHeight;
        // If the resize is of a 'signficant' amount, then delete the pixmap so that it will be regenerated at the new size.
        if (fabs(widthPctDelta) > 0.2 ||
                fabs(heightPctDelta) > 0.2)
        {
            deletePixmap();
        }
    }
}

void vogleditor_QTimelineView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        float oldZoom=m_zoom;
        int oldScroll=m_scroll;
        if (event->angleDelta().y()>0)
            m_zoom=m_zoom*(1.2f);
        else
            m_zoom=m_zoom/(1.2f);
        m_zoom = qMax(m_zoom, 1.0f);

        if ((long int)m_zoom*((long int)width())+(long int)m_gap*2+(long int)width() > 2147483647)
        {
            m_zoom=oldZoom;
            return;
        }

        //Update the scrolol range in scroll bars before updating the scroll position.
        emit(scrollRangeChanged(0, (int)((double)m_zoom*(double)width()-width())));

        //Keep time line stationary under the mouse (using the oldScroll in case m_scroll was changes when updating the limits):
        scrollToPx(((double)(oldScroll+event->x())/(double)oldZoom)*(double)m_zoom-event->x());
    }else
    {
        scrollToPx(m_scroll-event->angleDelta().y()/3);
    }
}

bool vogleditor_QTimelineView::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

        vogleditor_timelineItem* pItem = itemUnderPos(helpEvent->pos());
        if (pItem!=NULL)
        {
            if (pItem->isApiCallItem())
            {
                QString tip = QString::number(pItem->getApiCallItem()->globalCallIndex());
                tip.append(": ");
                tip.append(pItem->getApiCallItem()->apiFunctionCall());
                QToolTip::showText(helpEvent->globalPos(), tip, this);
            }
            else if(pItem->isGroupItem())
            {
                QString tip;
                if (pItem->getGroupItem()->callCount()>1)
                    tip = tr("%1 Calls").arg(QString::number(pItem->getGroupItem()->callCount()));
                else
                    tip = tr("1 Call");
                QToolTip::showText(helpEvent->globalPos(), tip, this);
            }
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    return QWidget::event(event);
}

void vogleditor_QTimelineView::resetZoom()
{
    m_zoom=1;
    emit(scrollRangeChanged(0, 0));
}

void vogleditor_QTimelineView::scrollToPx(int scroll)
{
    if (m_scroll==scroll)
        return;
    m_scroll=scroll;
    m_scroll=qMax(m_scroll, 0);
    m_scroll=qMin(m_scroll, (int)((double)m_zoom*(double)width()-width()));
    emit(scrollPosChanged(m_scroll));    
    deletePixmap();
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
    m_gap = 10;
    int arrowHeight = 10;
    int arrowTop = height() / 2 - m_gap - arrowHeight;
    int arrowHalfWidth = 3;
    m_lineLength = (double)width()*(double)m_zoom - 2 * m_gap;

    QPolygon triangle(3);
    triangle.setPoint(0, 0, arrowTop);
    triangle.setPoint(1, -arrowHalfWidth, arrowTop + arrowHeight);
    triangle.setPoint(2, arrowHalfWidth, arrowTop + arrowHeight);

    painter->save();
    painter->translate(-m_scroll, 0);    
    drawBaseTimeline(painter, event->rect(), m_gap);
    //Go back to unscrolled coordinates to paint the pixmap to the widget.
    painter->restore();

    if (m_pModel == NULL)
    {
        return;
    }

    if (m_pModel->get_root_item() == NULL)
    {
        return;
    }

    if (m_pPixmap == NULL)
    {
        m_timelineItemPosCache.clear();
        m_pPixmap = new QPixmap(event->rect().width(), event->rect().height());
        m_pPixmap->fill(Qt::transparent);
        QPainter pixmapPainter(m_pPixmap);

        //Apply the scroll translation:
        pixmapPainter.translate(-m_scroll, 0);
        // translate drawing to vertical center of rect
        // everything will have a small gap on the left and right sides
        pixmapPainter.translate(m_gap, height() / 2);

        m_horizontalScale = (double)m_lineLength / (double)m_pModel->get_root_item()->getDuration();

        // we don't want to draw the root item, but all of its children
        int numChildren = m_pModel->get_root_item()->childCount();
        int i_height = height() / 2 - 2 * m_gap;

        pixmapPainter.setBrush(m_triangleBrushWhite);
        pixmapPainter.setPen(m_trianglePen);

        double minimumOffset = 0;
        vogleditor_timelineItem *rootItem = m_pModel->get_root_item();
        for (int c = 0; c < numChildren; c++)
        {
            vogleditor_timelineItem *pChild = rootItem->child(c);
            drawTimelineItem(&pixmapPainter, pChild, i_height, minimumOffset);
        }
    }

    painter->drawPixmap(event->rect(), *m_pPixmap, m_pPixmap->rect());

    painter->translate(-m_scroll, 0);
    painter->setBrush(m_triangleBrushWhite);
    painter->setPen(m_trianglePen);

    // translate drawing to vertical center of rect
    // everything will have a small gap on the left and right sides
    painter->translate(m_gap, height() / 2);

    if (m_curFrameTime!=-1)
    {
        painter->save();
        //translate to the point to draw marker
        painter->translate(scalePositionHorizontally(m_curFrameTime), 0);
        painter->setBrush(m_triangleBrushBlack);
        painter->drawPolygon(triangle);
        painter->restore();
    }

    // draw current api call marker
    if (m_curApiCallTime!=-1)
    {
        double xpos = scalePositionHorizontally(m_curApiCallTime);
        xpos -= m_roundoff;

        painter->save();
        //translate to the point to draw marker
        painter->translate(xpos, 0);
        painter->drawPolygon(triangle);
        painter->restore();
    }
}

void vogleditor_QTimelineView::setScrollBar(QScrollBar *scrollBar)
{
    m_scrollBar=scrollBar;
    connect(this, SIGNAL(scrollPosChanged(int)), m_scrollBar, SLOT(setValue(int)));
    connect(this, SIGNAL(scrollRangeChanged(int,int)), m_scrollBar, SLOT(setRange(int,int)));
    connect(m_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(scrollToPx(int)));
}

double vogleditor_QTimelineView::scaleDurationHorizontally(double value)
{
    double scaled = (double)value * (double)m_horizontalScale;
    if (scaled <= m_horizontalScale)
    {
        scaled = m_horizontalScale;
    }

    return scaled;
}

double vogleditor_QTimelineView::scalePositionHorizontally(double value)
{
    double horizontalShift = m_pModel->get_root_item()->getBeginTime();
    double horizontalLength = m_pModel->get_root_item()->getDuration();
    double offset = ((value - horizontalShift) / horizontalLength) * m_lineLength;

    return offset;
}

vogleditor_timelineItem *vogleditor_QTimelineView::itemUnderPos(QPoint pos)
{
    int x=pos.x()+m_scroll-m_gap;
    {
        QLinkedListIterator<timelineItemPos> i(m_timelineItemPosCache);
        while (i.hasNext())
        {
            timelineItemPos itempos = i.next();
            //See if we have gone beyond the pos
            if (itempos.leftOffset > x)
                break;
            if (itempos.leftOffset<=x && itempos.rightOffset>=x)
            {
                return itempos.pItem;
            }
        }
    }
    {
        QLinkedListIterator<timelineItemPos> i(m_timelineItemPosCache);
        while (i.hasNext())
        {
            timelineItemPos itempos = i.next();
            if (itempos.leftOffset > x+1)
                break;
            if (itempos.leftOffset<= x+1  && itempos.rightOffset>= x-1 )
                return itempos.pItem;
        }
    }
    return NULL;
}

void vogleditor_QTimelineView::drawTimelineItem(QPainter *painter, vogleditor_timelineItem *pItem, int height, double &minimumOffset)
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
        double leftOffset = scalePositionHorizontally(pItem->getBeginTime());
        double scaledWidth = scaleDurationHorizontally(duration);
        double rightOffset = leftOffset + scaledWidth;
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
            int startTimePx = leftOffset - m_roundoff;
            int durationPx = scaledWidth + m_roundoff;

            // draw the colored box that represents this item
            QRect rect;
            rect.setLeft(startTimePx);
            rect.setTop(-height / 2);
            rect.setWidth(durationPx);
            rect.setHeight(height);
            painter->drawRect(rect);

            //Add the position of this item to the timelineItemPosCache for use by itemUnderPos().
            timelineItemPos itemPos;
            itemPos.pItem=pItem;
            itemPos.leftOffset=startTimePx;
            itemPos.rightOffset=startTimePx+durationPx;
            m_timelineItemPosCache.append(itemPos);
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
