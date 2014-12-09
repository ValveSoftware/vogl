#include "vogleditor_statetreebufferitem.h"
#include "vogl_buffer_state.h"

vogleditor_stateTreeBufferItem::vogleditor_stateTreeBufferItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_buffer_state *pState)
    : vogleditor_stateTreeItem(name, value, parent),
      m_pState(pState),
      m_pDiffBaseState(NULL)
{
    int iVals[4] = { 0, 0, 0, 0 };
#define GET_INT(name, num)                                                                                                                                   \
    if (pState->get_params().get<int>(name, 0, iVals, num))                                                                                                  \
    {                                                                                                                                                        \
        vogleditor_stateTreeStateVecIntItem *pItem = new vogleditor_stateTreeStateVecIntItem(#name, name, 0, pState->get_params(), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                    \
        this->appendChild(pItem);                                                                                                                            \
    }
#define GET_ENUM(name, num)                                                                                                                                    \
    if (pState->get_params().get<int>(name, 0, iVals, num))                                                                                                    \
    {                                                                                                                                                          \
        vogleditor_stateTreeStateVecEnumItem *pItem = new vogleditor_stateTreeStateVecEnumItem(#name, name, 0, pState->get_params(), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                      \
        this->appendChild(pItem);                                                                                                                              \
    }
#define GET_BOOL(name, num)                                                                                                                                    \
    if (pState->get_params().get<int>(name, 0, iVals, num))                                                                                                    \
    {                                                                                                                                                          \
        vogleditor_stateTreeStateVecBoolItem *pItem = new vogleditor_stateTreeStateVecBoolItem(#name, name, 0, pState->get_params(), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                      \
        this->appendChild(pItem);                                                                                                                              \
    }
#define GET_PTR(name, num)                                                                                                                                   \
    if (pState->get_params().get<int>(name, 0, iVals, num))                                                                                                  \
    {                                                                                                                                                        \
        vogleditor_stateTreeStateVecPtrItem *pItem = new vogleditor_stateTreeStateVecPtrItem(#name, name, 0, pState->get_params(), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                    \
        this->appendChild(pItem);                                                                                                                            \
    }
    GET_ENUM(GL_BUFFER_ACCESS, 1);
    GET_BOOL(GL_BUFFER_MAPPED, 1);
    GET_INT(GL_BUFFER_SIZE, 1);
    GET_ENUM(GL_BUFFER_USAGE, 1);
    GET_PTR(GL_BUFFER_MAP_POINTER, 1);
#undef GET_INT
#undef GET_ENUM
#undef GET_BOOL
#undef GET_PTR
}

void vogleditor_stateTreeBufferItem::set_diff_base_state(const vogl_buffer_state *pBaseState)
{
    m_pDiffBaseState = pBaseState;

    for (vogleditor_stateTreeStateVecDiffableItem *const *iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
    {
        (*iter)->set_diff_base_state(&(pBaseState->get_params()));
    }
}
