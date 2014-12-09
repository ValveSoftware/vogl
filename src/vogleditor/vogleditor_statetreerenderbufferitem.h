#ifndef VOGLEDITOR_STATETREERENDERBUFFERITEM_H
#define VOGLEDITOR_STATETREERENDERBUFFERITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_renderbuffer_state;

class vogleditor_stateTreeRenderbufferDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeRenderbufferDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pDiffBaseState(NULL)
    {
    }

    void set_diff_base_state(const vogl_renderbuffer_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const = 0;

protected:
    const vogl_renderbuffer_state *m_pDiffBaseState;
};

class vogleditor_stateTreeRenderbufferIntItem : public vogleditor_stateTreeRenderbufferDiffableItem
{
public:
    vogleditor_stateTreeRenderbufferIntItem(QString name, GLenum enumName, vogleditor_stateTreeItem *parent, const vogl_renderbuffer_state &state);
    virtual ~vogleditor_stateTreeRenderbufferIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    GLenum m_enum;
    const vogl_renderbuffer_state *m_pState;
};

class vogleditor_stateTreeRenderbufferEnumItem : public vogleditor_stateTreeRenderbufferDiffableItem
{
public:
    vogleditor_stateTreeRenderbufferEnumItem(QString name, GLenum enumName, vogleditor_stateTreeItem *parent, const vogl_renderbuffer_state &state);
    virtual ~vogleditor_stateTreeRenderbufferEnumItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    GLenum m_enum;
    const vogl_renderbuffer_state *m_pState;
};

class vogleditor_stateTreeRenderbufferItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeRenderbufferItem(QString name, GLuint64 handle, vogleditor_stateTreeItem *parent, const vogl_renderbuffer_state &state);
    virtual ~vogleditor_stateTreeRenderbufferItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_renderbuffer_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeRenderbufferDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

    GLuint64 get_handle() const
    {
        return m_handle;
    }

private:
    GLuint64 m_handle;
    const vogl_renderbuffer_state *m_pState;
    const vogl_renderbuffer_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeRenderbufferDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREERENDERBUFFERITEM_H
