#include "vogleditor_statetreequeryitem.h"

#include "vogl_query_state.h"
#include <inttypes.h>

vogleditor_stateTreeQueryItem::vogleditor_stateTreeQueryItem(QString name, GLuint64 handle, vogleditor_stateTreeItem *parent, const vogl_query_state *pState)
    : vogleditor_stateTreeItem(name, "", parent),
      m_handle(handle),
      m_pState(pState),
      m_pDiffBaseState(NULL)
{
    static QString tmp;
    tmp = tmp.sprintf("%s (%" PRIi64 ")", enum_to_string(pState->get_target()).toStdString().c_str(), pState->get_prev_result());
    setValue(tmp);
}

void vogleditor_stateTreeQueryItem::set_diff_base_state(const vogl_query_state *pBaseState)
{
    m_pDiffBaseState = pBaseState;
}

bool vogleditor_stateTreeQueryItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    if (m_pState->get_target() != m_pDiffBaseState->get_target())
        return true;

    if (m_pState->get_prev_result() != m_pDiffBaseState->get_prev_result())
        return true;

    return false;
}
