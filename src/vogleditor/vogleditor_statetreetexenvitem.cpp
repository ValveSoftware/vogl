#include "vogleditor_statetreetexenvitem.h"
#include "vogl_texenv_state.h"

vogleditor_stateTreeTexEnvStateVecDiffableItem::vogleditor_stateTreeTexEnvStateVecDiffableItem(GLenum target, GLenum enumId, unsigned int index, unsigned int numComponents, QString name, QString value, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeStateVecDiffableItem(name, value, parent),
      m_target(target),
      m_name(enumId),
      m_index(index),
      m_numComponents(numComponents),
      m_pDiffBaseInfo(NULL),
      m_pDiffBaseState(NULL)
{
}

//=============================================================================

vogleditor_stateTreeTexEnvStateVecEnumItem::vogleditor_stateTreeTexEnvStateVecEnumItem(GLenum target, QString glenumName, GLenum name, unsigned int index, int *values, unsigned int numComponents, vogleditor_stateTreeItem *parent, vogl_texenv_state &state)
    : vogleditor_stateTreeTexEnvStateVecDiffableItem(target, name, index, numComponents, glenumName, "", parent),
      m_pState(&state)
{
    setValue(getValueFromEnums(values, numComponents));
}

bool vogleditor_stateTreeTexEnvStateVecEnumItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL || m_pDiffBaseInfo == NULL)
        return false;

    if (m_index >= m_pDiffBaseInfo->get_max_texture_coords())
    {
        // this index did not exist in the base state, so it must be new
        return true;
    }

    int curVals[4] = { 0, 0, 0, 0 };
    int baseVals[4] = { 0, 0, 0, 0 };
    if (m_pState->get_state(m_target).get<int>(m_name, m_index, curVals, m_numComponents) &&
        m_pDiffBaseState->get_state(m_target).get<int>(m_name, m_index, baseVals, m_numComponents))
    {
        return curVals[0] != baseVals[0] || curVals[1] != baseVals[1] || curVals[2] != baseVals[2] || curVals[3] != baseVals[3];
    }
    else
    {
        // the values could not be obtained from both states, so something must have changed
        return true;
    }
}

QString vogleditor_stateTreeTexEnvStateVecEnumItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL || m_pDiffBaseInfo == NULL)
        return "";

    if (m_index >= m_pDiffBaseInfo->get_max_texture_coords())
    {
        // this index did not exist in the base state, so it must be new
        return "non-existent";
    }

    int baseVals[4] = { 0, 0, 0, 0 };
    if (m_pDiffBaseState->get_state(m_target).get<int>(m_name, m_index, baseVals, m_numComponents))
    {
        return getValueFromEnums(baseVals, m_numComponents);
    }
    else
    {
        // the values could not be obtained from both states, so something must have changed
        return "non-existent";
    }
}

//=============================================================================

vogleditor_stateTreeTexEnvStateVecFloatItem::vogleditor_stateTreeTexEnvStateVecFloatItem(GLenum target, QString glenumName, GLenum name, unsigned int index, float *values, unsigned int numComponents, vogleditor_stateTreeItem *parent, vogl_texenv_state &state)
    : vogleditor_stateTreeTexEnvStateVecDiffableItem(target, name, index, numComponents, glenumName, "", parent),
      m_pState(&state)
{
    setValue(getValueFromFloats(values, numComponents));
}

bool vogleditor_stateTreeTexEnvStateVecFloatItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL || m_pDiffBaseInfo == NULL)
        return false;

    if (m_index >= this->m_pDiffBaseInfo->get_max_texture_coords())
    {
        // this index did not exist in the base state, so it must be new
        return true;
    }

    float curVals[4] = { 0, 0, 0, 0 };
    float baseVals[4] = { 0, 0, 0, 0 };
    if (m_pState->get_state(m_target).get<float>(m_name, m_index, curVals, m_numComponents) &&
        m_pDiffBaseState->get_state(m_target).get<float>(m_name, m_index, baseVals, m_numComponents))
    {
        return curVals[0] != baseVals[0] || curVals[1] != baseVals[1] || curVals[2] != baseVals[2] || curVals[3] != baseVals[3];
    }
    else
    {
        // the values could not be obtained from both states, so something must have changed
        return true;
    }
}

