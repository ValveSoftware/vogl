#include "vogleditor_statetreesyncitem.h"

#include "vogl_sync_object.h"

vogleditor_stateTreeSyncItem::vogleditor_stateTreeSyncItem(QString name, vogleditor_stateTreeItem *parent, const vogl_sync_state &state)
    : vogleditor_stateTreeItem(name, "", parent),
      m_pState(&state),
      m_pDiffBaseState(NULL)
{
    int value = 0;
    if (state.get_params().get<int>(GL_SYNC_STATUS, 0, &value))
    {
        setValue(enum_to_string(value));
    }
}

void vogleditor_stateTreeSyncItem::set_diff_base_state(const vogl_sync_state *pBaseState)
{
    m_pDiffBaseState = pBaseState;
}

bool vogleditor_stateTreeSyncItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    return !(m_pState->compare_restorable_state(*m_pDiffBaseState));
}
