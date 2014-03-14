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

#ifndef VOGLEDITOR_FRAMEITEM_H
#define VOGLEDITOR_FRAMEITEM_H

#include <QList>

// external classes (could be predeclared)
#include "vogl_common.h"
#include "vogl_trace_packet.h"
#include "vogleditor_gl_state_snapshot.h"
#include "vogleditor_snapshotitem.h"
#include "vogleditor_apicallitem.h"

class vogleditor_frameItem : public vogleditor_snapshotItem
{
public:
   vogleditor_frameItem(uint64_t frameNumber)
      : m_frameNumber(frameNumber)
   {
   }

   ~vogleditor_frameItem()
   {
      for (int i = 0; i < m_apiCallList.size(); i++)
      {
         vogl_delete(m_apiCallList[i]);
         m_apiCallList[i] = NULL;
      }
      m_apiCallList.clear();
   }

   inline uint64_t frameNumber() const
   {
      return m_frameNumber;
   }

   void appendCall(vogleditor_apiCallItem* pItem)
   {
      m_apiCallList.append(pItem);
   }

   inline int callCount() const
   {
      return m_apiCallList.size();
   }

   vogleditor_apiCallItem* call(int index) const
   {
      if (index < 0 || index > callCount())
      {
         return NULL;
      }

      return m_apiCallList[index];
   }

   bool getStartEndTimes(uint64_t& start, uint64_t& end) const
   {
      int numCalls = callCount();
      if (numCalls == 0)
      {
         return false;
      }

      start = m_apiCallList[0]->startTime();
      end = m_apiCallList[numCalls-1]->endTime();
      return true;
   }

private:
   uint64_t m_frameNumber;
   QList<vogleditor_apiCallItem*> m_apiCallList;
};

#endif // VOGLEDITOR_FRAMEITEM_H
