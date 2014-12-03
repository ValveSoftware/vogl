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

#ifndef VOGLEDITOR_SNAPSHOTITEM_H
#define VOGLEDITOR_SNAPSHOTITEM_H

// external classes (could be predeclared)
#include "vogl_common.h"
#include "vogleditor_gl_state_snapshot.h"

// base class for items that might have a snapshot
class vogleditor_snapshotItem
{
public:
    vogleditor_snapshotItem()
        : m_pSnapshot(NULL)
    {
    }

    virtual ~vogleditor_snapshotItem()
    {
        if (m_pSnapshot != NULL)
        {
            vogl_delete(m_pSnapshot);
            m_pSnapshot = NULL;
        }
    }

    virtual bool needs_snapshot()
    {
        if (has_snapshot() && get_snapshot()->is_valid())
            return false;

        return true;
    }

    virtual void set_snapshot(vogleditor_gl_state_snapshot *pSnapshot)
    {
        if (m_pSnapshot != NULL)
        {
            vogl_delete(m_pSnapshot);
        }

        m_pSnapshot = pSnapshot;
    }

    virtual bool has_snapshot() const
    {
        return (m_pSnapshot != NULL);
    }

    virtual vogleditor_gl_state_snapshot *get_snapshot() const
    {
        return m_pSnapshot;
    }

private:
    vogleditor_gl_state_snapshot *m_pSnapshot;
};

#endif // VOGLEDITOR_SNAPSHOTITEM_H
