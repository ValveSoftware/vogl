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
#include <QVariant>
#include <QAbstractItemModel>

class vogleditor_frameItem;
class vogleditor_apiCallItem;

class vogleditor_timelineItem
{
public:
   vogleditor_timelineItem(float time, vogleditor_timelineItem* parent = 0);
   vogleditor_timelineItem(float begin, float end, vogleditor_timelineItem *parent = 0);
   ~vogleditor_timelineItem();

   void appendChild(vogleditor_timelineItem *child);

   vogleditor_timelineItem* child(int row);
   vogleditor_timelineItem* parent();

   int childCount() const;

   QBrush* getBrush();

   void setBrush(QBrush* brush);

   float getBeginTime() const;
   float getEndTime() const;
   float getDuration() const;

   float getMaxChildDuration() const;

   bool isSpan() const;
   bool isMarker() const;

   vogleditor_frameItem* getFrameItem() const
   {
      return m_pFrameItem;
   }

   void setFrameItem(vogleditor_frameItem* pFrameItem)
   {
      m_pFrameItem = pFrameItem;
   }

   vogleditor_apiCallItem* getApiCallItem() const
   {
      return m_pApiCallItem;
   }

   void setApiCallItem(vogleditor_apiCallItem* pItem )
   {
      m_pApiCallItem = pItem;
   }

private:
    QBrush* m_brush;
    float m_beginTime;
    float m_endTime;
    float m_duration;
    bool m_isSpan;
    float m_maxChildDuration;

    QList<vogleditor_timelineItem*> childItems;

    vogleditor_timelineItem* m_parentItem;
    vogleditor_frameItem* m_pFrameItem;
    vogleditor_apiCallItem* m_pApiCallItem;
};

#endif // VOGLEDITOR_TIMELINEITEM_H
