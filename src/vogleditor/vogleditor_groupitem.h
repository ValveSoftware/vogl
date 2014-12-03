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

#pragma once

#include <QList>
#include "vogleditor_snapshotitem.h"
#include "vogleditor_apicallitem.h"

#include "vogleditor_frameitem.h"

class vogleditor_groupItem : public vogleditor_snapshotItem
{
public:
    vogleditor_groupItem(vogleditor_frameItem *pFrameItem)
        : m_pParentFrame(pFrameItem)
    {
        pFrameItem->appendGroup(this);
    }

    ~vogleditor_groupItem()
    {
        m_apiCallList.clear();
    }

    void appendCall(vogleditor_apiCallItem *pItem)
    {
        m_apiCallList.append(pItem);
    }

    inline int callCount() const
    {
        return m_apiCallList.size();
    }

    vogleditor_apiCallItem *call(int index) const
    {
        if (index < 0 || index > callCount())
        {
            return NULL;
        }
        return m_apiCallList[index];
    }

    inline uint64_t firstApiCallIndex() const
    {
        return apiCallIndex(0);
    }

    inline uint64_t apiCallIndex(int index = 0) const
    {
        if (vogleditor_apiCallItem *apiCallItem = call(index))
        {
            return apiCallItem->globalCallIndex();
        }
        return uint64_t(-1); // (-1 index won't be found)
    }

    inline uint64_t startTime() const
    {
        return m_apiCallList[0]->startTime();
    }

    inline uint64_t endTime() const
    {
        return m_apiCallList[callCount() - 1]->endTime();
    }

    inline uint64_t duration() const
    {
        return endTime() - startTime();
    }

private:
    vogleditor_frameItem *m_pParentFrame;
    QList<vogleditor_apiCallItem *> m_apiCallList;
};
