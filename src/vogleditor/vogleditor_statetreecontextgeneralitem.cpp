#include "vogleditor_statetreecontextgeneralitem.h"
#include "vogl_general_context_state.h"
#include "vogl_state_vector.h"

vogleditor_stateTreeContextGeneralCompressTextureFormatItem::vogleditor_stateTreeContextGeneralCompressTextureFormatItem(QString glenumName, GLenum name, unsigned int index, const vogl_state_vector &stateVec, int formatEnum, unsigned int numComponents, bool isIndexed, vogleditor_stateTreeItem *parent)
    : vogleditor_stateTreeDatatypeItem<int>(glenumName, name, index, stateVec, numComponents, isIndexed, parent),
      m_formatEnum(formatEnum),
      m_pDiffBaseGeneralState(NULL)
{
    static QString tmp;

    switch (numComponents)
    {
        case 1:
            setValue(enum_to_string(m_formatEnum));
            break;
        default:
            VOGL_ASSERT(!"Unhandled component count in vogleditor_stateTreeContextGeneralCompressTextureFormatItem");
            break;
    }
}

bool vogleditor_stateTreeContextGeneralCompressTextureFormatItem::hasChanged() const
{
    if (m_pDiffBaseGeneralState == NULL)
        return false;

    int base_num_formats = 0;
    m_pDiffBaseGeneralState->get(GL_NUM_COMPRESSED_TEXTURE_FORMATS, 0, &base_num_formats);

    if (base_num_formats == 0)
    {
        // the base had no formats, so any existing items are new.
        return true;
    }

    int *pFormats = vogl_new_array(int, base_num_formats);
    if (m_pDiffBaseGeneralState->get<int>(GL_COMPRESSED_TEXTURE_FORMATS, 0, pFormats, base_num_formats))
    {
        // search for the current format in the list of base formats
        for (int i = 0; i < base_num_formats; i++)
        {
            if (m_formatEnum == pFormats[i])
            {
                // the format was found, so consider this as unchanged
                vogl_delete_array(pFormats);
                return false;
            }
        }

        // the format was not found, consider it as changed
        vogl_delete_array(pFormats);
        return true;
    }
    else
    {
        // the enum must have been added, so it is changed
        vogl_delete_array(pFormats);
        return true;
    }
    vogl_delete_array(pFormats);
    return false;
}

//=============================================================================

