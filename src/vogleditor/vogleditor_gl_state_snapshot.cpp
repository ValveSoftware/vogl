#include "vogleditor_gl_state_snapshot.h"

vogleditor_gl_state_snapshot::vogleditor_gl_state_snapshot(vogl_gl_state_snapshot *pSnapshot)
    : m_pSnapshot(pSnapshot),
      m_bEdited(false),
      m_bOutdated(false)
{
}

vogleditor_gl_state_snapshot::~vogleditor_gl_state_snapshot()
{
    if (m_pSnapshot != NULL)
    {
        vogl_delete(m_pSnapshot);
        m_pSnapshot = NULL;
    }
}

void vogleditor_gl_state_snapshot::set_outdated(bool bOutdated)
{
    if (m_bOutdated == bOutdated)
        return;

    m_bOutdated = bOutdated;
    if (m_bOutdated)
    {
        // for now, we will delete the snapshot to save memory, in the future we will
        // want to keep it around so that we can diff between them.
        if (m_pSnapshot != NULL)
        {
            vogl_delete(m_pSnapshot);
            m_pSnapshot = NULL;
        }
    }
    else
    {
        VOGL_ASSERT(!"We should not be setting a snapshot as no-longer outdated.");
    }
}
