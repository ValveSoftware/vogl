#ifndef VOGLEDITOR_STATETREECONTEXTITEM_H
#define VOGLEDITOR_STATETREECONTEXTITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_gl_state_snapshot.h"

class vogleditor_stateTreeArbProgramItem;
class vogleditor_stateTreeArbProgramEnvItem;
class vogleditor_stateTreeBufferItem;
class vogleditor_stateTreeContextInfoItem;
class vogleditor_stateTreeContextGeneralItem;
class vogleditor_stateTreeFramebufferItem;
class vogleditor_stateTreeLightItem;
class vogleditor_stateTreePolygonStippleItem;
class vogleditor_stateTreeProgramItem;
class vogleditor_stateTreeQueryItem;
class vogleditor_stateTreeRenderbufferItem;
class vogleditor_stateTreeSamplerItem;
class vogleditor_stateTreeShaderItem;
class vogleditor_stateTreeSyncItem;
class vogleditor_stateTreeTextureItem;
class vogleditor_stateTreeMatrixStackItem;
class vogleditor_stateTreeVertexArrayItem;
class vogleditor_stateTreeTexEnvItem;
class vogleditor_stateTreeProgramPipelineItem;

class vogleditor_stateTreeContextAttributesItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextAttributesItem(QString name, vogleditor_stateTreeItem *parent, const vogl_context_desc &desc)
        : vogleditor_stateTreeItem(name, "", parent),
          m_pState(&desc),
          m_pDiffBaseState(NULL)
    {
        setValue(attribsToString(desc.get_attribs()));
    }

    virtual ~vogleditor_stateTreeContextAttributesItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_context_desc *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return false;
        }

        const vogl_context_attribs &attribs = m_pState->get_attribs();

        for (uint i = 0; i < attribs.size(); i++)
        {
            if (i < m_pDiffBaseState->get_attribs().size())
            {
                if (attribs[i] != m_pDiffBaseState->get_attribs()[i])
                {
                    return true;
                }
            }
        }

        return false;
    }

    virtual QString getDiffedValue() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return "";
        }

        return attribsToString(m_pDiffBaseState->get_attribs());
    }

private:
    const vogl_context_desc *m_pState;
    const vogl_context_desc *m_pDiffBaseState;

    QString attribsToString(const vogl_context_attribs &attribs) const
    {
        QString attribStr;
        static QString tmp;
        for (uint i = 0; i < attribs.size(); i++)
        {
            if (i != 0)
            {
                attribStr += ", ";
            }

            GLenum id = attribs[i];

            // final value should always be 0, and should not be handled like the other attribs
            if (id == 0)
            {
                attribStr += "0";
            }
            else
            {
                tmp = get_gl_enums().find_glx_name(id);
                if (tmp.isEmpty() || tmp.isNull())
                {
                    tmp = tmp.sprintf("0x%x", id);
                }
                attribStr += tmp;
            }

            // check for a value to go with the above attrib
            if (i + 1 < attribs.size())
            {
                i += 1;
                attribStr += tmp.sprintf(" = 0x%x", attribs[i]);
            }
        }

        if (attribStr.isEmpty())
        {
            attribStr = "0";
        }
        return attribStr;
    }
};

class vogleditor_stateTreeContextCreationItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextCreationItem(QString name, vogleditor_stateTreeItem *parent, const vogl_context_desc &desc)
        : vogleditor_stateTreeItem(name, "", parent),
          m_pState(&desc),
          m_pDiffBaseState(NULL)
    {
        const gl_entrypoint_desc_t &creation_func_entrypoint_desc = g_vogl_entrypoint_descs[desc.get_creation_func()];

        setValue(QString(creation_func_entrypoint_desc.m_pName));
    }

    virtual ~vogleditor_stateTreeContextCreationItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_context_desc *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return false;
        }

        return m_pState->get_creation_func() != m_pDiffBaseState->get_creation_func();
    }

    virtual QString getDiffedValue() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return "";
        }

        const gl_entrypoint_desc_t &creation_func_entrypoint_desc = g_vogl_entrypoint_descs[m_pDiffBaseState->get_creation_func()];
        return QString(creation_func_entrypoint_desc.m_pName);
    }

private:
    const vogl_context_desc *m_pState;
    const vogl_context_desc *m_pDiffBaseState;
};

class vogleditor_stateTreeContextDirectItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextDirectItem(QString name, vogleditor_stateTreeItem *parent, const vogl_context_desc &desc)
        : vogleditor_stateTreeItem(name, "", parent),
          m_pState(&desc),
          m_pDiffBaseState(NULL)
    {
        setValue((bool)desc.get_direct());
    }

    virtual ~vogleditor_stateTreeContextDirectItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_context_desc *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return false;
        }

        return m_pState->get_direct() != m_pDiffBaseState->get_direct();
    }

    virtual QString getDiffedValue() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return "";
        }

        bool value = m_pDiffBaseState->get_direct();
        return getValueFromBools(&value, 1);
    }

private:
    const vogl_context_desc *m_pState;
    const vogl_context_desc *m_pDiffBaseState;
};

class vogleditor_stateTreeContextSharedItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextSharedItem(QString name, vogleditor_stateTreeItem *parent, const vogl_context_desc &desc)
        : vogleditor_stateTreeItem(name, "", parent),
          m_pState(&desc),
          m_pDiffBaseState(NULL)
    {
        setValue((void *)desc.get_trace_share_context());
    }

    virtual ~vogleditor_stateTreeContextSharedItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_context_desc *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return false;
        }

        return m_pState->get_trace_share_context() != m_pDiffBaseState->get_trace_share_context();
    }

    virtual QString getDiffedValue() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return "";
        }

        vogl_trace_ptr_value value = m_pDiffBaseState->get_trace_share_context();
        return getValueFromPtrs((int *)&value, 1);
    }

