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

#include "vogleditor_apicalltimelinemodel.h"
#include "vogleditor_timelineitem.h"
#include "vogleditor_qapicalltreemodel.h"
#include "vogleditor_apicalltreeitem.h"
#include "vogleditor_groupitem.h"
#include "vogleditor_frameitem.h"
#include "vogleditor_qsettings.h"

vogleditor_apiCallTimelineModel::vogleditor_apiCallTimelineModel(vogleditor_apiCallTreeItem *pRootApiCall)
    : m_pRootApiCall(pRootApiCall),
      m_rawBaseTime(0)
{
    refresh();
}

void vogleditor_apiCallTimelineModel::refresh()
{
    if (m_pRootApiCall == NULL)
    {
        return;
    }

    double timelineStart = 0;
    double timelineEnd = 1;
    m_rawBaseTime = timelineStart;

    int numChildren = m_pRootApiCall->childCount();
    if (numChildren > 0)
    {
        uint64_t firstStart = 0;
        uint64_t firstEnd = 0;
        m_pRootApiCall->child(0)->frameItem()->getStartEndTimes(firstStart, firstEnd);

        uint64_t lastStart = 0;
        uint64_t lastEnd = 0;
        vogleditor_apiCallTreeItem *pLastChild = m_pRootApiCall->child(numChildren - 1);
        pLastChild->frameItem()->getStartEndTimes(lastStart, lastEnd);

        m_rawBaseTime = firstStart;
        timelineStart = firstStart - m_rawBaseTime;
        timelineEnd = lastEnd - m_rawBaseTime;
    }

    // see if we actually have to update some of this stuff
    bool skipCreation = false;
    if (m_rootItem != NULL)
    {
        if (m_rootItem->getDuration() == timelineEnd - timelineStart &&
            m_rootItem->childCount() == numChildren)
        {
            // no need to make a new root
            skipCreation = true;
        }
    }

    if (!skipCreation)
    {
        if (m_rootItem != NULL)
        {
            delete m_rootItem;
        }

        m_rootItem = new vogleditor_timelineItem(timelineStart, timelineEnd);

        // add markers for the start of each frame
        double frameStart = 0;
        for (int c = 0; c < numChildren; c++)
        {
            vogleditor_apiCallTreeItem *pFrameItem = m_pRootApiCall->child(c);
            if (pFrameItem->childCount() > 0)
            {
                // add frame to root (root will manage deletion of frame object)
                frameStart = pFrameItem->startTime() - m_rawBaseTime;
                new vogleditor_timelineItem(frameStart, m_rootItem, pFrameItem->frameItem());
            }
            else
            {
                // if we get here, we are at a frame that had no api calls
            }
        }

        // recursively add children
        for (int frameIndex = 0; frameIndex < numChildren; frameIndex++)
        {
            vogleditor_apiCallTreeItem *pFrameChild = m_pRootApiCall->child(frameIndex);

            AddApiCallsToTimeline(pFrameChild, m_rootItem);
        }
    }
}

float vogleditor_apiCallTimelineModel::u64ToFloat(uint64_t value)
{
    // taken from: http://stackoverflow.com/questions/4400747/converting-from-unsigned-long-long-to-float-with-round-to-nearest-even
    const int mask_bit_count = 31;

    // How many bits are needed?
    int b = sizeof(uint64_t) * CHAR_BIT - 1;
    for (; b >= 0; --b)
    {
        if (value & (1ull << b))
        {
            break;
        }
    }

    // If there are few enough significant bits, use normal cast and done.
    if (b < mask_bit_count)
    {
        return static_cast<float>(value & ~1ull);
    }

    // Save off the low-order useless bits:
    uint64_t low_bits = value & ((1ull << (b - mask_bit_count)) - 1);

    // Now mask away those useless low bits:
    value &= ~((1ull << (b - mask_bit_count)) - 1);

    // Finally, decide how to round the new LSB:
    if (low_bits > ((1ull << (b - mask_bit_count)) / 2ull))
    {
        // Round up.
        value |= (1ull << (b - mask_bit_count));
    }
    else
    {
        // Round down.
        value &= ~(1ull << (b - mask_bit_count));
    }

    return static_cast<float>(value);
}

double vogleditor_apiCallTimelineModel::absoluteToRelativeTime(uint64_t time)
{
    return time - m_rawBaseTime;
}

unsigned int vogleditor_apiCallTimelineModel::randomRGB()
{

    // TODO: If Debug marker groups are allowed to be colored independent of the
    //       State/Render groups setting, then force their colors to be a strong
    //       mix of blue/red and blue/green to distinguish them separately from
    //       independent apicalls which are a mix of red/green.
    //static int sSwap = 2;
    //sSwap = 3 - sSwap;
    //return  (0xC0 | (0x40 << sSwap * 8) | (rand() & ((0xFF << sSwap * 8) | 0xFF)));

    // mask out some lower bits from each component for greater contrast
    return (rand() & 0xF8F8F8);
}

void vogleditor_apiCallTimelineModel::AddApiCallsToTimeline(vogleditor_apiCallTreeItem *pParentCallTreeItem, vogleditor_timelineItem *pParentTimelineItem)
{
    vogleditor_timelineItem *pNewTimelineItem;

    int numChildren = pParentCallTreeItem->childCount();
    for (int c = 0; c < numChildren; c++)
    {
        vogleditor_apiCallTreeItem *pChildCallTreeItem = pParentCallTreeItem->child(c);

        double beginFloat = pChildCallTreeItem->startTime() - m_rawBaseTime;
        double endFloat = pChildCallTreeItem->endTime() - m_rawBaseTime;

        if (pChildCallTreeItem->isGroup())
        {
            // Create a group timelineItem with group color
            pNewTimelineItem = new vogleditor_timelineItem(beginFloat, endFloat, m_rootItem, pChildCallTreeItem->groupItem());
            QColor color;
            if (pChildCallTreeItem->isStateChangeGroup())
            {
                color = Qt::green;
            }
            else if (pChildCallTreeItem->isRenderGroup())
            {
                color = Qt::red;
            }
            pNewTimelineItem->setBrush(new QBrush(color, Qt::Dense5Pattern));
        }
        else // (API call)
        {
            // close a timeline parent group if the tree parent group has ended
            if (!pChildCallTreeItem->parent()->isGroup() && pParentTimelineItem->isGroupItem())
            {
                pParentTimelineItem = pParentTimelineItem->parent();
            }

            // Create new timeline apicall item
            pNewTimelineItem = new vogleditor_timelineItem(beginFloat, endFloat, pParentTimelineItem, pChildCallTreeItem->apiCallItem());

            // Add random color for debug marker group parent
            if (g_settings.group_debug_marker_in_use())
            {
                // (For now) only if State/Render groups are enabled so that
                // default timeline display isn't affected (debug marker groups
                // are checked on)
                // TODO: remove check for state/render status if/when consensus
                //       is the debug marker groups should be separately colored
                //       by default (when checked on)
                if (g_settings.group_state_render_stat())
                {
                    if (vogl_is_marker_push_entrypoint(pChildCallTreeItem->entrypoint_id()))
                    {
                        pNewTimelineItem->setBrush(new QBrush(QColor(randomRGB())));
                    }
                }
            }
        }
        AddApiCallsToTimeline(pChildCallTreeItem, pNewTimelineItem);
    }
}