vogleditor_stateTreeContextGeneralItem::vogleditor_stateTreeContextGeneralItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_general_context_state &generalState, const vogl_context_info &info)
    : vogleditor_stateTreeItem(name, value, parent),
      m_pState(&generalState),
      m_pDiffBaseState(NULL)
{
    float fVals[16];
    int iVals[16];
    bool bVals[4];

    memset(fVals, 0, sizeof(fVals));
    memset(iVals, 0, sizeof(iVals));
    memset(bVals, 0, sizeof(bVals));

    QString tmp;

#define GET_PTR(name, num)                                                                                                                              \
    if (generalState.get<int>(name, 0, iVals, num))                                                                                                     \
    {                                                                                                                                                   \
        vogleditor_stateTreeStateVecPtrItem *pPtrItem = new vogleditor_stateTreeStateVecPtrItem(#name, name, 0, generalState, iVals, num, false, this); \
        this->m_diffableItems.push_back(pPtrItem);                                                                                                      \
        this->appendChild(pPtrItem);                                                                                                                    \
    }
#define GET_BOOL(name, num)                                                                                                                                \
    if (generalState.get<bool>(name, 0, bVals, num))                                                                                                       \
    {                                                                                                                                                      \
        vogleditor_stateTreeStateVecBoolItem *pBoolItem = new vogleditor_stateTreeStateVecBoolItem(#name, name, 0, generalState, bVals, num, false, this); \
        this->m_diffableItems.push_back(pBoolItem);                                                                                                        \
        this->appendChild(pBoolItem);                                                                                                                      \
    }
#define GET_INT(name, num)                                                                                                                              \
    if (generalState.get<int>(name, 0, iVals, num))                                                                                                     \
    {                                                                                                                                                   \
        vogleditor_stateTreeStateVecIntItem *pIntItem = new vogleditor_stateTreeStateVecIntItem(#name, name, 0, generalState, iVals, num, false, this); \
        this->m_diffableItems.push_back(pIntItem);                                                                                                      \
        this->appendChild(pIntItem);                                                                                                                    \
    }
#define GET_ENUM(name, num)                                                                                                                                \
    if (generalState.get<int>(name, 0, iVals, num))                                                                                                        \
    {                                                                                                                                                      \
        vogleditor_stateTreeStateVecEnumItem *pEnumItem = new vogleditor_stateTreeStateVecEnumItem(#name, name, 0, generalState, iVals, num, false, this); \
        this->m_diffableItems.push_back(pEnumItem);                                                                                                        \
        this->appendChild(pEnumItem);                                                                                                                      \
    }
#define GET_FLOAT(name, num)                                                                                                                                  \
    if (generalState.get<float>(name, 0, fVals, num))                                                                                                         \
    {                                                                                                                                                         \
        vogleditor_stateTreeStateVecFloatItem *pFloatItem = new vogleditor_stateTreeStateVecFloatItem(#name, name, 0, generalState, fVals, num, false, this); \
        this->m_diffableItems.push_back(pFloatItem);                                                                                                          \
        this->appendChild(pFloatItem);                                                                                                                        \
    }

#define GET_MATRIX(name, num)                                                                                                                                    \
    if (generalState.get<float>(name, 0, fVals, num))                                                                                                            \
    {                                                                                                                                                            \
        vogleditor_stateTreeStateVecMatrixItem *pMatrixNode = new vogleditor_stateTreeStateVecMatrixItem(#name, name, 0, generalState, fVals, num, false, this); \
        this->m_diffableItems.push_back(pMatrixNode);                                                                                                            \
        this->appendChild(pMatrixNode);                                                                                                                          \
    }

#define STR_INT(val) tmp.sprintf("%d", val)

// TODO: Properly support diffing indexed state
#define GET_INDEXED_INT(name, num, totalIndices)                                                                                                                           \
    if (totalIndices > 0)                                                                                                                                                  \
    {                                                                                                                                                                      \
        vogleditor_stateTreeItem *pNode = new vogleditor_stateTreeItem(#name, "", this);                                                                                   \
        for (int i = 0; i < totalIndices; i++)                                                                                                                             \
        {                                                                                                                                                                  \
            if (generalState.get<int>(name, i, iVals, num, true))                                                                                                          \
            {                                                                                                                                                              \
                vogleditor_stateTreeStateVecIntItem *pIntItem = new vogleditor_stateTreeStateVecIntItem(STR_INT(i), name, i, generalState, &(iVals[i]), num, true, pNode); \
                this->m_diffableItems.push_back(pIntItem);                                                                                                                 \
                pNode->appendChild(pIntItem);                                                                                                                              \
            }                                                                                                                                                              \
        }                                                                                                                                                                  \
        pNode->setValue(tmp.sprintf("[%d]", pNode->childCount()));                                                                                                         \
        this->appendChild(pNode);                                                                                                                                          \
    }

#define GET_INDEXED_BOOL(name, num, totalIndices)                                                                                                                             \
    if (totalIndices > 0)                                                                                                                                                     \
    {                                                                                                                                                                         \
        vogleditor_stateTreeItem *pNode = new vogleditor_stateTreeItem(#name, "", this);                                                                                      \
        for (int i = 0; i < totalIndices; i++)                                                                                                                                \
        {                                                                                                                                                                     \
            if (generalState.get<bool>(name, i, bVals, num, true))                                                                                                            \
            {                                                                                                                                                                 \
                vogleditor_stateTreeStateVecBoolItem *pBoolItem = new vogleditor_stateTreeStateVecBoolItem(STR_INT(i), name, i, generalState, &(bVals[i]), num, true, pNode); \
                this->m_diffableItems.push_back(pBoolItem);                                                                                                                   \
                pNode->appendChild(pBoolItem);                                                                                                                                \
            }                                                                                                                                                                 \
        }                                                                                                                                                                     \
        pNode->setValue(tmp.sprintf("[%d]", pNode->childCount()));                                                                                                            \
        this->appendChild(pNode);                                                                                                                                             \
    }

    GET_INT(GL_ACCUM_ALPHA_BITS, 1);
    GET_INT(GL_ACCUM_BLUE_BITS, 1);
    GET_INT(GL_ACCUM_GREEN_BITS, 1);
    GET_INT(GL_ACCUM_RED_BITS, 1);
    GET_FLOAT(GL_ACCUM_CLEAR_VALUE, 4);
    GET_ENUM(GL_ACTIVE_TEXTURE, 1);
    GET_FLOAT(GL_ALIASED_LINE_WIDTH_RANGE, 2);
    GET_FLOAT(GL_ALIASED_POINT_SIZE_RANGE, 2);
    GET_FLOAT(GL_ALPHA_BIAS, 1);
    GET_INT(GL_ALPHA_BITS, 1);
    GET_FLOAT(GL_ALPHA_SCALE, 1);
    GET_BOOL(GL_ALPHA_TEST, 1);
    GET_ENUM(GL_ALPHA_TEST_FUNC, 1);
    GET_FLOAT(GL_ALPHA_TEST_REF, 1);
    GET_INT(GL_ARRAY_BUFFER_BINDING, 1);
    GET_INT(GL_ATOMIC_COUNTER_BUFFER_BINDING, 1);
    GET_INT(GL_ATTRIB_STACK_DEPTH, 1);
    GET_BOOL(GL_AUTO_NORMAL, 1);
    GET_INT(GL_AUX_BUFFERS, 1);

    GET_BOOL(GL_BLEND, 1);
    int maxDrawBuffers = info.get_max_draw_buffers();
    GET_INDEXED_BOOL(GL_BLEND, 1, maxDrawBuffers);
    GET_FLOAT(GL_BLEND_COLOR, 4);
    GET_ENUM(GL_BLEND_DST, 1);
    GET_ENUM(GL_BLEND_DST_ALPHA, 1);
    GET_ENUM(GL_BLEND_DST_RGB, 1);
    GET_ENUM(GL_BLEND_EQUATION_ALPHA, 1);
    GET_ENUM(GL_BLEND_EQUATION_RGB, 1);
    GET_ENUM(GL_BLEND_SRC, 1);
    GET_ENUM(GL_BLEND_SRC_ALPHA, 1);
    GET_ENUM(GL_BLEND_SRC_RGB, 1);
    GET_FLOAT(GL_BLUE_BIAS, 1);
    GET_INT(GL_BLUE_BITS, 1);
    GET_FLOAT(GL_BLUE_SCALE, 1);

    GET_INT(GL_CLIENT_ACTIVE_TEXTURE, 1);
    GET_INT(GL_CLIENT_ATTRIB_STACK_DEPTH, 1);
    GET_BOOL(GL_CLIP_PLANE0, 1);
    GET_BOOL(GL_CLIP_PLANE1, 1);
    GET_BOOL(GL_CLIP_PLANE2, 1);
    GET_BOOL(GL_CLIP_PLANE3, 1);
    GET_BOOL(GL_CLIP_PLANE4, 1);
    GET_BOOL(GL_CLIP_PLANE5, 1);
    GET_BOOL(GL_CLIP_DISTANCE6, 1);
    GET_BOOL(GL_CLIP_DISTANCE7, 1);
    GET_FLOAT(GL_COLOR_CLEAR_VALUE, 4);
    GET_BOOL(GL_COLOR_LOGIC_OP, 1);
    GET_BOOL(GL_COLOR_MATERIAL, 1);
    GET_ENUM(GL_COLOR_MATERIAL_FACE, 1);
    GET_ENUM(GL_COLOR_MATERIAL_PARAMETER, 1);
    GET_BOOL(GL_COLOR_ARRAY, 1);
    GET_INT(GL_COLOR_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_COLOR_ARRAY_POINTER, 1);
    GET_INT(GL_COLOR_ARRAY_SIZE, 1);
    GET_INT(GL_COLOR_ARRAY_STRIDE, 1);
    GET_ENUM(GL_COLOR_ARRAY_TYPE, 1);
    GET_MATRIX(GL_COLOR_MATRIX, 16);
    GET_INT(GL_COLOR_MATRIX_STACK_DEPTH, 1);
    GET_BOOL(GL_COLOR_SUM, 1);
    GET_BOOL(GL_COLOR_SUM_ARB, 1);
    GET_BOOL(GL_COLOR_TABLE, 1);
    GET_BOOL(GL_COLOR_WRITEMASK, 4);
    GET_INDEXED_BOOL(GL_COLOR_WRITEMASK, 4, maxDrawBuffers);

    GET_INT(GL_NUM_COMPRESSED_TEXTURE_FORMATS, 1);

    int num_compressed_texture_formats = 0;
    if (generalState.get(GL_NUM_COMPRESSED_TEXTURE_FORMATS, 0, &num_compressed_texture_formats))
    {
        if (num_compressed_texture_formats > 0)
        {
            int *pFormats = vogl_new_array(int, num_compressed_texture_formats);
            if (generalState.get<int>(GL_COMPRESSED_TEXTURE_FORMATS, 0, pFormats, num_compressed_texture_formats))
            {
                vogleditor_stateTreeItem *pNode = new vogleditor_stateTreeItem("GL_COMPRESSED_TEXTURE_FORMATS", tmp.sprintf("[%d]", num_compressed_texture_formats), this);
                for (int i = 0; i < num_compressed_texture_formats; i++)
                {
                    vogleditor_stateTreeContextGeneralCompressTextureFormatItem *pEnumItem = new vogleditor_stateTreeContextGeneralCompressTextureFormatItem(STR_INT(i), GL_COMPRESSED_TEXTURE_FORMATS, i, generalState, pFormats[i], 1, true, pNode);
                    m_formatItems.push_back(pEnumItem);
                    pNode->appendChild(pEnumItem);
                }
                this->appendChild(pNode);
            }
            vogl_delete_array(pFormats);
        }
    }

    GET_INT(GL_CONTEXT_FLAGS, 1);
    GET_BOOL(GL_CONVOLUTION_1D, 1);
    GET_BOOL(GL_CONVOLUTION_2D, 1);
    GET_INT(GL_COPY_READ_BUFFER_BINDING, 1);
    GET_INT(GL_COPY_WRITE_BUFFER_BINDING, 1);
    GET_BOOL(GL_CULL_FACE, 1);
    GET_ENUM(GL_CULL_FACE_MODE, 1);
    GET_FLOAT(GL_CURRENT_COLOR, 4);
    GET_FLOAT(GL_CURRENT_FOG_COORD, 1);
    GET_FLOAT(GL_CURRENT_INDEX, 1);
    GET_FLOAT(GL_CURRENT_NORMAL, 3);
    GET_INT(GL_CURRENT_PROGRAM, 1);
    GET_FLOAT(GL_CURRENT_RASTER_COLOR, 4);
    GET_FLOAT(GL_CURRENT_RASTER_DISTANCE, 1);
    GET_FLOAT(GL_CURRENT_RASTER_INDEX, 1);
    GET_FLOAT(GL_CURRENT_RASTER_POSITION, 4);
    GET_FLOAT(GL_CURRENT_RASTER_SECONDARY_COLOR, 4);
    GET_INT(GL_CURRENT_RASTER_POSITION_VALID, 1);
    GET_FLOAT(GL_CURRENT_SECONDARY_COLOR, 4);

    GET_INT(GL_DEBUG_GROUP_STACK_DEPTH, 1);
    GET_BOOL(GL_DEPTH_CLAMP, 1);
    GET_FLOAT(GL_DEPTH_BIAS, 1);
    GET_INT(GL_DEPTH_BITS, 1);
    GET_FLOAT(GL_DEPTH_SCALE, 1);
    GET_FLOAT(GL_DEPTH_CLEAR_VALUE, 1);
    GET_ENUM(GL_DEPTH_FUNC, 1);
    GET_FLOAT(GL_DEPTH_RANGE, 2);
    GET_BOOL(GL_DEPTH_TEST, 1);
    GET_BOOL(GL_DEPTH_WRITEMASK, 1);
    GET_INT(GL_DISPATCH_INDIRECT_BUFFER_BINDING, 1);
    GET_BOOL(GL_DITHER, 1);
    GET_BOOL(GL_DOUBLEBUFFER, 1);
    GET_ENUM(GL_DRAW_BUFFER, 1);
    GET_ENUM(GL_DRAW_BUFFER0, 1);
    GET_ENUM(GL_DRAW_BUFFER1, 1);
    GET_ENUM(GL_DRAW_BUFFER2, 1);
    GET_ENUM(GL_DRAW_BUFFER3, 1);
    GET_ENUM(GL_DRAW_BUFFER4, 1);
    GET_ENUM(GL_DRAW_BUFFER5, 1);
    GET_ENUM(GL_DRAW_BUFFER6, 1);
    GET_ENUM(GL_DRAW_BUFFER7, 1);
    GET_ENUM(GL_DRAW_BUFFER8, 1);
    GET_ENUM(GL_DRAW_BUFFER9, 1);
    GET_INT(GL_DRAW_FRAMEBUFFER_BINDING, 1);
    GET_INT(GL_DRAW_INDIRECT_BUFFER_BINDING, 1);

    GET_BOOL(GL_EDGE_FLAG, 1);
    GET_BOOL(GL_EDGE_FLAG_ARRAY, 1);
    GET_INT(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_EDGE_FLAG_ARRAY_POINTER, 1);
    GET_INT(GL_EDGE_FLAG_ARRAY_STRIDE, 1);
    GET_INT(GL_ELEMENT_ARRAY_BUFFER_BINDING, 1);

    GET_PTR(GL_FEEDBACK_BUFFER_POINTER, 1);
    GET_INT(GL_FEEDBACK_BUFFER_SIZE, 1);
    GET_ENUM(GL_FEEDBACK_BUFFER_TYPE, 1);
    GET_BOOL(GL_FOG, 1);
    GET_FLOAT(GL_FOG_COLOR, 4);
    GET_BOOL(GL_FOG_COORD_ARRAY, 1);
    GET_INT(GL_FOG_COORD_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_FOG_COORD_ARRAY_POINTER, 1);
    GET_INT(GL_FOG_COORD_ARRAY_STRIDE, 1);
    GET_ENUM(GL_FOG_COORD_ARRAY_TYPE, 1);
    GET_ENUM(GL_FOG_COORD_SRC, 1);
    GET_FLOAT(GL_FOG_DENSITY, 1);
    GET_FLOAT(GL_FOG_END, 1);
    GET_ENUM(GL_FOG_HINT, 1);
    GET_INT(GL_FOG_INDEX, 1);
    GET_ENUM(GL_FOG_MODE, 1);
    GET_FLOAT(GL_FOG_START, 1);
    GET_BOOL(GL_FRAGMENT_PROGRAM_ARB, 1);
    GET_ENUM(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, 1);
    GET_BOOL(GL_FRAMEBUFFER_SRGB, 1);
    GET_ENUM(GL_FRONT_FACE, 1);

    GET_ENUM(GL_GENERATE_MIPMAP_HINT, 1);
    GET_FLOAT(GL_GREEN_BIAS, 1);
    GET_FLOAT(GL_GREEN_BITS, 1);
    GET_FLOAT(GL_GREEN_SCALE, 1);

    GET_BOOL(GL_HISTOGRAM, 1);

    GET_ENUM(GL_IMPLEMENTATION_COLOR_READ_FORMAT, 1);
    GET_ENUM(GL_IMPLEMENTATION_COLOR_READ_TYPE, 1);
    GET_BOOL(GL_INDEX_ARRAY, 1);
    GET_INT(GL_INDEX_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_INDEX_ARRAY_POINTER, 1);
    GET_INT(GL_INDEX_ARRAY_STRIDE, 1);
    GET_ENUM(GL_INDEX_ARRAY_TYPE, 1);
    GET_INT(GL_INDEX_BITS, 1);
    GET_INT(GL_INDEX_CLEAR_VALUE, 1);
    GET_ENUM(GL_INDEX_LOGIC_OP, 1); // these (GL_INDEX_LOGIC_OP & GL_LOGIC_OP) got merged? or replaced with GL_COLOR_LOGIC_OP
    GET_BOOL(GL_INDEX_MODE, 1);
    GET_INT(GL_INDEX_OFFSET, 1);
    GET_INT(GL_INDEX_SHIFT, 1);
    GET_INT(GL_INDEX_WRITEMASK, 1);

    GET_ENUM(GL_LAYER_PROVOKING_VERTEX, 1);
    GET_BOOL(GL_LIGHTING, 1);
    GET_BOOL(GL_LIGHT0, 1);
    GET_BOOL(GL_LIGHT1, 1);
    GET_BOOL(GL_LIGHT2, 1);
    GET_BOOL(GL_LIGHT3, 1);
    GET_BOOL(GL_LIGHT4, 1);
    GET_BOOL(GL_LIGHT5, 1);
    GET_BOOL(GL_LIGHT6, 1);
    GET_BOOL(GL_LIGHT7, 1);
    GET_FLOAT(GL_LIGHT_MODEL_AMBIENT, 4);
    GET_ENUM(GL_LIGHT_MODEL_COLOR_CONTROL, 1);
    GET_BOOL(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
    GET_BOOL(GL_LIGHT_MODEL_TWO_SIDE, 1);
    GET_ENUM(GL_LINE_SMOOTH, 1);
    GET_ENUM(GL_LINE_SMOOTH_HINT, 1);
    GET_FLOAT(GL_LINE_WIDTH, 1);
    GET_BOOL(GL_LINE_STIPPLE, 1);
    GET_INT(GL_LINE_STIPPLE_PATTERN, 1);
    GET_INT(GL_LINE_STIPPLE_REPEAT, 1);
    GET_FLOAT(GL_LINE_WIDTH_GRANULARITY, 1); // replaced with GL_SMOOTH_LINE_WIDTH_GRANULARITY
    GET_FLOAT(GL_LINE_WIDTH_RANGE, 2);       // replaced with GL_SMOOTH_LINE_WIDTH_RANGE
    GET_INT(GL_LIST_BASE, 1);
    GET_INT(GL_LIST_INDEX, 1);
    GET_ENUM(GL_LIST_MODE, 1);
    GET_ENUM(GL_LOGIC_OP, 1); // these (GL_INDEX_LOGIC_OP & GL_LOGIC_OP) got merged? or replaced with GL_COLOR_LOGIC_OP
    GET_ENUM(GL_LOGIC_OP_MODE, 1);

    GET_BOOL(GL_MAP1_COLOR_4, 1);
    GET_FLOAT(GL_MAP1_GRID_DOMAIN, 2);
    GET_INT(GL_MAP1_GRID_SEGMENTS, 1);
    GET_BOOL(GL_MAP1_INDEX, 1);
    GET_BOOL(GL_MAP1_NORMAL, 1);
    GET_BOOL(GL_MAP1_TEXTURE_COORD_1, 1);
    GET_BOOL(GL_MAP1_TEXTURE_COORD_2, 1);
    GET_BOOL(GL_MAP1_TEXTURE_COORD_3, 1);
    GET_BOOL(GL_MAP1_TEXTURE_COORD_4, 1);
    GET_BOOL(GL_MAP1_VERTEX_3, 1);
    GET_BOOL(GL_MAP1_VERTEX_4, 1);
    GET_BOOL(GL_MAP2_COLOR_4, 1);
    GET_FLOAT(GL_MAP2_GRID_DOMAIN, 4);
    GET_INT(GL_MAP2_GRID_SEGMENTS, 2);
    GET_BOOL(GL_MAP2_INDEX, 1);
    GET_BOOL(GL_MAP2_NORMAL, 1);
    GET_BOOL(GL_MAP2_TEXTURE_COORD_1, 1);
    GET_BOOL(GL_MAP2_TEXTURE_COORD_2, 1);
    GET_BOOL(GL_MAP2_TEXTURE_COORD_3, 1);
    GET_BOOL(GL_MAP2_TEXTURE_COORD_4, 1);
    GET_BOOL(GL_MAP2_VERTEX_3, 1);
    GET_BOOL(GL_MAP2_VERTEX_4, 1);
    GET_BOOL(GL_MAP_COLOR, 1);
    GET_BOOL(GL_MAP_STENCIL, 1);
    GET_ENUM(GL_MATRIX_MODE, 1);

    GET_INT(GL_MAX_3D_TEXTURE_SIZE, 1);

    GET_INT(GL_MAX_ARRAY_TEXTURE_LAYERS, 1);
    GET_INT(GL_MAX_ATTRIB_STACK_DEPTH, 1);
    GET_INT(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH, 1);
    GET_INT(GL_MAX_CLIP_PLANES, 1);    // aka GL_MAX_CLIP_DISTANCES
    GET_INT(GL_MAX_CLIP_DISTANCES, 1); // alias to GL_MAX_CLIP_PLANES
    GET_INT(GL_MAX_COLOR_ATTACHMENTS, 1);
    GET_INT(GL_MAX_COLOR_MATRIX_STACK_DEPTH, 1);
    GET_INT(GL_MAX_COLOR_TEXTURE_SAMPLES, 1);
    GET_INT(GL_MAX_COMBINED_ATOMIC_COUNTERS, 1);
    GET_INT(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, 1);
    GET_INT(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 1);
    GET_INT(GL_MAX_COMBINED_UNIFORM_BLOCKS, 1);
    GET_INT(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS, 1);
    GET_INT(GL_MAX_COMPUTE_ATOMIC_COUNTERS, 1);
    GET_INT(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, 1);
    GET_INT(GL_MAX_COMPUTE_UNIFORM_BLOCKS, 1);
    GET_INT(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1);
    GET_INT(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, 1);
    GET_INT(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1);
    GET_INT(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, 1);
    GET_INT(GL_MAX_CUBE_MAP_TEXTURE_SIZE, 1);
    GET_INT(GL_MAX_DEBUG_GROUP_STACK_DEPTH, 1);
    GET_INT(GL_MAX_DEPTH_TEXTURE_SAMPLES, 1);
    GET_INT(GL_MAX_DRAW_BUFFERS, 1);
    GET_INT(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, 1);

    GET_INT(GL_MAX_ELEMENT_INDEX, 1);
    GET_INT(GL_MAX_ELEMENTS_INDICES, 1);
    GET_INT(GL_MAX_ELEMENTS_VERTICES, 1);
    GET_INT(GL_MAX_EVAL_ORDER, 1);
    GET_INT(GL_MAX_FRAGMENT_ATOMIC_COUNTERS, 1);
    GET_INT(GL_MAX_FRAGMENT_INPUT_COMPONENTS, 1);
    GET_INT(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, 1);
    GET_INT(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, 1);
    GET_INT(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_FRAGMENT_UNIFORM_VECTORS, 1);
    GET_INT(GL_MAX_FRAMEBUFFER_HEIGHT, 1);
    GET_INT(GL_MAX_FRAMEBUFFER_LAYERS, 1);
    GET_INT(GL_MAX_FRAMEBUFFER_SAMPLES, 1);
    GET_INT(GL_MAX_FRAMEBUFFER_WIDTH, 1);
    GET_INT(GL_MAX_GEOMETRY_ATOMIC_COUNTERS, 1);
    GET_INT(GL_MAX_GEOMETRY_INPUT_COMPONENTS, 1);
    GET_INT(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS, 1);
    GET_INT(GL_MAX_GEOMETRY_OUTPUT_VERTICES, 1);
    GET_INT(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS, 1);
    GET_INT(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, 1);
    GET_INT(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS, 1);
    GET_INT(GL_MAX_GEOMETRY_UNIFORM_BLOCKS, 1);
    GET_INT(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_INTEGER_SAMPLES, 1);

    GET_INT(GL_MAX_LABEL_LENGTH, 1);
    GET_INT(GL_MAX_LIGHTS, 1);
    GET_INT(GL_MAX_LIST_NESTING, 1);
    GET_INT(GL_MAX_MODELVIEW_STACK_DEPTH, 1);
    GET_INT(GL_MAX_NAME_STACK_DEPTH, 1);
    GET_INT(GL_MAX_PIXEL_MAP_TABLE, 1);
    GET_FLOAT(GL_MAX_PROGRAM_TEXEL_OFFSET, 1);
    GET_FLOAT(GL_MIN_PROGRAM_TEXEL_OFFSET, 1);
    GET_INT(GL_MAX_PROJECTION_STACK_DEPTH, 1);
    GET_INT(GL_MAX_RECTANGLE_TEXTURE_SIZE, 1);
    GET_INT(GL_MAX_RENDERBUFFER_SIZE, 1);
    GET_INT(GL_MAX_SAMPLE_MASK_WORDS, 1);
    GET_INT(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 1);
    GET_INT(GL_MAX_SERVER_WAIT_TIMEOUT, 1);
    GET_INT(GL_MAX_TEXTURE_BUFFER_SIZE, 1);
    GET_INT(GL_MAX_TEXTURE_COORDS, 1);
    GET_INT(GL_MAX_TEXTURE_IMAGE_UNITS, 1);
    GET_FLOAT(GL_MAX_TEXTURE_LOD_BIAS, 1);
    GET_INT(GL_MAX_TEXTURE_SIZE, 1);
    GET_INT(GL_MAX_TEXTURE_STACK_DEPTH, 1);
    GET_INT(GL_MAX_TEXTURE_UNITS, 1);
    GET_INT(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS, 1);
    GET_INT(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS, 1);
    GET_INT(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS, 1);
    GET_INT(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS, 1);
    GET_INT(GL_MAX_UNIFORM_BLOCK_SIZE, 1);
    GET_INT(GL_MAX_UNIFORM_BUFFER_BINDINGS, 1);
    GET_INT(GL_MAX_UNIFORM_LOCATIONS, 1);
    GET_INT(GL_MAX_VARYING_VECTORS, 1);
    GET_INT(GL_MAX_VERTEX_ATOMIC_COUNTERS, 1);
    GET_INT(GL_MAX_VERTEX_ATTRIBS, 1);
    GET_INT(GL_MAX_VERTEX_ATTRIB_BINDINGS, 1);
    GET_INT(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, 1);
    GET_INT(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, 1);
    GET_INT(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, 1);
    GET_INT(GL_MAX_VERTEX_UNIFORM_BLOCKS, 1);
    GET_INT(GL_MAX_VERTEX_UNIFORM_COMPONENTS, 1);
    GET_INT(GL_MAX_VERTEX_UNIFORM_VECTORS, 1);
    GET_INT(GL_MAX_VARYING_COMPONENTS, 1); // alias of GL_MAX_VARYING_FLOATS from GL2
    GET_INT(GL_MAX_VARYING_FLOATS, 1);
    GET_INT(GL_MAX_VERTEX_OUTPUT_COMPONENTS, 1);
    GET_INT(GL_MAX_VIEWPORTS, 1);
    GET_FLOAT(GL_MAX_VIEWPORT_DIMS, 2);
    GET_FLOAT(GL_MIN_SAMPLE_SHADING_VALUE, 1);
    GET_INT(GL_MIN_MAP_BUFFER_ALIGNMENT, 1);

    GET_INT(GL_MAJOR_VERSION, 1);
    GET_INT(GL_MINOR_VERSION, 1);

    GET_MATRIX(GL_MODELVIEW_MATRIX, 16);
    GET_INT(GL_MODELVIEW_STACK_DEPTH, 1);
    GET_BOOL(GL_MULTISAMPLE, 1);
    GET_BOOL(GL_MINMAX, 1);

    GET_INT(GL_NAME_STACK_DEPTH, 1);
    GET_BOOL(GL_NORMAL_ARRAY, 1);
    GET_INT(GL_NORMAL_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_NORMAL_ARRAY_POINTER, 1);
    GET_INT(GL_NORMAL_ARRAY_STRIDE, 1);
    GET_ENUM(GL_NORMAL_ARRAY_TYPE, 1);
    GET_BOOL(GL_NORMALIZE, 1);
    GET_INT(GL_NUM_EXTENSIONS, 1);
    GET_INT(GL_NUM_PROGRAM_BINARY_FORMATS, 1);
    GET_INT(GL_NUM_SHADER_BINARY_FORMATS, 1);

    GET_INT(GL_PACK_ALIGNMENT, 1);
    GET_INT(GL_PACK_IMAGE_HEIGHT, 1);
    GET_BOOL(GL_PACK_LSB_FIRST, 1);
    GET_INT(GL_PACK_ROW_LENGTH, 1);
    GET_INT(GL_PACK_SKIP_IMAGES, 1);
    GET_INT(GL_PACK_SKIP_PIXELS, 1);
    GET_INT(GL_PACK_SKIP_ROWS, 1);
    GET_BOOL(GL_PACK_SWAP_BYTES, 1);

    GET_FLOAT(GL_POINT_SIZE, 1);
    GET_FLOAT(GL_POINT_SIZE_GRANULARITY, 1);
    GET_FLOAT(GL_POINT_SIZE_RANGE, 2);
    GET_BOOL(GL_POLYGON_SMOOTH, 1);
    GET_ENUM(GL_POLYGON_SMOOTH_HINT, 1);
    GET_ENUM(GL_PERSPECTIVE_CORRECTION_HINT, 1);
    GET_INT(GL_PIXEL_MAP_A_TO_A_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_B_TO_B_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_G_TO_G_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_I_TO_A_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_I_TO_B_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_I_TO_G_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_I_TO_I_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_I_TO_R_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_R_TO_R_SIZE, 1);
    GET_INT(GL_PIXEL_MAP_S_TO_S_SIZE, 1);
    GET_INT(GL_PIXEL_PACK_BUFFER_BINDING, 1);
    GET_INT(GL_PIXEL_UNPACK_BUFFER_BINDING, 1);
    GET_BOOL(GL_POINT_SMOOTH, 1);
    GET_ENUM(GL_POINT_SMOOTH_HINT, 1);
    GET_ENUM(GL_POLYGON_MODE, 2);
    GET_FLOAT(GL_POLYGON_OFFSET_FACTOR, 1);
    GET_BOOL(GL_POLYGON_OFFSET_FILL, 1);
    GET_BOOL(GL_POLYGON_OFFSET_LINE, 1);
    GET_BOOL(GL_POLYGON_OFFSET_POINT, 1);
    GET_FLOAT(GL_POLYGON_OFFSET_UNITS, 1);
    GET_BOOL(GL_POLYGON_STIPPLE, 1);
    GET_FLOAT(GL_POINT_SIZE_MAX, 1);
    GET_FLOAT(GL_POINT_SIZE_MIN, 1);
    GET_FLOAT(GL_POINT_DISTANCE_ATTENUATION, 3);
    GET_FLOAT(GL_POINT_FADE_THRESHOLD_SIZE, 1);
    GET_BOOL(GL_POINT_SPRITE, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_ALPHA_BIAS, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_ALPHA_SCALE, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_BLUE_BIAS, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_BLUE_SCALE, 1);
    GET_INT(GL_POST_COLOR_MATRIX_COLOR_TABLE, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_GREEN_BIAS, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_GREEN_SCALE, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_RED_BIAS, 1);
    GET_FLOAT(GL_POST_COLOR_MATRIX_RED_SCALE, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_ALPHA_BIAS, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_ALPHA_SCALE, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_BLUE_BIAS, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_BLUE_SCALE, 1);
    GET_INT(GL_POST_CONVOLUTION_COLOR_TABLE, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_GREEN_BIAS, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_GREEN_SCALE, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_RED_BIAS, 1);
    GET_FLOAT(GL_POST_CONVOLUTION_RED_SCALE, 1);
    GET_BOOL(GL_PRIMITIVE_RESTART, 1);
    GET_INT(GL_PRIMITIVE_RESTART_INDEX, 1);
    GET_MATRIX(GL_PROJECTION_MATRIX, 16);
    GET_INT(GL_PROJECTION_STACK_DEPTH, 1);
    GET_INT(GL_PROGRAM_BINARY_FORMATS, 1);
    GET_INT(GL_PROGRAM_PIPELINE_BINDING, 1);
    GET_BOOL(GL_PROGRAM_POINT_SIZE, 1); // alias of GL_VERTEX_PROGRAM_POINT_SIZE from GL2
    GET_ENUM(GL_PROVOKING_VERTEX, 1);

    GET_ENUM(GL_READ_BUFFER, 1);
    GET_INT(GL_READ_FRAMEBUFFER_BINDING, 1);
    GET_FLOAT(GL_RED_BIAS, 1);
    GET_INT(GL_RED_BITS, 1);
    GET_FLOAT(GL_RED_SCALE, 1);
    GET_INT(GL_RENDERBUFFER_BINDING, 1);
    GET_ENUM(GL_RENDER_MODE, 1);
    GET_BOOL(GL_RESCALE_NORMAL, 1);
    GET_BOOL(GL_RGBA_MODE, 1);

    GET_INT(GL_SAMPLES, 1);
    GET_INT(GL_SAMPLE_BUFFERS, 1);
    GET_BOOL(GL_SAMPLE_COVERAGE_INVERT, 1);
    GET_FLOAT(GL_SAMPLE_COVERAGE_VALUE, 1);
    GET_BOOL(GL_SAMPLE_ALPHA_TO_COVERAGE, 1);
    GET_BOOL(GL_SAMPLE_ALPHA_TO_ONE, 1);
    GET_BOOL(GL_SAMPLE_COVERAGE, 1);
    GET_BOOL(GL_SAMPLE_MASK, 1);
    GET_BOOL(GL_SAMPLE_SHADING, 1);
    GET_INT(GL_SCISSOR_BOX, 4);
    GET_BOOL(GL_SCISSOR_TEST, 1);
    GET_INT(GL_SECONDARY_COLOR_ARRAY_SIZE, 1);
    GET_INT(GL_SECONDARY_COLOR_ARRAY_STRIDE, 1);
    GET_ENUM(GL_SECONDARY_COLOR_ARRAY_TYPE, 1);
    GET_PTR(GL_SECONDARY_COLOR_ARRAY_POINTER, 1);
    GET_INT(GL_SECONDARY_COLOR_ARRAY, 1);
    GET_INT(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_SELECTION_BUFFER_POINTER, 1);
    GET_INT(GL_SELECTION_BUFFER_SIZE, 1);
    GET_BOOL(GL_SEPARABLE_2D, 1);
    GET_ENUM(GL_SHADE_MODEL, 1);
    GET_BOOL(GL_SHADER_COMPILER, 1);
    GET_INT(GL_SHADER_STORAGE_BUFFER_BINDING, 1);
    GET_INT(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, 1);
    GET_INT(GL_SHADER_STORAGE_BUFFER_SIZE, 1);
    GET_INT(GL_SHADER_STORAGE_BUFFER_START, 1);
    GET_FLOAT(GL_SMOOTH_POINT_SIZE_GRANULARITY, 1);
    GET_FLOAT(GL_SMOOTH_POINT_SIZE_RANGE, 2);
    GET_FLOAT(GL_SMOOTH_LINE_WIDTH_GRANULARITY, 1);
    GET_FLOAT(GL_SMOOTH_LINE_WIDTH_RANGE, 2);
    GET_ENUM(GL_STENCIL_BACK_FAIL, 1);
    GET_ENUM(GL_STENCIL_BACK_FUNC, 1);
    GET_ENUM(GL_STENCIL_BACK_PASS_DEPTH_FAIL, 1);
    GET_ENUM(GL_STENCIL_BACK_PASS_DEPTH_PASS, 1);
    GET_ENUM(GL_STENCIL_BACK_REF, 1);
    GET_ENUM(GL_STENCIL_BACK_VALUE_MASK, 1);
    GET_ENUM(GL_STENCIL_BACK_WRITEMASK, 1);
    GET_INT(GL_STENCIL_BITS, 1);
    GET_INT(GL_STENCIL_CLEAR_VALUE, 1);
    GET_ENUM(GL_STENCIL_FAIL, 1);
    GET_ENUM(GL_STENCIL_FUNC, 1);
    GET_ENUM(GL_STENCIL_PASS_DEPTH_FAIL, 1);
    GET_ENUM(GL_STENCIL_PASS_DEPTH_PASS, 1);
    GET_INT(GL_STENCIL_REF, 1);
    GET_BOOL(GL_STENCIL_TEST, 1);
    GET_ENUM(GL_STENCIL_VALUE_MASK, 1);
    GET_ENUM(GL_STENCIL_WRITEMASK, 1);
    GET_BOOL(GL_STEREO, 1);
    GET_INT(GL_SUBPIXEL_BITS, 1);

#define GET_TC_BOOL(name, index, num)                                                                                                                                  \
    if (generalState.get<bool>(name, index, bVals, num))                                                                                                               \
    {                                                                                                                                                                  \
        vogleditor_stateTreeStateVecBoolItem *pBoolItem = new vogleditor_stateTreeStateVecBoolItem(#name, name, index, generalState, bVals, num, false, pTexUnitNode); \
        this->m_diffableItems.push_back(pBoolItem);                                                                                                                    \
        pTexUnitNode->appendChild(pBoolItem);                                                                                                                          \
    }
#define GET_TC_INT(name, index, num)                                                                                                                                \
    if (generalState.get<int>(name, index, iVals, num))                                                                                                             \
    {                                                                                                                                                               \
        vogleditor_stateTreeStateVecIntItem *pIntItem = new vogleditor_stateTreeStateVecIntItem(#name, name, index, generalState, iVals, num, false, pTexUnitNode); \
        this->m_diffableItems.push_back(pIntItem);                                                                                                                  \
        pTexUnitNode->appendChild(pIntItem);                                                                                                                        \
    }
#define GET_TC_FLOAT(name, index, num)                                                                                                                                    \
    if (generalState.get<float>(name, index, fVals, num))                                                                                                                 \
    {                                                                                                                                                                     \
        vogleditor_stateTreeStateVecFloatItem *pFloatItem = new vogleditor_stateTreeStateVecFloatItem(#name, name, index, generalState, fVals, num, false, pTexUnitNode); \
        this->m_diffableItems.push_back(pFloatItem);                                                                                                                      \
        pTexUnitNode->appendChild(pFloatItem);                                                                                                                            \
    }

    const uint max_texture_unit = info.get_max_texture_image_units();
    for (unsigned int texcoord_index = 0; texcoord_index < max_texture_unit; texcoord_index++)
    {
        vogleditor_stateTreeItem *pTexUnitNode = new vogleditor_stateTreeItem(tmp.sprintf("GL_TEXTURE%u", texcoord_index), "", this);
        this->appendChild(pTexUnitNode);

        GET_TC_INT(GL_SAMPLER_BINDING, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_1D, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_1D, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_2D, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_2D, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_3D, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_3D, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_CUBE_MAP, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_CUBE_MAP, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_RECTANGLE, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_RECTANGLE, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_BUFFER, texcoord_index, 1);

        GET_TC_INT(GL_TEXTURE_BINDING_1D_ARRAY, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_2D_ARRAY, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_2D_MULTISAMPLE, texcoord_index, 1);
        GET_TC_INT(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, texcoord_index, 1);

        GET_TC_FLOAT(GL_CURRENT_RASTER_TEXTURE_COORDS, texcoord_index, 4);
        GET_TC_FLOAT(GL_CURRENT_TEXTURE_COORDS, texcoord_index, 4);
        GET_TC_BOOL(GL_TEXTURE_GEN_Q, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_GEN_R, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_GEN_S, texcoord_index, 1);
        GET_TC_BOOL(GL_TEXTURE_GEN_T, texcoord_index, 1);
    }

#undef GET_TC_BOOL
#undef GET_TC_INT
#undef GET_TC_FLOAT

    GET_INT(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, 1);
    GET_BOOL(GL_TEXTURE_CUBE_MAP_SEAMLESS, 1);

    GET_ENUM(GL_TEXTURE_COMPRESSION_HINT, 1);
    GET_BOOL(GL_TEXTURE_COORD_ARRAY, 1);
    GET_INT(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_TEXTURE_COORD_ARRAY_POINTER, 1);
    GET_INT(GL_TEXTURE_COORD_ARRAY_STRIDE, 1);
    GET_INT(GL_TEXTURE_COORD_ARRAY_SIZE, 1);
    GET_ENUM(GL_TEXTURE_COORD_ARRAY_TYPE, 1);
    GET_FLOAT(GL_TEXTURE_ENV_COLOR, 4);
    GET_ENUM(GL_TEXTURE_ENV_MODE, 1);
    GET_MATRIX(GL_TEXTURE_MATRIX, 16);
    GET_INT(GL_TEXTURE_STACK_DEPTH, 1);
    GET_INT(GL_TIMESTAMP, 1);
    GET_INT(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 1);
    GET_MATRIX(GL_TRANSPOSE_COLOR_MATRIX, 16);
    GET_MATRIX(GL_TRANSPOSE_MODELVIEW_MATRIX, 16);
    GET_MATRIX(GL_TRANSPOSE_PROJECTION_MATRIX, 16);
    GET_MATRIX(GL_TRANSPOSE_TEXTURE_MATRIX, 16);
    int max_transform_feedback_separate_attribs = info.get_max_transform_feedback_separate_attribs();
    GET_INDEXED_INT(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 1, max_transform_feedback_separate_attribs);
    GET_INDEXED_INT(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 1, max_transform_feedback_separate_attribs);
    GET_INDEXED_INT(GL_TRANSFORM_FEEDBACK_BUFFER_START, 1, max_transform_feedback_separate_attribs);

    GET_INT(GL_UNPACK_ALIGNMENT, 1);
    GET_BOOL(GL_UNPACK_LSB_FIRST, 1);
    GET_INT(GL_UNPACK_ROW_LENGTH, 1);
    GET_INT(GL_UNPACK_SKIP_PIXELS, 1);
    GET_INT(GL_UNPACK_SKIP_ROWS, 1);
    GET_BOOL(GL_UNPACK_SWAP_BYTES, 1);
    GET_INT(GL_UNPACK_IMAGE_HEIGHT, 1);
    GET_INT(GL_UNPACK_SKIP_IMAGES, 1);
    GET_INT(GL_UNIFORM_BUFFER_BINDING, 1);
    int max_uniform_buffer_bindings = info.get_max_uniform_buffer_bindings();
    GET_INDEXED_INT(GL_UNIFORM_BUFFER_BINDING, 1, max_uniform_buffer_bindings)
    GET_INDEXED_INT(GL_UNIFORM_BUFFER_START, 1, max_uniform_buffer_bindings)
    GET_INDEXED_INT(GL_UNIFORM_BUFFER_SIZE, 1, max_uniform_buffer_bindings)
    GET_INT(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, 1);

    GET_BOOL(GL_VERTEX_ARRAY, 1);
    GET_INT(GL_VERTEX_ARRAY_BINDING, 1);
    GET_INT(GL_VERTEX_ARRAY_BUFFER_BINDING, 1);
    GET_PTR(GL_VERTEX_ARRAY_POINTER, 1);
    GET_INT(GL_VERTEX_ARRAY_SIZE, 1);
    GET_INT(GL_VERTEX_ARRAY_STRIDE, 1);
    GET_ENUM(GL_VERTEX_ARRAY_TYPE, 1);
    GET_INT(GL_VERTEX_BINDING_DIVISOR, 1);
    GET_INT(GL_VERTEX_BINDING_OFFSET, 1);
    GET_INT(GL_VERTEX_BINDING_STRIDE, 1);
    GET_BOOL(GL_VERTEX_PROGRAM_ARB, 1);
    GET_BOOL(GL_VERTEX_PROGRAM_POINT_SIZE, 1); // aka GL_PROGRAM_POINT_SIZE
    GET_BOOL(GL_VERTEX_PROGRAM_TWO_SIDE, 1);
    GET_FLOAT(GL_VERTEX_PROGRAM_POINT_SIZE_ARB, 1);
    GET_BOOL(GL_VERTEX_PROGRAM_TWO_SIDE_ARB, 1);
    GET_INT(GL_VIEWPORT, 4);
    GET_FLOAT(GL_VIEWPORT_BOUNDS_RANGE, 2);
    GET_ENUM(GL_VIEWPORT_INDEX_PROVOKING_VERTEX, 1);
    GET_INT(GL_VIEWPORT_SUBPIXEL_BITS, 1);

    GET_FLOAT(GL_ZOOM_X, 1);
    GET_FLOAT(GL_ZOOM_Y, 1);

#undef STR_INT

#undef GET_PTR
#undef GET_BOOL
#undef GET_INT
#undef GET_ENUM
#undef GET_FLOAT
#undef GET_MATRIX
#undef GET_INDEXED_INT
#undef GET_INDEXED_BOOL
}
