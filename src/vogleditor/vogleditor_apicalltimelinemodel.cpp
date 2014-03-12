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

#include "vogleditor_apicalltimelinemodel.h"

vogleditor_apiCallTimelineModel::vogleditor_apiCallTimelineModel(vogleditor_apiCallTreeItem* pRootApiCall) :
   m_pRootApiCall(pRootApiCall),
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


   float timelineStart = 0;
   float timelineEnd = 1;
   m_rawBaseTime = timelineStart;

   int numChildren = m_pRootApiCall->childCount();
   if (numChildren > 0)
   {
       uint64_t firstStart = 0;
       uint64_t firstEnd = 0;
       m_pRootApiCall->child(0)->frameItem()->getStartEndTimes(firstStart, firstEnd);

       uint64_t lastStart = 0;
       uint64_t lastEnd = 0;
       vogleditor_apiCallTreeItem* pLastChild = m_pRootApiCall->child(numChildren-1);
       pLastChild->frameItem()->getStartEndTimes(lastStart, lastEnd);

       m_rawBaseTime = firstStart;
       timelineStart = u64ToFloat(firstStart - m_rawBaseTime);
       timelineEnd = u64ToFloat(lastEnd - m_rawBaseTime);
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
      float frameStart = 0;
      for (int c = 0; c < numChildren; c++)
      {
         vogleditor_apiCallTreeItem* pFrameItem = m_pRootApiCall->child(c);
         if (pFrameItem->childCount() > 0)
         {
            frameStart = u64ToFloat(pFrameItem->child(0)->apiCallItem()->startTime() - m_rawBaseTime);
            vogleditor_timelineItem* pFrameTimelineItem = new vogleditor_timelineItem(frameStart, m_rootItem);
            pFrameTimelineItem->setFrameItem(pFrameItem->frameItem());
            m_rootItem->appendChild(pFrameTimelineItem);
         }
         else
         {
            // if we get here, we are at a frame that had no api calls
         }
      }

      // recursively add each children
      for (int frameIndex = 0; frameIndex < numChildren; frameIndex++)
      {
         vogleditor_apiCallTreeItem* pFrameChild = m_pRootApiCall->child(frameIndex);

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
    for(; b >= 0; --b)
    {
        if(value & (1ull << b))
        {
            break;
        }
    }

    // If there are few enough significant bits, use normal cast and done.
    if(b < mask_bit_count)
    {
        return static_cast<float>(value & ~1ull);
    }

    // Save off the low-order useless bits:
    uint64_t low_bits = value & ((1ull << (b - mask_bit_count)) - 1);

    // Now mask away those useless low bits:
    value &= ~((1ull << (b - mask_bit_count)) - 1);

    // Finally, decide how to round the new LSB:
    if(low_bits > ((1ull << (b - mask_bit_count)) / 2ull))
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

void vogleditor_apiCallTimelineModel::AddApiCallsToTimeline(vogleditor_apiCallTreeItem* pRoot, vogleditor_timelineItem* pDestRoot)
{
   int numChildren = pRoot->childCount();
   for (int c = 0; c < numChildren; c++)
   {
      vogleditor_apiCallTreeItem* pChild = pRoot->child(c);
      if (pChild->apiCallItem() != NULL)
      {
          float beginFloat = u64ToFloat(pChild->apiCallItem()->startTime() - m_rawBaseTime);
          float endFloat = u64ToFloat(pChild->apiCallItem()->endTime() - m_rawBaseTime);

          vogleditor_timelineItem* pNewItem = new vogleditor_timelineItem(beginFloat, endFloat, pDestRoot);
          pNewItem->setApiCallItem(pChild->apiCallItem());
          pDestRoot->appendChild(pNewItem);
          AddApiCallsToTimeline(pChild, pNewItem);
      }
   }
}
