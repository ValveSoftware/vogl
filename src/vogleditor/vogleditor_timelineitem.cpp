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

#include <QBrush>

#include "vogleditor_timelineitem.h"
#include "vogl_common.h"

// Timeline root (fullspan)
vogleditor_timelineItem::vogleditor_timelineItem(double begin, double end)
    : m_brush(NULL),
      m_beginTime(begin),
      m_endTime(end),
      m_duration(end - begin),
      m_isSpan(true),
      m_maxChildDuration(end - begin),
      m_parentItem(NULL),
      m_pFrameItem(NULL),
      m_pGroupItem(NULL),
      m_pApiCallItem(NULL)
{
}

// Timeline markers (frame demarcations)
vogleditor_timelineItem::vogleditor_timelineItem(double time, vogleditor_timelineItem *parent, vogleditor_frameItem *frameItem)
    : m_brush(NULL),
      m_beginTime(time),
      m_endTime(time),
      m_duration(0),
      m_isSpan(false),
      m_maxChildDuration(0),
      m_parentItem(parent),
      m_pFrameItem(frameItem),
      m_pGroupItem(NULL),
      m_pApiCallItem(NULL)
{
    VOGL_ASSERT(parent != NULL);
    parent->appendChild(this);
}

// Timeline groups
vogleditor_timelineItem::vogleditor_timelineItem(double begin, double end, vogleditor_timelineItem *parent, vogleditor_groupItem *groupItem)
    : m_brush(NULL),
      m_beginTime(begin),
      m_endTime(end),
      m_duration(end - begin),
      m_isSpan(true),
      m_maxChildDuration(end - begin),
      m_parentItem(parent),
      m_pFrameItem(NULL),
      m_pGroupItem(groupItem),
      m_pApiCallItem(NULL)
{
    VOGL_ASSERT(parent != NULL);
    parent->appendChild(this);
}

// Timeline segmented spans
vogleditor_timelineItem::vogleditor_timelineItem(double begin, double end, vogleditor_timelineItem *parent, vogleditor_apiCallItem *apiCallItem)
    : m_brush(NULL),
      m_beginTime(begin),
      m_endTime(end),
      m_duration(end - begin),
      m_isSpan(true),
      m_maxChildDuration(end - begin),
      m_parentItem(parent),
      m_pFrameItem(NULL),
      m_pGroupItem(NULL),
      m_pApiCallItem(apiCallItem)
{
    VOGL_ASSERT(parent != NULL);
    parent->appendChild(this);
}

bool vogleditor_timelineItem::isApiCallItem()
{
    return (m_pApiCallItem != NULL);
}

bool vogleditor_timelineItem::isGroupItem()
{
    return (m_pGroupItem != NULL);
}

bool vogleditor_timelineItem::isFrameItem()
{
    return (m_pFrameItem != NULL);
}

bool vogleditor_timelineItem::isRootItem()
{
    return !(isApiCallItem() | isGroupItem() | isFrameItem());
}

vogleditor_timelineItem::~vogleditor_timelineItem()
{
    delete m_brush;
    for (int i = 0; i < m_childItems.size(); i++)
    {
        delete m_childItems[i];
        m_childItems[i] = NULL;
    }
    m_childItems.clear();
}

void vogleditor_timelineItem::appendChild(vogleditor_timelineItem *child)
{
    m_childItems.append(child);

    if (m_childItems.size() == 1)
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

vogleditor_timelineItem *vogleditor_timelineItem::child(int row)
{
    return m_childItems[row];
}

int vogleditor_timelineItem::childCount() const
{
    return m_childItems.size();
}

vogleditor_timelineItem *vogleditor_timelineItem::parent()
{
    return m_parentItem;
}

QBrush *vogleditor_timelineItem::getBrush()
{
    // if local brush isn't set, use first non-null ancestor's brush as default
    if (m_brush == NULL)
    {
        if (parent() != NULL)
        {
            return parent()->getBrush();
        }
    }

    return m_brush;
}

void vogleditor_timelineItem::setBrush(QBrush *brush)
{
    m_brush = brush;
}

double vogleditor_timelineItem::getBeginTime() const
{
    return m_beginTime;
}

double vogleditor_timelineItem::getEndTime() const
{
    return m_endTime;
}

double vogleditor_timelineItem::getDuration() const
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

double vogleditor_timelineItem::getMaxChildDuration() const
{
    return m_maxChildDuration;
}
