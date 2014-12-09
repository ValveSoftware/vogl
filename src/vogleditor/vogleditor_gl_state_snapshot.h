#ifndef VOGLEDITOR_GL_STATE_SNAPSHOT_H
#define VOGLEDITOR_GL_STATE_SNAPSHOT_H

#include "vogl_gl_state_snapshot.h"

class vogleditor_gl_state_snapshot
{
public:
    vogleditor_gl_state_snapshot(vogl_gl_state_snapshot *pSnapshot);
    virtual ~vogleditor_gl_state_snapshot();

    bool is_valid() const
    {
        return m_pSnapshot != NULL && m_pSnapshot->is_valid();
    }

    void set_edited(bool bEdited)
    {
        m_bEdited = bEdited;
    }
    bool is_edited() const
    {
        return m_bEdited;
    }

    void set_outdated(bool bOutdated);
    bool is_outdated() const
    {
        return m_bOutdated;
    }

    inline vogl_gl_state_snapshot *get_snapshot()
    {
        return m_pSnapshot;
    }

    // direct accessors to the snapshot object
    vogl_trace_ptr_value get_cur_trace_context() const
    {
        return m_pSnapshot->get_cur_trace_context();
    }
    vogl_context_snapshot_ptr_vec &get_contexts()
    {
        return m_pSnapshot->get_contexts();
    }
    const vogl_context_snapshot_ptr_vec &get_contexts() const
    {
        return m_pSnapshot->get_contexts();
    }
    vogl_context_snapshot *get_context(vogl_trace_ptr_value contextHandle) const
    {
        return m_pSnapshot->get_context(contextHandle);
    }
    vogl_default_framebuffer_state &get_default_framebuffer()
    {
        return m_pSnapshot->get_default_framebuffer();
    }

private:
    vogl_gl_state_snapshot *m_pSnapshot;
    bool m_bEdited;
    bool m_bOutdated;
};

#endif // VOGLEDITOR_GL_STATE_SNAPSHOT_H