QString vogleditor_stateTreeTexEnvStateVecFloatItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL || m_pDiffBaseInfo == NULL)
        return "";

    if (m_index >= m_pDiffBaseInfo->get_max_texture_coords())
    {
        // this index did not exist in the base state, so it must be new
        return "non-existent";
    }

    float baseVals[4] = { 0, 0, 0, 0 };
    if (m_pDiffBaseState->get_state(m_target).get<float>(m_name, m_index, baseVals, m_numComponents))
    {
        return getValueFromFloats(baseVals, m_numComponents);
    }
    else
    {
        // the values could not be obtained from both states, so something must have changed
        return "non-existent";
    }
}
//=============================================================================

vogleditor_stateTreeTexEnvItem::vogleditor_stateTreeTexEnvItem(QString name, vogleditor_stateTreeItem *parent, vogl_texenv_state &state, const vogl_context_info &info)
    : vogleditor_stateTreeItem(name, "", parent),
      m_pState(&state),
      m_pDiffBaseState(NULL),
      m_pDiffBaseInfo(NULL)
{
    QString tmp;
    for (uint texcoord_index = 0; texcoord_index < info.get_max_texture_coords(); texcoord_index++)
    {
        vogleditor_stateTreeItem *pTexNode = new vogleditor_stateTreeItem(tmp.sprintf("GL_TEXTURE%u", texcoord_index), "", this);
        this->appendChild(pTexNode);

        int iVals[4] = { 0, 0, 0, 0 };
        float fVals[4] = { 0, 0, 0, 0 };
#define GET_ENUM(target, idx, name, num)                                                                                                                               \
    if (m_pState->get_state(target).get<int>(name, idx, iVals, num))                                                                                                   \
    {                                                                                                                                                                  \
        vogleditor_stateTreeTexEnvStateVecEnumItem *pItem = new vogleditor_stateTreeTexEnvStateVecEnumItem(target, #name, name, idx, iVals, num, pTexNode, *m_pState); \
        m_diffableItems.push_back(pItem);                                                                                                                              \
        pTexNode->appendChild(pItem);                                                                                                                                  \
    }
#define GET_FLOAT(target, idx, name, num)                                                                                                                                \
    if (m_pState->get_state(target).get<float>(name, idx, fVals, num))                                                                                                   \
    {                                                                                                                                                                    \
        vogleditor_stateTreeTexEnvStateVecFloatItem *pItem = new vogleditor_stateTreeTexEnvStateVecFloatItem(target, #name, name, idx, fVals, num, pTexNode, *m_pState); \
        m_diffableItems.push_back(pItem);                                                                                                                                \
        pTexNode->appendChild(pItem);                                                                                                                                    \
    }

        GET_FLOAT(GL_TEXTURE_FILTER_CONTROL, texcoord_index, GL_TEXTURE_LOD_BIAS, 1);
        GET_ENUM(GL_POINT_SPRITE, texcoord_index, GL_COORD_REPLACE, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_TEXTURE_ENV_MODE, 1);
        GET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_TEXTURE_ENV_COLOR, 4);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_COMBINE_RGB, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_COMBINE_ALPHA, 1);
        GET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_RGB_SCALE, 1);
        GET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_ALPHA_SCALE, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_SRC0_RGB, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_SRC1_RGB, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_SRC2_RGB, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_SRC0_ALPHA, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_SRC1_ALPHA, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_SRC2_ALPHA, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND0_RGB, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND1_RGB, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND2_RGB, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND0_ALPHA, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND1_ALPHA, 1);
        GET_ENUM(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND2_ALPHA, 1);

