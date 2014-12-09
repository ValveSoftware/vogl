#include "vogleditor_statetreetextureitem.h"

#include "vogl_texture_state.h"

void vogleditor_stateTreeTextureItem::set_diff_base_state(const vogl_texture_state *pBaseState)
{
    m_pDiffBaseState = pBaseState;
    for (vogleditor_stateTreeStateVecDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
    {
        (*iter)->set_diff_base_state(&(pBaseState->get_params()));
    }
}

vogleditor_stateTreeTextureItem::vogleditor_stateTreeTextureItem(QString name, QString value, vogleditor_stateTreeItem *parentNode, vogl_texture_state *pState, const vogl_context_info &info)
    : vogleditor_stateTreeItem(name, value, parentNode),
      m_pTexture(pState),
      m_pDiffBaseState(NULL)
{
    QString tmp;

    float fVals[16];
    int iVals[16];

#define STR_INT1(val) tmp.sprintf("%d", val[0])

#define GET_INT(name, num)                                                                                                                                       \
    memset(iVals, 0, sizeof(iVals));                                                                                                                             \
    if (m_pTexture->get_params().get<int>(name, 0, iVals, num))                                                                                                  \
    {                                                                                                                                                            \
        vogleditor_stateTreeStateVecIntItem *pItem = new vogleditor_stateTreeStateVecIntItem(#name, name, 0, m_pTexture->get_params(), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                        \
        this->appendChild(pItem);                                                                                                                                \
    }
#define GET_ENUM(name, num)                                                                                                                                        \
    memset(iVals, 0, sizeof(iVals));                                                                                                                               \
    if (m_pTexture->get_params().get<int>(name, 0, iVals, num))                                                                                                    \
    {                                                                                                                                                              \
        vogleditor_stateTreeStateVecEnumItem *pItem = new vogleditor_stateTreeStateVecEnumItem(#name, name, 0, m_pTexture->get_params(), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                          \
        this->appendChild(pItem);                                                                                                                                  \
    }
#define GET_FLOAT(name, num)                                                                                                                                         \
    memset(fVals, 0, sizeof(fVals));                                                                                                                                 \
    if (m_pTexture->get_params().get<float>(name, 0, fVals, num))                                                                                                    \
    {                                                                                                                                                                \
        vogleditor_stateTreeStateVecFloatItem *pItem = new vogleditor_stateTreeStateVecFloatItem(#name, name, 0, m_pTexture->get_params(), fVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                            \
        this->appendChild(pItem);                                                                                                                                    \
    }
    GET_INT(GL_TEXTURE_BASE_LEVEL, 1);
    int base_level = iVals[0];
    GET_INT(GL_TEXTURE_MAX_LEVEL, 1);
    GET_FLOAT(GL_TEXTURE_BORDER_COLOR, 4);
    GET_ENUM(GL_TEXTURE_COMPARE_MODE, 1);
    GET_ENUM(GL_TEXTURE_COMPARE_FUNC, 1);
    GET_FLOAT(GL_TEXTURE_LOD_BIAS, 1);
    GET_ENUM(GL_TEXTURE_MIN_FILTER, 1);
    GET_ENUM(GL_TEXTURE_MAG_FILTER, 1);
    GET_FLOAT(GL_TEXTURE_MIN_LOD, 1);
    GET_FLOAT(GL_TEXTURE_MAX_LOD, 1);
    GET_ENUM(GL_TEXTURE_SWIZZLE_R, 1);
    GET_ENUM(GL_TEXTURE_SWIZZLE_G, 1);
    GET_ENUM(GL_TEXTURE_SWIZZLE_B, 1);
    GET_ENUM(GL_TEXTURE_SWIZZLE_A, 1);
    GET_ENUM(GL_TEXTURE_SWIZZLE_RGBA, 4);
    GET_ENUM(GL_TEXTURE_WRAP_S, 1);
    GET_ENUM(GL_TEXTURE_WRAP_T, 1);
    GET_ENUM(GL_TEXTURE_WRAP_R, 1);

    if (!info.is_core_profile())
    {
        GET_ENUM(GL_GENERATE_MIPMAP, 1);
    }

    GET_INT(GL_TEXTURE_IMMUTABLE_FORMAT, 1);

    if (info.supports_extension("GL_EXT_texture_filter_anisotropic"))
    {
        GET_FLOAT(GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
    }

    if (info.supports_extension("GL_EXT_texture_sRGB_decode"))
    {
        GET_ENUM(GL_TEXTURE_SRGB_DECODE_EXT, 1);
    }

    if (!info.is_core_profile() && info.supports_extension("GL_ARB_shadow_ambient"))
    {
        GET_FLOAT(GL_TEXTURE_COMPARE_FAIL_VALUE_ARB, 1);
    }

    if (!info.is_core_profile())
    {
        GET_ENUM(GL_DEPTH_TEXTURE_MODE, 1);
        // TODO
        //GL_TEXTURE_PRIORITY
        //GL_TEXTURE_RESIDENT
    }

    int num_actual_levels = m_pTexture->get_num_levels();
    uint num_faces = (m_pTexture->get_target() == GL_TEXTURE_CUBE_MAP) ? 6 : 1;

    for (uint face = 0; face < num_faces; face++)
    {
        GLenum face_target_to_query = m_pTexture->get_target();
        if (m_pTexture->get_target() == GL_TEXTURE_CUBE_MAP)
            face_target_to_query = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

        vogleditor_stateTreeItem *pFaceNode = this;

        if (m_pTexture->get_target() == GL_TEXTURE_CUBE_MAP)
        {
            pFaceNode = new vogleditor_stateTreeItem(enum_to_string(face_target_to_query), "", this);
            this->appendChild(pFaceNode);
        }

        for (int level = base_level; level < num_actual_levels; level++)
        {
            vogleditor_stateTreeItem *pLevelNode = new vogleditor_stateTreeItem(tmp.sprintf("Mip level %d", level), "", pFaceNode);
            pFaceNode->appendChild(pLevelNode);

            const vogl_state_vector &level_params = m_pTexture->get_level_params(face, level);

// TODO: Check for core vs. compat profiles and not query the old stuff
#undef GET_INT
#undef GET_ENUM
#define GET_INT(name, num)                                                                                                             \
    if (level_params.get<int>(name, 0, iVals, num))                                                                                    \
    {                                                                                                                                  \
        pLevelNode->appendChild(new vogleditor_stateTreeStateVecIntItem(#name, name, 0, level_params, iVals, num, false, pLevelNode)); \
    }
#define GET_ENUM(name, num)                                                                                                             \
    if (level_params.get<int>(name, 0, iVals, num))                                                                                     \
    {                                                                                                                                   \
        pLevelNode->appendChild(new vogleditor_stateTreeStateVecEnumItem(#name, name, 0, level_params, iVals, num, false, pLevelNode)); \
    }
            GET_INT(GL_TEXTURE_WIDTH, 1);
            GET_INT(GL_TEXTURE_HEIGHT, 1);
            GET_INT(GL_TEXTURE_DEPTH, 1);
            GET_ENUM(GL_TEXTURE_INTERNAL_FORMAT, 1);

            GET_INT(GL_TEXTURE_SAMPLES, 1);
            GET_ENUM(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS, 1);

            GET_INT(GL_TEXTURE_RED_SIZE, 1);
            GET_INT(GL_TEXTURE_GREEN_SIZE, 1);
            GET_INT(GL_TEXTURE_BLUE_SIZE, 1);
            GET_INT(GL_TEXTURE_ALPHA_SIZE, 1);
            GET_INT(GL_TEXTURE_DEPTH_SIZE, 1);
            GET_INT(GL_TEXTURE_STENCIL_SIZE, 1);
            GET_INT(GL_TEXTURE_LUMINANCE_SIZE, 1);
            GET_INT(GL_TEXTURE_INTENSITY_SIZE, 1);
            GET_INT(GL_TEXTURE_SHARED_SIZE, 1);
            GET_INT(GL_TEXTURE_COMPRESSED, 1);
            bool is_compressed = (bool)iVals[0];

            if (info.supports_extension("GL_ARB_depth_texture"))
            {
                GET_INT(GL_TEXTURE_DEPTH_SIZE, 1);
                GET_INT(GL_TEXTURE_DEPTH_TYPE, 1);
            }

            if (info.supports_extension("GL_EXT_packed_depth_stencil"))
                GET_INT(GL_TEXTURE_STENCIL_SIZE_EXT, 1);

            if (m_pTexture->get_target() == GL_TEXTURE_BUFFER)
            {
                GET_INT(GL_TEXTURE_BUFFER_DATA_STORE_BINDING, 1);
                GET_INT(GL_TEXTURE_BUFFER_OFFSET, 1);
                GET_INT(GL_TEXTURE_BUFFER_SIZE, 1);
            }

            if (is_compressed)
            {
                GET_INT(GL_TEXTURE_COMPRESSED_IMAGE_SIZE, 1);
            }
        }
    }
#undef GET_FLOAT
#undef GET_ENUM
#undef GET_INT
}
