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


#include "vogleditor_timelineitem.h"

vogleditor_timelineItem::vogleditor_timelineItem(float time, vogleditor_timelineItem* parent)
   :  m_beginTime(time),
      m_endTime(time),
      m_duration(0),
      m_isSpan(false),
      m_maxChildDuration(0),
      m_parentItem(parent),
      m_pFrameItem(NULL),
      m_pApiCallItem(NULL)
{
}

vogleditor_timelineItem::vogleditor_timelineItem(float begin, float end, vogleditor_timelineItem* parent)
   :  m_beginTime(begin),
      m_endTime(end),
      m_duration(end - begin),
      m_isSpan(true),
      m_maxChildDuration(end - begin),
      m_parentItem(parent),
      m_pFrameItem(NULL),
      m_pApiCallItem(NULL)
{
}

vogleditor_timelineItem::~vogleditor_timelineItem()
{
}

void vogleditor_timelineItem::appendChild(vogleditor_timelineItem* child)
{
    childItems.append(child);

    if (childItems.size() == 1)
    {
        // just added the first child, so overwrite the current maxChildDuration
        m_maxChildDuration = child->getMaxChildDuration();
    }
    else
    {
        // update the maxChildDuration if needed
        m_maxChildDuration = std::max(m_maxChildDuration, child->getMaxChildDuration());
    }
}

vogleditor_timelineItem* vogleditor_timelineItem::child(int row)
{
   return childItems[row];
}

int vogleditor_timelineItem::childCount() const
{
   return childItems.size();
}

vogleditor_timelineItem* vogleditor_timelineItem::parent()
{
   return m_parentItem;
}

QBrush* vogleditor_timelineItem::getBrush()
{
   // if a local brush isn't set, use the parent's brush as a default
   if (m_brush == NULL)
   {
      if (parent() != NULL)
      {
         return parent()->getBrush();
      }
      else
      {
         return NULL;
      }
   }

   return m_brush;
}

void vogleditor_timelineItem::setBrush(QBrush* brush)
{
   m_brush = brush;
}

float vogleditor_timelineItem::getBeginTime() const
{
   return m_beginTime;
}

float vogleditor_timelineItem::getEndTime() const
{
   return m_endTime;
}

float vogleditor_timelineItem::getDuration() const
{
   return m_duration;
}

bool vogleditor_timelineItem::isSpan() const
{
   return m_isSpan;
}

bool vogleditor_timelineItem::isMarker() const
{
   return !m_isSpan;
}

float vogleditor_timelineItem::getMaxChildDuration() const
{
    return m_maxChildDuration;
}
