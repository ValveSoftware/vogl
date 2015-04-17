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

#ifndef VOGLEDITOR_TIMELINEITEM_H
#define VOGLEDITOR_TIMELINEITEM_H

#include <QList>
class QBrush;

class vogleditor_frameItem;
class vogleditor_groupItem;
class vogleditor_apiCallItem;

class vogleditor_timelineItem
{
public:
    vogleditor_timelineItem(double begin, double end);
    vogleditor_timelineItem(double time, vogleditor_timelineItem *parent, vogleditor_frameItem *groupItem);
    vogleditor_timelineItem(double begin, double end, vogleditor_timelineItem *parent, vogleditor_groupItem *groupItem);
    vogleditor_timelineItem(double begin, double end, vogleditor_timelineItem *parent, vogleditor_apiCallItem *apiCallItem);
    ~vogleditor_timelineItem();

    void appendChild(vogleditor_timelineItem *child);

    vogleditor_timelineItem *child(int row);
    vogleditor_timelineItem *parent();

    int childCount() const;

    QBrush *getBrush();

    void setBrush(QBrush *brush);

    double getBeginTime() const;
    double getEndTime() const;
    double getDuration() const;

    double getMaxChildDuration() const;

    bool isSpan() const;
    bool isMarker() const;

    // (Currently only isGroupItem() is used)
    bool isApiCallItem();
    bool isGroupItem();
    bool isFrameItem();
    bool isRootItem();

    vogleditor_frameItem *getFrameItem() const
    {
        return m_pFrameItem;
    }

    vogleditor_groupItem *getGroupItem() const
    {
        return m_pGroupItem;
    }

    void setFrameItem(vogleditor_frameItem *pFrameItem)
    {
        m_pFrameItem = pFrameItem;
    }

    vogleditor_apiCallItem *getApiCallItem() const
    {
        return m_pApiCallItem;
    }

    void setApiCallItem(vogleditor_apiCallItem *pItem)
    {
        m_pApiCallItem = pItem;
    }

private:
    QBrush *m_brush;
    double m_beginTime;
    double m_endTime;
    double m_duration;
    bool m_isSpan;
    double m_maxChildDuration;

    QList<vogleditor_timelineItem *> m_childItems;

    vogleditor_timelineItem *m_parentItem;
    vogleditor_frameItem *m_pFrameItem;
    vogleditor_groupItem *m_pGroupItem;
    vogleditor_apiCallItem *m_pApiCallItem;
};

#endif // VOGLEDITOR_TIMELINEITEM_H
