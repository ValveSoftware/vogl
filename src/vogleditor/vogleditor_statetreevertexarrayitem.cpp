#include "vogleditor_statetreevertexarrayitem.h"

vogleditor_stateTreeVertexArrayBoolItem::vogleditor_stateTreeVertexArrayBoolItem(QString name, bool vogl_vertex_attrib_desc::* pVal, vogleditor_stateTreeItem* parent, const vogl_vertex_attrib_desc& state, unsigned int arrayIndex)
    : vogleditor_stateTreeVertexArrayDiffableItem(name, "", parent, arrayIndex),
      m_pState(&state),
      m_pVal(pVal),
      m_pIntVal(NULL)
{
    bool val = state.*pVal;
    setValue(val? "GL_TRUE" : "GL_FALSE");
}

vogleditor_stateTreeVertexArrayBoolItem::vogleditor_stateTreeVertexArrayBoolItem(QString name, int vogl_vertex_attrib_desc::* pVal, vogleditor_stateTreeItem* parent, const vogl_vertex_attrib_desc& state, unsigned int arrayIndex)
    : vogleditor_stateTreeVertexArrayDiffableItem(name, "", parent, arrayIndex),
      m_pState(&state),
      m_pVal(NULL),
      m_pIntVal(pVal)
{
    int val = state.*pVal;
    setValue((val != 0)? "GL_TRUE" : "GL_FALSE");
}

bool vogleditor_stateTreeVertexArrayBoolItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return true;
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);
        if (m_pVal != NULL)
            return (m_pState->*m_pVal) != (baseDesc.*m_pVal);

        if (m_pIntVal != NULL)
            return (m_pState->*m_pIntVal) != (baseDesc.*m_pIntVal);
    }

    return false;
}

QString vogleditor_stateTreeVertexArrayBoolItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return "non-existent";
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);
        if (m_pVal != NULL)
        {
            bool value = baseDesc.*m_pVal;
            return getValueFromBools(&value, 1);
        }

        if (m_pIntVal != NULL)
        {
            bool value = (baseDesc.*m_pVal != 0);
            return getValueFromBools(&value, 1);
        }
    }

    return "";
}


//=============================================================================

vogleditor_stateTreeVertexArrayEnumItem::vogleditor_stateTreeVertexArrayEnumItem(QString name, GLenum vogl_vertex_attrib_desc::* pVal, vogleditor_stateTreeItem* parent, const vogl_vertex_attrib_desc& state, unsigned int arrayIndex)
    : vogleditor_stateTreeVertexArrayDiffableItem(name, "", parent, arrayIndex),
      m_pState(&state),
      m_pVal(pVal)
{
    GLenum val = state.*pVal;
    setValue(enum_to_string(val));
}

bool vogleditor_stateTreeVertexArrayEnumItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return true;
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);
        return (m_pState->*m_pVal) != (baseDesc.*m_pVal);
    }
}

QString vogleditor_stateTreeVertexArrayEnumItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return "non-existent";
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);

        GLenum value = baseDesc.*m_pVal;
        return enum_to_string(value);
    }

    return "";
}

//=============================================================================

vogleditor_stateTreeVertexArrayPtrItem::vogleditor_stateTreeVertexArrayPtrItem(QString name, vogl_trace_ptr_value vogl_vertex_attrib_desc::* pVal, vogleditor_stateTreeItem* parent, const vogl_vertex_attrib_desc& state, unsigned int arrayIndex)
    : vogleditor_stateTreeVertexArrayDiffableItem(name, "", parent, arrayIndex),
      m_pState(&state),
      m_pVal(pVal)
{
    vogl_trace_ptr_value val = state.*pVal;
    setValue((void*)val);
}

bool vogleditor_stateTreeVertexArrayPtrItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return true;
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);
        return (m_pState->*m_pVal) != (baseDesc.*m_pVal);
    }
}

QString vogleditor_stateTreeVertexArrayPtrItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return "non-existent";
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);

        vogl_trace_ptr_value value = baseDesc.*m_pVal;
        return getValueFromPtrs((int*)&value, 1);
    }

    return "";
}

//=============================================================================

vogleditor_stateTreeVertexArrayUIntItem::vogleditor_stateTreeVertexArrayUIntItem(QString name, GLuint vogl_vertex_attrib_desc::* pVal, vogleditor_stateTreeItem* parent, const vogl_vertex_attrib_desc& state, unsigned int arrayIndex)
    : vogleditor_stateTreeVertexArrayDiffableItem(name, "", parent, arrayIndex),
      m_pState(&state),
      m_pVal(pVal)
{
    uint val = state.*pVal;
    setValue(val);
}

bool vogleditor_stateTreeVertexArrayUIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return true;
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);
        return (m_pState->*m_pVal) != (baseDesc.*m_pVal);
    }
}

QString vogleditor_stateTreeVertexArrayUIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return "non-existent";
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);

        uint value = baseDesc.*m_pVal;
        return getValueFromUints(&value, 1);
    }

    return "";
}

//=============================================================================

vogleditor_stateTreeVertexArrayIntItem::vogleditor_stateTreeVertexArrayIntItem(QString name, GLint vogl_vertex_attrib_desc::* pVal, vogleditor_stateTreeItem* parent, const vogl_vertex_attrib_desc& state, unsigned int arrayIndex)
    : vogleditor_stateTreeVertexArrayDiffableItem(name, "", parent, arrayIndex),
      m_pState(&state),
      m_pVal(pVal)
{
    uint val = state.*pVal;
    setValue(val);
}

bool vogleditor_stateTreeVertexArrayIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return true;
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);
        return (m_pState->*m_pVal) != (baseDesc.*m_pVal);
    }
}

QString vogleditor_stateTreeVertexArrayIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    if (m_arrayIndex >= m_pDiffBaseState->get_vertex_attrib_count())
    {
        // the current node does not exist in the base snapshot, so it must be new and different.
        return "non-existent";
    }
    else
    {
        const vogl_vertex_attrib_desc& baseDesc = m_pDiffBaseState->get_vertex_attrib_desc(m_arrayIndex);

        int value = baseDesc.*m_pVal;
        return getValueFromInts(&value, 1);
    }

    return "";
}

//=============================================================================

vogleditor_stateTreeVertexArrayItem::vogleditor_stateTreeVertexArrayItem(QString name, QString value, GLuint64 handle, vogleditor_stateTreeItem* parent, vogl_vao_state& state, const vogl_context_info& info)
    : vogleditor_stateTreeItem(name, value, parent),
      m_pState(&state),
      m_handle(handle),
      m_pDiffBaseState(NULL)
{
    static QString tmp;
    for (uint i = 0; i < info.get_max_vertex_attribs(); i++)
    {
       vogleditor_stateTreeItem* pAttribNode = new vogleditor_stateTreeItem(tmp.sprintf("GL_VERTEX_ATTRIB %u", i), "", this);
       this->appendChild(pAttribNode);

       const vogl_vertex_attrib_desc& desc = state.get_vertex_attrib_desc(i);

       { vogleditor_stateTreeVertexArrayUIntItem* pItem = new vogleditor_stateTreeVertexArrayUIntItem("GL_ELEMENT_ARRAY_BUFFER_BINDING", &vogl_vertex_attrib_desc::m_element_array_binding, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayUIntItem* pItem = new vogleditor_stateTreeVertexArrayUIntItem("GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING", &vogl_vertex_attrib_desc::m_array_binding, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayBoolItem* pItem = new vogleditor_stateTreeVertexArrayBoolItem("GL_VERTEX_ATTRIB_ARRAY_ENABLED", &vogl_vertex_attrib_desc::m_enabled, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayIntItem* pItem = new vogleditor_stateTreeVertexArrayIntItem("GL_VERTEX_ATTRIB_ARRAY_SIZE", &vogl_vertex_attrib_desc::m_size, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayEnumItem* pItem = new vogleditor_stateTreeVertexArrayEnumItem("GL_VERTEX_ATTRIB_ARRAY_TYPE", &vogl_vertex_attrib_desc::m_type, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayBoolItem* pItem = new vogleditor_stateTreeVertexArrayBoolItem("GL_VERTEX_ATTRIB_ARRAY_NORMALIZED", &vogl_vertex_attrib_desc::m_normalized, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayIntItem* pItem = new vogleditor_stateTreeVertexArrayIntItem("GL_VERTEX_ATTRIB_ARRAY_STRIDE", &vogl_vertex_attrib_desc::m_stride, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayBoolItem* pItem = new vogleditor_stateTreeVertexArrayBoolItem("GL_VERTEX_ATTRIB_ARRAY_INTEGER", &vogl_vertex_attrib_desc::m_integer, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayUIntItem* pItem = new vogleditor_stateTreeVertexArrayUIntItem("GL_VERTEX_ATTRIB_ARRAY_DIVISOR", &vogl_vertex_attrib_desc::m_divisor, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
       { vogleditor_stateTreeVertexArrayPtrItem* pItem = new vogleditor_stateTreeVertexArrayPtrItem("GL_VERTEX_ATTRIB_ARRAY_POINTER", &vogl_vertex_attrib_desc::m_pointer, pAttribNode, desc, i); m_diffableItems.push_back(pItem); pAttribNode->appendChild(pItem); }
    }
}
