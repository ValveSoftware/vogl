#include "vogleditor_statetreeframebufferitem.h"

#include "vogl_fbo_state.h"

vogleditor_stateTreeFramebufferBoolItem::vogleditor_stateTreeFramebufferBoolItem(QString name, GLenum enumName, GLenum attachment, GLuint attachmentHandle, vogleditor_stateTreeItem *parent, const vogl_framebuffer_attachment *pAttachment)
    : vogleditor_stateTreeFramebufferDiffableItem(name, "", parent, attachment, attachmentHandle),
      m_enumName(enumName),
      m_pState(pAttachment)
{
    int val = pAttachment->get_param(enumName);
    setValue(val != 0);
}

bool vogleditor_stateTreeFramebufferBoolItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    const vogl_framebuffer_state::GLenum_to_attachment_map &rAttachments = m_pDiffBaseState->get_attachments();
    vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.find(m_attachment);
    if (iter == rAttachments.end())
        return true;

    return (m_pState->get_param(m_enumName) != ((vogl_framebuffer_attachment)iter->second).get_param(m_enumName));
}

QString vogleditor_stateTreeFramebufferBoolItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    const vogl_framebuffer_state::GLenum_to_attachment_map &rAttachments = m_pDiffBaseState->get_attachments();
    vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.find(m_attachment);
    if (iter == rAttachments.end())
        return "non-existent";

    bool bVal = ((vogl_framebuffer_attachment)iter->second).get_param(m_enumName);
    return getValueFromBools(&bVal, 1);
}

//=============================================================================

vogleditor_stateTreeFramebufferIntItem::vogleditor_stateTreeFramebufferIntItem(QString name, GLenum enumName, GLenum attachment, GLuint attachmentHandle, vogleditor_stateTreeItem *parent, const vogl_framebuffer_attachment *pAttachment)
    : vogleditor_stateTreeFramebufferDiffableItem(name, "", parent, attachment, attachmentHandle),
      m_enumName(enumName),
      m_pState(pAttachment)
{
    int val = pAttachment->get_param(enumName);
    setValue(val);
}

bool vogleditor_stateTreeFramebufferIntItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    const vogl_framebuffer_state::GLenum_to_attachment_map &rAttachments = m_pDiffBaseState->get_attachments();
    vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.find(m_attachment);
    if (iter == rAttachments.end())
        return true;

    return (m_pState->get_param(m_enumName) != ((vogl_framebuffer_attachment)iter->second).get_param(m_enumName));
}

QString vogleditor_stateTreeFramebufferIntItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    const vogl_framebuffer_state::GLenum_to_attachment_map &rAttachments = m_pDiffBaseState->get_attachments();
    vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.find(m_attachment);
    if (iter == rAttachments.end())
        return "non-existent";

    int val = ((vogl_framebuffer_attachment)iter->second).get_param(m_enumName);
    return getValueFromInts(&val, 1);
}

//=============================================================================

vogleditor_stateTreeFramebufferEnumItem::vogleditor_stateTreeFramebufferEnumItem(QString name, GLenum enumName, GLenum attachment, GLuint attachmentHandle, vogleditor_stateTreeItem *parent, const vogl_framebuffer_attachment *pAttachment)
    : vogleditor_stateTreeFramebufferDiffableItem(name, "", parent, attachment, attachmentHandle),
      m_enumName(enumName),
      m_pState(pAttachment)
{
    int val = pAttachment->get_param(enumName);
    setValue(enum_to_string(val));
}

bool vogleditor_stateTreeFramebufferEnumItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    const vogl_framebuffer_state::GLenum_to_attachment_map &rAttachments = m_pDiffBaseState->get_attachments();
    vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.find(m_attachment);
    if (iter == rAttachments.end())
        return true;

    return (m_pState->get_param(m_enumName) != ((vogl_framebuffer_attachment)iter->second).get_param(m_enumName));
}

QString vogleditor_stateTreeFramebufferEnumItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    const vogl_framebuffer_state::GLenum_to_attachment_map &rAttachments = m_pDiffBaseState->get_attachments();
    vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.find(m_attachment);
    if (iter == rAttachments.end())
        return "non-existent";

    int val = ((vogl_framebuffer_attachment)iter->second).get_param(m_enumName);
    return getValueFromEnums(&val, 1);
}

//=============================================================================

vogleditor_stateTreeFramebufferReadbufferItem::vogleditor_stateTreeFramebufferReadbufferItem(QString name, vogleditor_stateTreeItem *parent, const vogl_framebuffer_state *pState)
    : vogleditor_stateTreeFramebufferDiffableItem(name, "", parent, 0, 0),
      m_pState(pState)
{
    setValue(enum_to_string(m_pState->get_read_buffer()));
}

bool vogleditor_stateTreeFramebufferReadbufferItem::hasChanged() const
{
    if (m_pDiffBaseState == NULL)
        return false;

    return (m_pState->get_read_buffer() != m_pDiffBaseState->get_read_buffer());
}

QString vogleditor_stateTreeFramebufferReadbufferItem::getDiffedValue() const
{
    if (m_pDiffBaseState == NULL)
        return "";

    int val = m_pDiffBaseState->get_read_buffer();
    return getValueFromEnums(&val, 1);
}

//=============================================================================

vogleditor_stateTreeFramebufferItem::vogleditor_stateTreeFramebufferItem(QString name, QString value, GLuint64 handle, vogleditor_stateTreeItem *parent, vogl_framebuffer_state *pState)
    : vogleditor_stateTreeItem(name, value, parent),
      m_handle(handle),
      m_pFramebufferState(pState)
{
    QString tmp;
    vogleditor_stateTreeFramebufferReadbufferItem *pReadbufferItem = new vogleditor_stateTreeFramebufferReadbufferItem("GL_READ_BUFFER", this, m_pFramebufferState);
    m_diffableItems.push_back(pReadbufferItem);
    this->appendChild(pReadbufferItem);

    const uint_vec &rDrawBuffers = m_pFramebufferState->get_draw_buffers();
    for (uint i = 0; i < rDrawBuffers.size(); i++)
    {
        this->appendChild(new vogleditor_stateTreeItem(tmp.sprintf("GL_DRAW_BUFFER%d", i), enum_to_string(rDrawBuffers[i]), this));
    }

    // TODO: support MAX_COLOR_ATTACHMENT
    //                  int max_color_attachments = 0;
    //                  GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);

    const vogl_framebuffer_state::GLenum_to_attachment_map &rAttachments = m_pFramebufferState->get_attachments();

    for (vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.begin(); iter != rAttachments.end(); iter++)
    {
        GLenum attachment = iter->first;
        const vogl_framebuffer_attachment *pAttachment = &(iter->second);
        vogleditor_stateTreeItem *pAttachmentNode = new vogleditor_stateTreeItem(enum_to_string(attachment), int_to_string(pAttachment->get_handle()) + " (" + enum_to_string(pAttachment->get_type()) + ")", this);
        this->appendChild(pAttachmentNode);

#define GET_BOOL(name)                                                                                                                                                                  \
    {                                                                                                                                                                                   \
        vogleditor_stateTreeFramebufferBoolItem *pItem = new vogleditor_stateTreeFramebufferBoolItem(#name, name, attachment, pAttachment->get_handle(), pAttachmentNode, pAttachment); \
        m_diffableItems.push_back(pItem);                                                                                                                                               \
        pAttachmentNode->appendChild(pItem);                                                                                                                                            \
    }
#define GET_INT(name)                                                                                                                                                                 \
    {                                                                                                                                                                                 \
        vogleditor_stateTreeFramebufferIntItem *pItem = new vogleditor_stateTreeFramebufferIntItem(#name, name, attachment, pAttachment->get_handle(), pAttachmentNode, pAttachment); \
        m_diffableItems.push_back(pItem);                                                                                                                                             \
        pAttachmentNode->appendChild(pItem);                                                                                                                                          \
    }
#define GET_ENUM(name)                                                                                                                                                                  \
    {                                                                                                                                                                                   \
        vogleditor_stateTreeFramebufferEnumItem *pItem = new vogleditor_stateTreeFramebufferEnumItem(#name, name, attachment, pAttachment->get_handle(), pAttachmentNode, pAttachment); \
        m_diffableItems.push_back(pItem);                                                                                                                                               \
        pAttachmentNode->appendChild(pItem);                                                                                                                                            \
    }
        GET_INT(GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE);
        GET_INT(GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE);
        GET_INT(GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE);
        GET_INT(GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE);
        GET_INT(GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE);
        GET_INT(GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE);
        GET_ENUM(GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE);
        GET_BOOL(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING);

        if (pAttachment->get_type() == GL_TEXTURE)
        {
            GET_INT(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL);
            GET_ENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE);
            GET_INT(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER);
            GET_BOOL(GL_FRAMEBUFFER_ATTACHMENT_LAYERED);
        }
#undef GET_BOOL
#undef GET_INT
#undef GET_ENUM
    }
}

//#undef STR_INT
