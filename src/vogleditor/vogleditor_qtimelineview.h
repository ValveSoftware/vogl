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

#ifndef VOGLEDITOR_QTIMELINEVIEW_H
#define VOGLEDITOR_QTIMELINEVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QPainter;
class QPaintEvent;
class QScrollBar;
QT_END_NAMESPACE

#include <QBrush>
#include <QFont>
#include <QPen>
#include <QLinkedList>

#include "vogleditor_timelinemodel.h"
#include "vogleditor_timelineitem.h"

static const float cVOGL_TIMELINEOFFSET = 0.085f;

class vogleditor_QTimelineView : public QWidget
{
    Q_OBJECT
public:
    explicit vogleditor_QTimelineView(QWidget *parent = 0);
    virtual ~vogleditor_QTimelineView();

    void paint(QPainter *painter, QPaintEvent *event);
    void setScrollBar(QScrollBar *scrollBar);

    inline void setModel(vogleditor_timelineModel *pModel)
    {
        m_pModel = pModel;
        if (m_pModel != NULL)
        {
            m_maxItemDuration = m_pModel->get_root_item()->getMaxChildDuration();
        }
    }

    inline vogleditor_timelineModel *model()
    {
        return m_pModel;
    }

    inline void setCurrentFrame(unsigned long long frameNumber)
    {
        m_curFrame = frameNumber;
    }

    inline void setCurrentGroup(unsigned long long groupNumber)
    {
        setCurrentApiCall(groupNumber);
    }

    inline void setCurrentApiCall(unsigned long long apiCallNumber)
    {
        m_curApiCallNumber = apiCallNumber;
    }

private:
    QBrush m_triangleBrushWhite;
    QBrush m_triangleBrushBlack;
    QPen m_trianglePen;
    QPen m_textPen;
    QFont m_textFont;
    float m_roundoff;
    float m_horizontalScale;
    int m_lineLength;
    unsigned long long m_curFrame;
    unsigned long long m_curApiCallNumber;
    float m_maxItemDuration;
    float m_zoom;
    float m_scroll;
    int m_mouseDragStartPos;
    int m_mouseDragStartScroll;
    QScrollBar *m_scrollBar;
    int m_gap;

    vogleditor_timelineModel *m_pModel;

    void drawBaseTimeline(QPainter *painter, const QRect &rect, int gap);
    void drawTimelineItem(QPainter *painter, vogleditor_timelineItem *pItem, int height, float &minimumOffset);
    bool drawCurrentApiCallMarker(QPainter *painter, QPolygon &triangle, vogleditor_timelineItem *pItem);

    float scaleDurationHorizontally(float value);
    float scalePositionHorizontally(float value);
    vogleditor_timelineItem* itemUnderPos(QPoint pos);

    struct timelineItemPos
    {
        vogleditor_timelineItem *pItem;
        float leftOffset;
        float rightOffset;
    };
    QLinkedList<timelineItemPos> m_timelineItemPosCache;
public slots:
    void scrollToPx(int scroll);
    void resetZoom();
protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent * event);    
    void mouseMoveEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *);
    bool event(QEvent *event);
signals:
    void scrollRangeChanged(int min, int max);
    void scrollPosChanged(int pos);
    void timelineItemClicked(vogleditor_apiCallItem *pItem);
};

#endif // VOGLEDITOR_QTIMELINEVIEW_H
