#include "vogleditor_statetreelightitem.h"
#include "vogl_state_vector.h"

vogleditor_stateTreeLightItem::vogleditor_stateTreeLightItem(QString name, unsigned int lightIndex, vogleditor_stateTreeItem *parent, const vogl_state_vector *pState)
    : vogleditor_stateTreeItem(name, "", parent),
      m_lightIndex(lightIndex),
      m_pState(pState),
      m_pDiffBaseState(NULL)
{
    float fVals[4];
#define GET_FLOAT(name, num)                                                                                                                        \
    if (pState->get<float>(name, 0, fVals, num))                                                                                                    \
    {                                                                                                                                               \
        vogleditor_stateTreeStateVecFloatItem *pItem = new vogleditor_stateTreeStateVecFloatItem(#name, name, 0, *pState, fVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                           \
        this->appendChild(pItem);                                                                                                                   \
    }
    GET_FLOAT(GL_CONSTANT_ATTENUATION, 1);
    GET_FLOAT(GL_LINEAR_ATTENUATION, 1);
    GET_FLOAT(GL_QUADRATIC_ATTENUATION, 1);
    GET_FLOAT(GL_SPOT_EXPONENT, 1);
    GET_FLOAT(GL_SPOT_CUTOFF, 1);
    GET_FLOAT(GL_AMBIENT, 4);
    GET_FLOAT(GL_DIFFUSE, 4);
    GET_FLOAT(GL_SPECULAR, 4);
    GET_FLOAT(GL_POSITION, 4);
    GET_FLOAT(GL_SPOT_DIRECTION, 3);
#undef GET_FLOAT
}

void vogleditor_stateTreeLightItem::set_diff_base_state(const vogl_state_vector *pBaseState)
{
    m_pDiffBaseState = pBaseState;

    for (vogleditor_stateTreeStateVecDiffableItem *const *iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
    {
        (*iter)->set_diff_base_state(pBaseState);
    }
}
