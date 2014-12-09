#include "vogleditor_statetreecontextitem.h"
#include "vogleditor_statetreearbprogramitem.h"
#include "vogleditor_statetreearbprogramenvitem.h"
#include "vogleditor_statetreedisplaylistitem.h"
#include "vogleditor_statetreeframebufferitem.h"
#include "vogleditor_statetreematrixitem.h"
#include "vogleditor_statetreepolygonstippleitem.h"
#include "vogleditor_statetreeprogramitem.h"
#include "vogleditor_statetreequeryitem.h"
#include "vogleditor_statetreerenderbufferitem.h"
#include "vogleditor_statetreeshaderitem.h"
#include "vogleditor_statetreesyncitem.h"
#include "vogleditor_statetreetexenvitem.h"
#include "vogleditor_statetreevertexarrayitem.h"
#include "vogleditor_statetreeprogrampipelineitem.h"
#include "vogl_sync_object.h"
#include "vogleditor_statetreetexenvitem.h"
#include "vogleditor_statetreesampleritem.h"
#include "vogleditor_statetreetextureitem.h"
#include "vogleditor_statetreelightitem.h"
#include "vogleditor_statetreecontextgeneralitem.h"
#include "vogleditor_statetreecontextinfoitem.h"
#include "vogleditor_statetreebufferitem.h"

struct vogl_gl_object_state_handle_less_than
{
    inline bool operator()(const vogl_gl_object_state *a, const vogl_gl_object_state *b) const
    {
        return a->get_snapshot_handle() < b->get_snapshot_handle();
    }
};

#define STR_INT(val) tmp.sprintf("%d", val)

vogleditor_stateTreeContextItem::vogleditor_stateTreeContextItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_context_snapshot &contextState)
    : vogleditor_stateTreeItem(name, value, parent),
      m_pState(&contextState),
      m_pDiffBaseState(NULL),
      m_pAttributesItem(NULL),
      m_pCreationItem(NULL),
      m_pDirectItem(NULL),
      m_pSharedItem(NULL),
      m_pPolygonStippleItem(NULL),
      m_pTexEnvItem(NULL)
{
    QString tmp;

    const vogl_context_desc &desc = contextState.get_context_desc();

    // context desc
    m_pAttributesItem = new vogleditor_stateTreeContextAttributesItem("Attributes", this, desc);
    this->appendChild(m_pAttributesItem);

    m_pCreationItem = new vogleditor_stateTreeContextCreationItem("Created by", this, desc);
    this->appendChild(m_pCreationItem);

    m_pDirectItem = new vogleditor_stateTreeContextDirectItem("Direct", this, desc);
    this->appendChild(m_pDirectItem);

    m_pSharedItem = new vogleditor_stateTreeContextSharedItem("Shared context", this, desc);
    this->appendChild(m_pSharedItem);

    const vogl_context_info &info = contextState.get_context_info();
    vogleditor_stateTreeContextInfoItem *pInfoNode = new vogleditor_stateTreeContextInfoItem("Info", "", this, info);
    m_contextInfoItems.push_back(pInfoNode);
    this->appendChild(pInfoNode);

    // in order for the rest of this information to be available, info must be valid
    if (info.is_valid())
    {
        vogl_general_context_state &generalState = contextState.get_general_state();
        vogleditor_stateTreeContextGeneralItem *pGeneralNode = new vogleditor_stateTreeContextGeneralItem("General", "", this, generalState, info);
        m_generalStateItems.push_back(pGeneralNode);
        this->appendChild(pGeneralNode);

        vogl_display_list_state displayListState = contextState.get_display_list_state();
        vogl_display_list_map &displayLists = displayListState.get_display_list_map();
        if (displayLists.size() > 0)
        {
            vogleditor_stateTreeDisplaylistItem *pDisplayListStateNode = new vogleditor_stateTreeDisplaylistItem("DisplayLists", "", this, &displayListState);
            this->appendChild(pDisplayListStateNode);
        }

        vogl_light_state &lightState = contextState.get_light_state();
        if (lightState.is_valid())
        {
            vogleditor_stateTreeItem *pLightStateNode = new vogleditor_stateTreeItem("Lights", tmp.sprintf("[%u]", lightState.get_num_lights()), this);
            this->appendChild(pLightStateNode);

            for (uint i = 0; i < lightState.get_num_lights(); i++)
            {
                const vogl_state_vector &lightVec = lightState.get_light(i);
                vogleditor_stateTreeLightItem *pLightNode = new vogleditor_stateTreeLightItem(tmp.sprintf("GL_LIGHT%u", i), i, pLightStateNode, &lightVec);
                m_lightItems.push_back(pLightNode);
                pLightStateNode->appendChild(pLightNode);
            }
        }

        vogl_material_state &materialState = contextState.get_material_state();
        if (materialState.is_valid())
        {
            vogleditor_stateTreeItem *pMaterialNode = new vogleditor_stateTreeItem("Material", "", this);
            this->appendChild(pMaterialNode);

            vogleditor_stateTreeContextMaterialItem *pFront = new vogleditor_stateTreeContextMaterialItem("GL_FRONT", materialState.cFront, pMaterialNode, materialState);
            m_materialItems.push_back(pFront);
            pMaterialNode->appendChild(pFront);

            vogleditor_stateTreeContextMaterialItem *pBack = new vogleditor_stateTreeContextMaterialItem("GL_BACK", materialState.cBack, pMaterialNode, materialState);
            m_materialItems.push_back(pBack);
            pMaterialNode->appendChild(pBack);
        }

        vogl_matrix_state &matrixState = contextState.get_matrix_state();
        if (matrixState.is_valid())
        {
            vogleditor_stateTreeItem *pMatrixStateNode = new vogleditor_stateTreeItem("Matrix", "", this);
            this->appendChild(pMatrixStateNode);

            vogleditor_stateTreeItem *pProjectionMatrixNode = new vogleditor_stateTreeItem("GL_PROJECTION stack", tmp.sprintf("[%u]", matrixState.get_matrix_stack_depth(GL_PROJECTION, 0)), pMatrixStateNode);
            pMatrixStateNode->appendChild(pProjectionMatrixNode);
            vogleditor_stateTreeItem *pModelviewMatrixNode = new vogleditor_stateTreeItem("GL_MODELVIEW stack", tmp.sprintf("[%u]", matrixState.get_matrix_stack_depth(GL_MODELVIEW, 0)), pMatrixStateNode);
            pMatrixStateNode->appendChild(pModelviewMatrixNode);
            vogleditor_stateTreeItem *pColorMatrixNode = new vogleditor_stateTreeItem("GL_COLOR stack", tmp.sprintf("[%u]", matrixState.get_matrix_stack_depth(GL_COLOR, 0)), pMatrixStateNode);
            pMatrixStateNode->appendChild(pColorMatrixNode);

            {
                vogleditor_stateTreeMatrixStackItem *pItem = new vogleditor_stateTreeMatrixStackItem("GL_PROJECTION", GL_PROJECTION, 0, pProjectionMatrixNode, matrixState);
                pProjectionMatrixNode->appendChild(pItem);
                m_matrixStackItems.push_back(pItem);
            }
            {
                vogleditor_stateTreeMatrixStackItem *pItem = new vogleditor_stateTreeMatrixStackItem("GL_MODELVIEW", GL_MODELVIEW, 0, pModelviewMatrixNode, matrixState);
                pModelviewMatrixNode->appendChild(pItem);
                m_matrixStackItems.push_back(pItem);
            }
            {
                vogleditor_stateTreeMatrixStackItem *pItem = new vogleditor_stateTreeMatrixStackItem("GL_COLOR", GL_COLOR, 0, pColorMatrixNode, matrixState);
                pColorMatrixNode->appendChild(pItem);
                m_matrixStackItems.push_back(pItem);
            }

            QString tmpName;
            vogleditor_stateTreeItem *pTextureMatrixNode = new vogleditor_stateTreeItem("GL_TEXTURE", "", pMatrixStateNode);
            pMatrixStateNode->appendChild(pTextureMatrixNode);
            for (uint texcoord_index = 0; texcoord_index < info.get_max_texture_coords(); texcoord_index++)
            {
                vogleditor_stateTreeItem *pTextureStackNode = new vogleditor_stateTreeItem(tmpName.sprintf("GL_TEXTURE%u stack", texcoord_index), tmpName.sprintf("[%u]", matrixState.get_matrix_stack_depth(GL_TEXTURE0 + texcoord_index, texcoord_index)), pTextureMatrixNode);
                pTextureMatrixNode->appendChild(pTextureStackNode);

                {
                    vogleditor_stateTreeMatrixStackItem *pItem = new vogleditor_stateTreeMatrixStackItem(tmpName.sprintf("GL_TEXTURE%u", texcoord_index), GL_TEXTURE0 + texcoord_index, texcoord_index, pTextureStackNode, matrixState);
                    pTextureStackNode->appendChild(pItem);
                    m_matrixStackItems.push_back(pItem);
                }
            }

            vogleditor_stateTreeItem *pMatrixMatrixNode = new vogleditor_stateTreeItem("GL_MATRIX", "", pMatrixStateNode);
            pMatrixStateNode->appendChild(pMatrixMatrixNode);
            for (uint i = 0; i < info.get_max_arb_program_matrices(); i++)
            {
                vogleditor_stateTreeItem *pMatrixStackNode = new vogleditor_stateTreeItem(tmpName.sprintf("GL_MATRIX%u_ARB stack", i), tmpName.sprintf("[%u]", matrixState.get_matrix_stack_depth(GL_MATRIX0_ARB + i, i)), pMatrixMatrixNode);
                pMatrixMatrixNode->appendChild(pMatrixStackNode);

                {
                    vogleditor_stateTreeMatrixStackItem *pItem = new vogleditor_stateTreeMatrixStackItem(tmpName.sprintf("GL_MATRIX%u_ARB", i), GL_MATRIX0_ARB + i, i, pMatrixStackNode, matrixState);
                    pMatrixStackNode->appendChild(pItem);
                    m_matrixStackItems.push_back(pItem);
                }
            }
        }

        vogl_arb_program_environment_state &programState = contextState.get_arb_program_environment_state();
        if (programState.is_valid())
        {
            vogleditor_stateTreeItem *pProgramStateNode = new vogleditor_stateTreeItem("ARB Program Env", "", this);
            this->appendChild(pProgramStateNode);

            for (uint i = 0; i < programState.cNumTargets; i++)
            {
                vogleditor_stateTreeArbProgramEnvItem *pProgramNode = new vogleditor_stateTreeArbProgramEnvItem(enum_to_string(programState.get_target_enum(i)), i, pProgramStateNode, programState);
                m_arbProgramEnvItems.push_back(pProgramNode);
                pProgramStateNode->appendChild(pProgramNode);
            }
        }

        vogl_polygon_stipple_state &stippleState = contextState.get_polygon_stipple_state();
        if (stippleState.is_valid())
        {
            m_pPolygonStippleItem = new vogleditor_stateTreePolygonStippleItem("Polygon Stipple", this, stippleState);
            this->appendChild(m_pPolygonStippleItem);
        }

        vogl_texenv_state &texEnvState = contextState.get_texenv_state();
        if (texEnvState.is_valid())
        {
            m_pTexEnvItem = new vogleditor_stateTreeTexEnvItem("Texture Env", this, texEnvState, info);
            this->appendChild(m_pTexEnvItem);
        }

        // sort all state objects by snapshot handle
        vogl_gl_object_state_ptr_vec objectState = contextState.get_objects();
        objectState.sort(vogl_gl_object_state_handle_less_than());
        vogleditor_stateTreeItem *pObjectsNode = new vogleditor_stateTreeItem("Objects", "", this);
        this->appendChild(pObjectsNode);

        {
// Append a node for each of the state object types
#define DEF(x) vogleditor_stateTreeItem *pNode##x = new vogleditor_stateTreeItem(#x "s", "", pObjectsNode);
            VOGL_GL_OBJECT_STATE_TYPES
#undef DEF

            for (vogl_gl_object_state_ptr_vec::iterator iter = objectState.begin(); iter != objectState.end(); iter++)
            {
                switch ((*iter)->get_type())
                {
                    case cGLSTTexture:
                    {
                        vogl_texture_state *pTexState = static_cast<vogl_texture_state *>(*iter);
                        QString valueStr;
                        valueStr = valueStr.sprintf("%s (%u x %u x %u) %s", enum_to_string(pTexState->get_target()).toStdString().c_str(), pTexState->get_texture().get_width(), pTexState->get_texture().get_height(), pTexState->get_texture().get_depth(), enum_to_string(pTexState->get_texture().get_ogl_internal_fmt()).toStdString().c_str());
                        vogleditor_stateTreeTextureItem *pNode = new vogleditor_stateTreeTextureItem(int64_to_string(pTexState->get_snapshot_handle()), valueStr, pNodeTexture, pTexState, info);
                        m_textureItems.push_back(pNode);
                        pNodeTexture->appendChild(pNode);
                        break;
                    }
                    case cGLSTBuffer:
                    {
                        vogl_buffer_state *pBuffer = static_cast<vogl_buffer_state *>(*iter);
                        vogleditor_stateTreeBufferItem *pNode = new vogleditor_stateTreeBufferItem(int64_to_string(pBuffer->get_snapshot_handle()), enum_to_string(pBuffer->get_target()), pNodeBuffer, pBuffer);
                        m_bufferItems.push_back(pNode);
                        pNodeBuffer->appendChild(pNode);

                        break;
                    }
                    case cGLSTSampler:
                    {
                        const vogl_sampler_state *pSampler = static_cast<const vogl_sampler_state *>(*iter);
                        vogleditor_stateTreeSamplerItem *pNode = new vogleditor_stateTreeSamplerItem(int64_to_string(pSampler->get_snapshot_handle()), "", pNodeSampler, pSampler, info);
                        m_samplerItems.push_back(pNode);
                        pNodeSampler->appendChild(pNode);

                        break;
                    }
                    case cGLSTQuery:
                    {
                        vogl_query_state *pQuery = static_cast<vogl_query_state *>(*iter);
                        vogleditor_stateTreeQueryItem *pNode = new vogleditor_stateTreeQueryItem(int64_to_string(pQuery->get_snapshot_handle()), pQuery->get_snapshot_handle(), pNodeQuery, pQuery);
                        m_queryItems.push_back(pNode);
                        pNodeQuery->appendChild(pNode);
                        break;
                    }
                    case cGLSTRenderbuffer:
                    {
                        vogl_renderbuffer_state *pRenderbuffer = static_cast<vogl_renderbuffer_state *>(*iter);
                        vogleditor_stateTreeRenderbufferItem *pNode = new vogleditor_stateTreeRenderbufferItem(int64_to_string(pRenderbuffer->get_snapshot_handle()), pRenderbuffer->get_snapshot_handle(), pNodeRenderbuffer, *pRenderbuffer);
                        QString valueStr;
                        int width = 0;
                        int height = 0;
                        int format = 0;
                        pRenderbuffer->get_desc().get_int(GL_RENDERBUFFER_WIDTH, &width);
                        pRenderbuffer->get_desc().get_int(GL_RENDERBUFFER_HEIGHT, &height);
                        pRenderbuffer->get_desc().get_int(GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
                        valueStr = valueStr.sprintf("(%d x %d) %s", width, height, enum_to_string(format).toStdString().c_str());
                        pNode->setValue(valueStr);
                        m_renderbufferItems.push_back(pNode);
                        pNodeRenderbuffer->appendChild(pNode);

                        break;
                    }
                    case cGLSTFramebuffer:
                    {
                        vogl_framebuffer_state *pFramebuffer = static_cast<vogl_framebuffer_state *>(*iter);
                        if (!pFramebuffer->is_valid())
                        {
                            break;
                        }

                        vogleditor_stateTreeFramebufferItem *pNode = new vogleditor_stateTreeFramebufferItem(int64_to_string(pFramebuffer->get_snapshot_handle()), "", pFramebuffer->get_snapshot_handle(), pNodeFramebuffer, pFramebuffer);
                        m_framebufferItems.push_back(pNode);
                        pNodeFramebuffer->appendChild(pNode);

                        break;
                    }
                    case cGLSTVertexArray:
                    {
                        vogl_vao_state *pVAO = static_cast<vogl_vao_state *>(*iter);
                        if (!pVAO->is_valid())
                        {
                            break;
                        }

                        vogleditor_stateTreeVertexArrayItem *pNode = new vogleditor_stateTreeVertexArrayItem(int64_to_string(pVAO->get_snapshot_handle()), tmp.sprintf("[%u]", info.get_max_vertex_attribs()), pVAO->get_snapshot_handle(), pNodeVertexArray, *pVAO, info);
                        m_vertexArrayItems.push_back(pNode);
                        pNodeVertexArray->appendChild(pNode);

                        break;
                    }
                    case cGLSTShader:
                    {
                        vogl_shader_state *pShader = static_cast<vogl_shader_state *>(*iter);
                        if (!pShader->is_valid())
                        {
                            break;
                        }

                        vogleditor_stateTreeShaderItem *pNode = new vogleditor_stateTreeShaderItem(int64_to_string(pShader->get_snapshot_handle()), enum_to_string(pShader->get_shader_type()), pNodeShader, *pShader);
                        m_shaderItems.push_back(pNode);
                        pNodeShader->appendChild(pNode);

                        break;
                    }
                    case cGLSTProgram:
                    {
                        vogl_program_state *pProgram = static_cast<vogl_program_state *>(*iter);
                        if (!pProgram->is_valid())
                        {
                            break;
                        }

                        vogleditor_stateTreeProgramItem *pNode = new vogleditor_stateTreeProgramItem(int64_to_string(pProgram->get_snapshot_handle()), "", pNodeProgram, *pProgram, info);
                        m_programItems.push_back(pNode);
                        pNodeProgram->appendChild(pNode);

                        break;
                    }
                    case cGLSTSync:
                    {
                        vogl_sync_state *pSync = static_cast<vogl_sync_state *>(*iter);
                        vogleditor_stateTreeSyncItem *pNode = new vogleditor_stateTreeSyncItem(int64_to_string(pSync->get_snapshot_handle()), pNodeSync, *pSync);
                        m_syncItems.push_back(pNode);
                        pNodeSync->appendChild(pNode);
                        break;
                    }
                    case cGLSTARBProgram:
                    {
                        vogl_arb_program_state *pARBProgram = static_cast<vogl_arb_program_state *>(*iter);
                        if (!pARBProgram->is_valid())
                        {
                            break;
                        }
                        vogleditor_stateTreeArbProgramItem *pNode = new vogleditor_stateTreeArbProgramItem(int64_to_string(pARBProgram->get_snapshot_handle()), "", pNodeARBProgram, *pARBProgram);
                        m_arbProgramItems.push_back(pNode);
                        pNodeARBProgram->appendChild(pNode);
                        break;
                    }
                    case cGLSTProgramPipeline:
                    {
                        vogl_sso_state *pSSO = static_cast<vogl_sso_state *>(*iter);
                        if (!pSSO->is_valid())
                        {
                            break;
                        }
                        vogleditor_stateTreeProgramPipelineItem *pNode = new vogleditor_stateTreeProgramPipelineItem(int64_to_string(pSSO->get_snapshot_handle()), pSSO->get_snapshot_handle(), pNodeProgramPipeline, *pSSO);
                        m_programPipelineItems.push_back(pNode);
                        pNodeProgramPipeline->appendChild(pNode);

                        break;
                    }
                    case cGLSTInvalid:
                    default:
                    {
                        // in the Invalid and default case add the object to the invalids
                        vogleditor_stateTreeItem *pNode = new vogleditor_stateTreeItem(int64_to_string((*iter)->get_snapshot_handle()), get_gl_object_state_type_str((*iter)->get_type()), pNodeARBProgram);
                        pNodeARBProgram->appendChild(pNode);
                    }
                }
            } // end for

// update the value of the list node to reflect the number of elements in that list
#define DEF(x) pNode##x->setValue(tmp.sprintf("[%d]", pNode##x->childCount()));
            VOGL_GL_OBJECT_STATE_TYPES
#undef DEF

// only append list if there are objects of that type
#define DEF(x)                      \
    if (pNode##x->childCount() > 0) \
        pObjectsNode->appendChild(pNode##x);
            VOGL_GL_OBJECT_STATE_TYPES
#undef DEF
        }
    }
}

vogleditor_stateTreeContextItem::~vogleditor_stateTreeContextItem()
{
    m_pState = NULL;
    m_pDiffBaseState = NULL;
    m_pAttributesItem = NULL;
    m_pCreationItem = NULL;
    m_pDirectItem = NULL;
    m_pSharedItem = NULL;
    m_pPolygonStippleItem = NULL;
    m_pTexEnvItem = NULL;
}

void vogleditor_stateTreeContextItem::set_diff_base_state(const vogl_context_snapshot *pBaseState)
{
    m_pDiffBaseState = pBaseState;

    if (m_pAttributesItem != NULL)
    {
        m_pAttributesItem->set_diff_base_state(&(m_pDiffBaseState->get_context_desc()));
    }

    if (m_pCreationItem != NULL)
    {
        m_pCreationItem->set_diff_base_state(&(m_pDiffBaseState->get_context_desc()));
    }

    if (m_pDirectItem != NULL)
    {
        m_pDirectItem->set_diff_base_state(&(m_pDiffBaseState->get_context_desc()));
    }

    if (m_pSharedItem != NULL)
    {
        m_pSharedItem->set_diff_base_state(&(m_pDiffBaseState->get_context_desc()));
    }

    if (m_pPolygonStippleItem != NULL)
    {
        m_pPolygonStippleItem->set_diff_base_state(&(m_pDiffBaseState->get_polygon_stipple_state()));
    }

    if (m_pTexEnvItem != NULL)
    {
        m_pTexEnvItem->set_diff_base_state(&(m_pDiffBaseState->get_context_info()), &(m_pDiffBaseState->get_texenv_state()));
    }

    for (vogleditor_stateTreeContextGeneralItem **iter = m_generalStateItems.begin(); iter != m_generalStateItems.end(); iter++)
    {
        if (m_pDiffBaseState != NULL)
        {
            (*iter)->set_diff_base_state(&(pBaseState->get_general_state()));
        }
        else
        {
            (*iter)->set_diff_base_state(NULL);
        }
    }

    for (vogleditor_stateTreeContextInfoItem **iter = m_contextInfoItems.begin(); iter != m_contextInfoItems.end(); iter++)
    {
        if (m_pDiffBaseState != NULL)
        {
            (*iter)->set_diff_base_state(&(pBaseState->get_context_info()));
        }
        else
        {
            (*iter)->set_diff_base_state(NULL);
        }
    }

    vogl_gl_object_state_ptr_vec texObjs;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTTexture, texObjs);
    for (vogleditor_stateTreeTextureItem **iter = m_textureItems.begin(); iter != m_textureItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = texObjs.begin(); diffIter != texObjs.end(); diffIter++)
        {
            const vogl_texture_state *pDiffTex = static_cast<const vogl_texture_state *>(*diffIter);
            if (pDiffTex->get_snapshot_handle() == (*iter)->get_texture_state()->get_snapshot_handle())
            {
                (*iter)->set_diff_base_state(pDiffTex);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec queryObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTQuery, queryObjects);
    for (vogleditor_stateTreeQueryItem **iter = m_queryItems.begin(); iter != m_queryItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = queryObjects.begin(); diffIter != queryObjects.end(); diffIter++)
        {
            const vogl_query_state *pDiffState = static_cast<const vogl_query_state *>(*diffIter);
            if (pDiffState->get_snapshot_handle() == (*iter)->get_handle())
            {
                (*iter)->set_diff_base_state(pDiffState);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec renderbufferObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTRenderbuffer, renderbufferObjects);
    for (vogleditor_stateTreeRenderbufferItem **iter = m_renderbufferItems.begin(); iter != m_renderbufferItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = renderbufferObjects.begin(); diffIter != renderbufferObjects.end(); diffIter++)
        {
            const vogl_renderbuffer_state *pDiffState = static_cast<const vogl_renderbuffer_state *>(*diffIter);
            if (pDiffState->get_snapshot_handle() == (*iter)->get_handle())
            {
                (*iter)->set_diff_base_state(pDiffState);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec framebufferObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTFramebuffer, framebufferObjects);
    for (vogleditor_stateTreeFramebufferItem **iter = m_framebufferItems.begin(); iter != m_framebufferItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = framebufferObjects.begin(); diffIter != framebufferObjects.end(); diffIter++)
        {
            const vogl_framebuffer_state *pDiffState = static_cast<const vogl_framebuffer_state *>(*diffIter);
            if (pDiffState->get_snapshot_handle() == (*iter)->get_handle())
            {
                (*iter)->set_diff_base_state(pDiffState);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec samplerObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTSampler, samplerObjects);
    for (vogleditor_stateTreeSamplerItem **iter = m_samplerItems.begin(); iter != m_samplerItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = samplerObjects.begin(); diffIter != samplerObjects.end(); diffIter++)
        {
            const vogl_sampler_state *pDiffObj = static_cast<const vogl_sampler_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_sampler_state()->get_snapshot_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec bufferObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTBuffer, bufferObjects);
    for (vogleditor_stateTreeBufferItem **iter = m_bufferItems.begin(); iter != m_bufferItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = bufferObjects.begin(); diffIter != bufferObjects.end(); diffIter++)
        {
            const vogl_buffer_state *pDiffObj = static_cast<const vogl_buffer_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_buffer_state()->get_snapshot_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec vertexArrayObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTVertexArray, vertexArrayObjects);
    for (vogleditor_stateTreeVertexArrayItem **iter = m_vertexArrayItems.begin(); iter != m_vertexArrayItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = vertexArrayObjects.begin(); diffIter != vertexArrayObjects.end(); diffIter++)
        {
            const vogl_vao_state *pDiffObj = static_cast<const vogl_vao_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec shaderObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTShader, shaderObjects);
    for (vogleditor_stateTreeShaderItem **iter = m_shaderItems.begin(); iter != m_shaderItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = shaderObjects.begin(); diffIter != shaderObjects.end(); diffIter++)
        {
            const vogl_shader_state *pDiffObj = static_cast<const vogl_shader_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_current_state()->get_snapshot_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec programObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTProgram, programObjects);
    for (vogleditor_stateTreeProgramItem **iter = m_programItems.begin(); iter != m_programItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = programObjects.begin(); diffIter != programObjects.end(); diffIter++)
        {
            const vogl_program_state *pDiffObj = static_cast<const vogl_program_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_current_state()->get_snapshot_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec syncObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTSync, syncObjects);
    for (vogleditor_stateTreeSyncItem **iter = m_syncItems.begin(); iter != m_syncItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = syncObjects.begin(); diffIter != syncObjects.end(); diffIter++)
        {
            vogl_sync_state *pDiffObj = static_cast<vogl_sync_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_current_state()->get_snapshot_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec arbProgramObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTARBProgram, arbProgramObjects);
    for (vogleditor_stateTreeArbProgramItem **iter = m_arbProgramItems.begin(); iter != m_arbProgramItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = arbProgramObjects.begin(); diffIter != arbProgramObjects.end(); diffIter++)
        {
            const vogl_arb_program_state *pDiffObj = static_cast<const vogl_arb_program_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_current_state()->get_snapshot_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    vogl_gl_object_state_ptr_vec programPipelineObjects;
    m_pDiffBaseState->get_all_objects_of_category(cGLSTProgramPipeline, programPipelineObjects);
    for (vogleditor_stateTreeProgramPipelineItem **iter = m_programPipelineItems.begin(); iter != m_programPipelineItems.end(); iter++)
    {
        for (vogl_gl_object_state **diffIter = programPipelineObjects.begin(); diffIter != programPipelineObjects.end(); diffIter++)
        {
            const vogl_sso_state *pDiffObj = static_cast<const vogl_sso_state *>(*diffIter);
            if (pDiffObj->get_snapshot_handle() == (*iter)->get_handle())
            {
                (*iter)->set_diff_base_state(pDiffObj);
                break;
            }
        }
    }

    const vogl_light_state &lightState = pBaseState->get_light_state();
    if (lightState.is_valid())
    {
        for (vogleditor_stateTreeLightItem **iter = m_lightItems.begin(); iter != m_lightItems.end(); iter++)
        {
            const vogl_state_vector &lightVec = lightState.get_light((*iter)->get_light_index());
            (*iter)->set_diff_base_state(&lightVec);
        }
    }

    const vogl_arb_program_environment_state &progEnvState = pBaseState->get_arb_program_environment_state();
    if (progEnvState.is_valid())
    {
        for (vogleditor_stateTreeArbProgramEnvItem **iter = m_arbProgramEnvItems.begin(); iter != m_arbProgramEnvItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(&progEnvState);
        }
    }

    const vogl_material_state &materialState = pBaseState->get_material_state();
    if (materialState.is_valid())
    {
        for (vogleditor_stateTreeContextMaterialItem **iter = m_materialItems.begin(); iter != m_materialItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(&materialState);
        }
    }

    const vogl_matrix_state &matrixState = pBaseState->get_matrix_state();
    if (matrixState.is_valid())
    {
        for (vogleditor_stateTreeMatrixStackItem **iter = m_matrixStackItems.begin(); iter != m_matrixStackItems.end(); iter++)
        {
            if (m_pDiffBaseState != NULL)
            {
                (*iter)->set_diff_base_state(&matrixState);
            }
        }
    }
}
