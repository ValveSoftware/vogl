#ifndef VOGLEDITOR_STATETREEVERTEXARRAYITEM_H
#define VOGLEDITOR_STATETREEVERTEXARRAYITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_vao_state.h"

class vogleditor_stateTreeVertexArrayDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeVertexArrayDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent, unsigned int arrayIndex)
        : vogleditor_stateTreeItem(name, value, parent),
          m_arrayIndex(arrayIndex),
          m_pDiffBaseState(NULL)
    {
    }

    void set_diff_base_state(const vogl_vao_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    unsigned int get_array_index() const
    {
        return m_arrayIndex;
    }

    virtual bool hasChanged() const = 0;

protected:
    unsigned int m_arrayIndex;
    const vogl_vao_state *m_pDiffBaseState;
};

class vogleditor_stateTreeVertexArrayBoolItem : public vogleditor_stateTreeVertexArrayDiffableItem
{
public:
    vogleditor_stateTreeVertexArrayBoolItem(QString name, bool vogl_vertex_attrib_desc::*pVal, vogleditor_stateTreeItem *parent, const vogl_vertex_attrib_desc &state, unsigned int arrayIndex);
    vogleditor_stateTreeVertexArrayBoolItem(QString name, int vogl_vertex_attrib_desc::*pVal, vogleditor_stateTreeItem *parent, const vogl_vertex_attrib_desc &state, unsigned int arrayIndex);
    virtual ~vogleditor_stateTreeVertexArrayBoolItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_vertex_attrib_desc *m_pState;
    bool vogl_vertex_attrib_desc::*m_pVal;
    int vogl_vertex_attrib_desc::*m_pIntVal;
};

class vogleditor_stateTreeVertexArrayEnumItem : public vogleditor_stateTreeVertexArrayDiffableItem
{
public:
    vogleditor_stateTreeVertexArrayEnumItem(QString name, GLenum vogl_vertex_attrib_desc::*pVal, vogleditor_stateTreeItem *parent, const vogl_vertex_attrib_desc &state, unsigned int arrayIndex);
    virtual ~vogleditor_stateTreeVertexArrayEnumItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_vertex_attrib_desc *m_pState;
    GLenum vogl_vertex_attrib_desc::*m_pVal;
};

class vogleditor_stateTreeVertexArrayPtrItem : public vogleditor_stateTreeVertexArrayDiffableItem
{
public:
    vogleditor_stateTreeVertexArrayPtrItem(QString name, vogl_trace_ptr_value vogl_vertex_attrib_desc::*pVal, vogleditor_stateTreeItem *parent, const vogl_vertex_attrib_desc &state, unsigned int arrayIndex);
    virtual ~vogleditor_stateTreeVertexArrayPtrItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_vertex_attrib_desc *m_pState;
    vogl_trace_ptr_value vogl_vertex_attrib_desc::*m_pVal;
};

class vogleditor_stateTreeVertexArrayUIntItem : public vogleditor_stateTreeVertexArrayDiffableItem
{
public:
    vogleditor_stateTreeVertexArrayUIntItem(QString name, GLuint vogl_vertex_attrib_desc::*pVal, vogleditor_stateTreeItem *parent, const vogl_vertex_attrib_desc &state, unsigned int arrayIndex);
    virtual ~vogleditor_stateTreeVertexArrayUIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_vertex_attrib_desc *m_pState;
    GLuint vogl_vertex_attrib_desc::*m_pVal;
};

class vogleditor_stateTreeVertexArrayIntItem : public vogleditor_stateTreeVertexArrayDiffableItem
{
public:
    vogleditor_stateTreeVertexArrayIntItem(QString name, GLint vogl_vertex_attrib_desc::*pVal, vogleditor_stateTreeItem *parent, const vogl_vertex_attrib_desc &state, unsigned int arrayIndex);
    virtual ~vogleditor_stateTreeVertexArrayIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_vertex_attrib_desc *m_pState;
    GLint vogl_vertex_attrib_desc::*m_pVal;
};

class vogleditor_stateTreeElementArrayUIntItem : public vogleditor_stateTreeVertexArrayDiffableItem
{
public:
    vogleditor_stateTreeElementArrayUIntItem(QString name, GLuint (vogl_vao_state::*pVal)() const, vogleditor_stateTreeItem *parent, const vogl_vao_state &state);
    virtual ~vogleditor_stateTreeElementArrayUIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_vao_state *m_pState;
    GLuint (vogl_vao_state::*m_pVal)() const;
};

class vogleditor_stateTreeVertexArrayItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeVertexArrayItem(QString name, QString value, GLuint64 handle, vogleditor_stateTreeItem *parent, vogl_vao_state &state, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeVertexArrayItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_vao_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeVertexArrayDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

    GLuint64 get_handle() const
    {
        return m_handle;
    }

private:
    vogl_vao_state *m_pState;
    GLuint64 m_handle;
    const vogl_vao_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeVertexArrayDiffableItem *> m_diffableItems;
};
#endif // VOGLEDITOR_STATETREEVERTEXARRAYITEM_H