private:
    const vogl_context_desc *m_pState;
    const vogl_context_desc *m_pDiffBaseState;
};

class vogleditor_stateTreeContextMaterialItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextMaterialItem(QString name, unsigned int face, vogleditor_stateTreeItem *parent, vogl_material_state &state)
        : vogleditor_stateTreeItem(name, "", parent),
          m_face(face),
          m_pState(&state),
          m_pDiffBaseState(NULL)
    {
        vogl_state_vector *pStateVec = state.get_state_vector(face);

        int iVals[4] = { 0, 0, 0, 0 };
        float fVals[4] = { 0, 0, 0, 0 };
#define GET_FLOAT(side, name, num)                                                                                                                       \
    if (pStateVec->get<float>(name, 0, fVals, num))                                                                                                      \
    {                                                                                                                                                    \
        vogleditor_stateTreeStateVecFloatItem *pItem = new vogleditor_stateTreeStateVecFloatItem(#name, name, 0, *(pStateVec), fVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                                \
        this->appendChild(pItem);                                                                                                                        \
    }
#define GET_INT(side, name, num)                                                                                                                     \
    if (pStateVec->get<int>(name, 0, iVals, num))                                                                                                    \
    {                                                                                                                                                \
        vogleditor_stateTreeStateVecIntItem *pItem = new vogleditor_stateTreeStateVecIntItem(#name, name, 0, *(pStateVec), iVals, num, false, this); \
        m_diffableItems.push_back(pItem);                                                                                                            \
        this->appendChild(pItem);                                                                                                                    \
    }
        VOGL_ASSERT(pStateVec->size() == 6);
        GET_FLOAT(face, GL_AMBIENT, 4);
        GET_FLOAT(face, GL_DIFFUSE, 4);
        GET_FLOAT(face, GL_SPECULAR, 4);
        GET_FLOAT(face, GL_EMISSION, 4);
        GET_FLOAT(face, GL_SHININESS, 1);
        GET_INT(face, GL_COLOR_INDEXES, 3);
#undef GET_FLOAT
#undef GET_INT
    }

    virtual ~vogleditor_stateTreeContextMaterialItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_material_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;

        for (vogleditor_stateTreeStateVecDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(m_pDiffBaseState->get_state_vector(m_face));
        }
    }

private:
    const unsigned int m_face;
    const vogl_material_state *m_pState;
    const vogl_material_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
};

class vogleditor_stateTreeContextItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextItem(QString name, QString value, vogleditor_stateTreeItem *parent, vogl_context_snapshot &contextState);

    virtual ~vogleditor_stateTreeContextItem();

    void set_diff_base_state(const vogl_context_snapshot *pBaseState);

private:
    vogl_context_snapshot *m_pState;
    const vogl_context_snapshot *m_pDiffBaseState;
    vogleditor_stateTreeContextAttributesItem *m_pAttributesItem;
    vogleditor_stateTreeContextCreationItem *m_pCreationItem;
    vogleditor_stateTreeContextDirectItem *m_pDirectItem;
    vogleditor_stateTreeContextSharedItem *m_pSharedItem;
    vogleditor_stateTreePolygonStippleItem *m_pPolygonStippleItem;
    vogleditor_stateTreeTexEnvItem *m_pTexEnvItem;
    vogl::vector<vogleditor_stateTreeContextInfoItem *> m_contextInfoItems;
    vogl::vector<vogleditor_stateTreeContextGeneralItem *> m_generalStateItems;
    vogl::vector<vogleditor_stateTreeStateVecDiffableItem *> m_diffableItems;
    vogl::vector<vogleditor_stateTreeTextureItem *> m_textureItems;
    vogl::vector<vogleditor_stateTreeSamplerItem *> m_samplerItems;
    vogl::vector<vogleditor_stateTreeBufferItem *> m_bufferItems;
    vogl::vector<vogleditor_stateTreeLightItem *> m_lightItems;
    vogl::vector<vogleditor_stateTreeContextMaterialItem *> m_materialItems;
    vogl::vector<vogleditor_stateTreeMatrixStackItem *> m_matrixStackItems;
    vogl::vector<vogleditor_stateTreeQueryItem *> m_queryItems;
    vogl::vector<vogleditor_stateTreeRenderbufferItem *> m_renderbufferItems;
    vogl::vector<vogleditor_stateTreeVertexArrayItem *> m_vertexArrayItems;
    vogl::vector<vogleditor_stateTreeFramebufferItem *> m_framebufferItems;
    vogl::vector<vogleditor_stateTreeShaderItem *> m_shaderItems;
    vogl::vector<vogleditor_stateTreeProgramItem *> m_programItems;
    vogl::vector<vogleditor_stateTreeSyncItem *> m_syncItems;
    vogl::vector<vogleditor_stateTreeArbProgramItem *> m_arbProgramItems;
    vogl::vector<vogleditor_stateTreeArbProgramEnvItem *> m_arbProgramEnvItems;
    vogl::vector<vogleditor_stateTreeProgramPipelineItem *> m_programPipelineItems;
};

#endif // VOGLEDITOR_STATETREECONTEXTITEM_H
