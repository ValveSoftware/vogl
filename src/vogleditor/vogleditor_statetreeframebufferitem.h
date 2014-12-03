#ifndef VOGLEDITOR_STATETREEFRAMEBUFFERITEM_H
#define VOGLEDITOR_STATETREEFRAMEBUFFERITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_framebuffer_state;
class vogl_framebuffer_attachment;

class vogleditor_stateTreeFramebufferDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeFramebufferDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent, GLenum attachment, GLuint attachmentHandle)
        : vogleditor_stateTreeItem(name, value, parent),
          m_attachment(attachment),
          m_attachmentHandle(attachmentHandle),
          m_pDiffBaseState(NULL)
    {
    }

    void set_diff_base_state(const vogl_framebuffer_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    GLenum get_attachment() const
    {
        return m_attachment;
    }
    GLuint get_attachment_handle() const
    {
        return m_attachmentHandle;
    }

    virtual bool hasChanged() const = 0;

protected:
    GLenum m_attachment;
    GLuint m_attachmentHandle;
    const vogl_framebuffer_state *m_pDiffBaseState;
};

class vogleditor_stateTreeFramebufferBoolItem : public vogleditor_stateTreeFramebufferDiffableItem
{
public:
    vogleditor_stateTreeFramebufferBoolItem(QString name, GLenum enumName, GLenum attachment, GLuint attachmentHandle, vogleditor_stateTreeItem *parent, const vogl_framebuffer_attachment *pAttachment);
    virtual ~vogleditor_stateTreeFramebufferBoolItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;

    virtual QString getDiffedValue() const;

private:
    GLenum m_enumName;
    const vogl_framebuffer_attachment *m_pState;
};

class vogleditor_stateTreeFramebufferIntItem : public vogleditor_stateTreeFramebufferDiffableItem
{
public:
    vogleditor_stateTreeFramebufferIntItem(QString name, GLenum enumName, GLenum attachment, GLuint attachmentHandle, vogleditor_stateTreeItem *parent, const vogl_framebuffer_attachment *pAttachment);
    virtual ~vogleditor_stateTreeFramebufferIntItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;

    virtual QString getDiffedValue() const;

private:
    GLenum m_enumName;
    const vogl_framebuffer_attachment *m_pState;
};

class vogleditor_stateTreeFramebufferEnumItem : public vogleditor_stateTreeFramebufferDiffableItem
{
public:
    vogleditor_stateTreeFramebufferEnumItem(QString name, GLenum enumName, GLenum attachment, GLuint attachmentHandle, vogleditor_stateTreeItem *parent, const vogl_framebuffer_attachment *pAttachment);
    virtual ~vogleditor_stateTreeFramebufferEnumItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;

    virtual QString getDiffedValue() const;

private:
    GLenum m_enumName;
    const vogl_framebuffer_attachment *m_pState;
};

class vogleditor_stateTreeFramebufferReadbufferItem : public vogleditor_stateTreeFramebufferDiffableItem
{
public:
    vogleditor_stateTreeFramebufferReadbufferItem(QString name, vogleditor_stateTreeItem *parent, const vogl_framebuffer_state *pState);
    virtual ~vogleditor_stateTreeFramebufferReadbufferItem()
    {
        m_pState = NULL;
    }

    virtual bool hasChanged() const;

    virtual QString getDiffedValue() const;

private:
    const vogl_framebuffer_state *m_pState;
};

class vogleditor_stateTreeFramebufferItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeFramebufferItem(QString name, QString value, GLuint64 handle, vogleditor_stateTreeItem *parent, vogl_framebuffer_state *pState);
    virtual ~vogleditor_stateTreeFramebufferItem()
    {
        m_pFramebufferState = NULL;
    }
    virtual state_tree_type getStateType() const
    {
        return vogleditor_stateTreeItem::cFRAMEBUFFER;
    }

    vogl_framebuffer_state *get_framebuffer_state() const
    {
        return m_pFramebufferState;
    }

    void set_diff_base_state(const vogl_framebuffer_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;

        for (vogleditor_stateTreeFramebufferDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
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
    vogl_framebuffer_state *m_pFramebufferState;
    const vogl_framebuffer_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeFramebufferDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREEFRAMEBUFFERITEM_H
