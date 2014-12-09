#ifndef VOGLEDITOR_STATETREEQUERYITEM_H
#define VOGLEDITOR_STATETREEQUERYITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_query_state;

class vogleditor_stateTreeQueryItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeQueryItem(QString name, GLuint64 handle, vogleditor_stateTreeItem *parent, const vogl_query_state *pState);
    virtual ~vogleditor_stateTreeQueryItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_query_state *pBaseState);

    GLuint64 get_handle() const
    {
        return m_handle;
    }

    virtual bool hasChanged() const;

private:
    GLuint64 m_handle;
    const vogl_query_state *m_pState;
    const vogl_query_state *m_pDiffBaseState;
};

#endif // VOGLEDITOR_STATETREEQUERYITEM_H
