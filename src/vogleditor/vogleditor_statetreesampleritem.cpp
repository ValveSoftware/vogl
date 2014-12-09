#include "vogleditor_statetreesampleritem.h"
#include "vogl_sampler_state.h"

vogleditor_stateTreeSamplerItem::vogleditor_stateTreeSamplerItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_sampler_state *pState, const vogl_context_info &info)
    : vogleditor_stateTreeItem(name, value, parent),
      m_pState(pState),
      m_pDiffBaseState(NULL)
{
    int iVals[4] = { 0, 0, 0, 0 };
    float fVals[4] = { 0, 0, 0, 0 };
#define GET_ENUM(name, num)                                                                                                                                    \
    if (pState->get_params().get<int>(name, 0, iVals, num))                                                                                                    \
    {                                                                                                                                                          \
        vogleditor_stateTreeStateVecEnumItem *pItem = new vogleditor_stateTreeStateVecEnumItem(#name, name, 0, pState->get_params(), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                      \
        this->appendChild(pItem);                                                                                                                              \
    }
#define GET_FLOAT(name, num)                                                                                                                                     \
    if (pState->get_params().get<float>(name, 0, fVals, num))                                                                                                    \
    {                                                                                                                                                            \
        vogleditor_stateTreeStateVecFloatItem *pItem = new vogleditor_stateTreeStateVecFloatItem(#name, name, 0, pState->get_params(), fVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                        \
        this->appendChild(pItem);                                                                                                                                \
    }

    GET_ENUM(GL_TEXTURE_MAG_FILTER, 1);
    GET_ENUM(GL_TEXTURE_MIN_FILTER, 1);
    GET_FLOAT(GL_TEXTURE_MIN_LOD, 1);
    GET_FLOAT(GL_TEXTURE_MAX_LOD, 1);
    GET_ENUM(GL_TEXTURE_WRAP_S, 1);
    GET_ENUM(GL_TEXTURE_WRAP_T, 1);
    GET_ENUM(GL_TEXTURE_WRAP_R, 1);
    GET_FLOAT(GL_TEXTURE_BORDER_COLOR, 4);
    GET_ENUM(GL_TEXTURE_COMPARE_MODE, 1);
    GET_ENUM(GL_TEXTURE_COMPARE_FUNC, 1);

    if (info.supports_extension("GL_EXT_texture_filter_anisotropic"))
    {
        GET_FLOAT(GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
    }

    if (info.supports_extension("GL_EXT_texture_sRGB_decode"))
    {
        GET_ENUM(GL_TEXTURE_SRGB_DECODE_EXT, 1);
    }
#undef GET_ENUM
#undef GET_FLOAT
}

void vogleditor_stateTreeSamplerItem::set_diff_base_state(const vogl_sampler_state *pBaseState)
{
    m_pDiffBaseState = pBaseState;

    for (vogleditor_stateTreeStateVecDiffableItem *const *iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
    {
        (*iter)->set_diff_base_state(&(pBaseState->get_params()));
    }
}