// TODO:
//{ "glGetTexEnv",	'E',	1,	"GL_SOURCE3_RGB_NV",  0x8583},
//{ "glGetTexEnv",	'E',	1,	"GL_SOURCE3_ALPHA_NV",  0x858B},
//{ "glGetTexEnv",	'E',	1,	"GL_OPERAND3_RGB_NV",  0x8593},
//{ "glGetTexEnv",	'E',	1,	"GL_OPERAND3_ALPHA_NV",  0x859B},
//{ "glGetTexEnv",	'E',	1,	"GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV",  0x86D9},
//{ "glGetTexEnv",	'E',	1,	"GL_SHADER_OPERATION_NV",  0x86DF},
//{ "glGetTexEnv",	'E',	4,	"GL_CULL_MODES_NV",  0x86E0},
//{ "glGetTexEnv",	'F',	4,	"GL_OFFSET_TEXTURE_MATRIX_NV",  0x86E1},
//{ "glGetTexEnv",	'F',	1,	"GL_OFFSET_TEXTURE_SCALE_NV",  0x86E2},
//{ "glGetTexEnv",	'F',	1,	"GL_OFFSET_TEXTURE_BIAS_NV",  0x86E3},
//{ "glGetTexEnv",	'E',	1,	"GL_PREVIOUS_TEXTURE_INPUT_NV",  0x86E4},
//{ "glGetTexEnv",	'F',	3,	"GL_CONST_EYE_NV",  0x86E5},
//{ "glGetTexEnv",	'E',	1,	"GL_BUMP_TARGET_ATI",  0x877C},

#undef GET_FLOAT
#undef GET_ENUM
#define GET_ENUM(target, idx, name, num)                                                                                                                                           \
    if (m_pState->get_state(target).get<int>(name, idx, iVals, num))                                                                                                               \
    {                                                                                                                                                                              \
        vogleditor_stateTreeTexEnvStateVecEnumItem *pItem = new vogleditor_stateTreeTexEnvStateVecEnumItem(target, #target " " #name, name, idx, iVals, num, pTexNode, *m_pState); \
        m_diffableItems.push_back(pItem);                                                                                                                                          \
        pTexNode->appendChild(pItem);                                                                                                                                              \
    }
#define GET_FLOAT(target, idx, name, num)                                                                                                                                            \
    if (m_pState->get_state(target).get<float>(name, idx, fVals, num))                                                                                                               \
    {                                                                                                                                                                                \
        vogleditor_stateTreeTexEnvStateVecFloatItem *pItem = new vogleditor_stateTreeTexEnvStateVecFloatItem(target, #target " " #name, name, idx, fVals, num, pTexNode, *m_pState); \
        m_diffableItems.push_back(pItem);                                                                                                                                            \
        pTexNode->appendChild(pItem);                                                                                                                                                \
    }

        GET_ENUM(GL_S, texcoord_index, GL_TEXTURE_GEN_MODE, 1);
        GET_FLOAT(GL_S, texcoord_index, GL_OBJECT_PLANE, 4);
        GET_FLOAT(GL_S, texcoord_index, GL_EYE_PLANE, 4);

        GET_ENUM(GL_T, texcoord_index, GL_TEXTURE_GEN_MODE, 1);
        GET_FLOAT(GL_T, texcoord_index, GL_OBJECT_PLANE, 4);
        GET_FLOAT(GL_T, texcoord_index, GL_EYE_PLANE, 4);

        GET_ENUM(GL_R, texcoord_index, GL_TEXTURE_GEN_MODE, 1);
        GET_FLOAT(GL_R, texcoord_index, GL_OBJECT_PLANE, 4);
        GET_FLOAT(GL_R, texcoord_index, GL_EYE_PLANE, 4);

        GET_ENUM(GL_Q, texcoord_index, GL_TEXTURE_GEN_MODE, 1);
        GET_FLOAT(GL_Q, texcoord_index, GL_OBJECT_PLANE, 4);
        GET_FLOAT(GL_Q, texcoord_index, GL_EYE_PLANE, 4);
#undef GET_FLOAT
#undef GET_ENUM
    }
}

void vogleditor_stateTreeTexEnvItem::set_diff_base_state(const vogl_context_info *pBaseInfo, const vogl_texenv_state *pBaseState)
{
    m_pDiffBaseState = pBaseState;
    m_pDiffBaseInfo = pBaseInfo;

    for (vogleditor_stateTreeTexEnvStateVecDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
    {
        (*iter)->set_diff_base_state(pBaseInfo, pBaseState);
    }
}
