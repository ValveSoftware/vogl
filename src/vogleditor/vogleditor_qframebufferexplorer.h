#ifndef VOGLEDITOR_QFRAMEBUFFEREXPLORER_H
#define VOGLEDITOR_QFRAMEBUFFEREXPLORER_H

#include <QWidget>
#include "vogl_core.h"

class QVBoxLayout;
class QTextureViewer;
class vogleditor_QTextureExplorer;
class vogl_context_snapshot;
class vogl_default_framebuffer_state;
class vogl_gl_object_state;
class vogl_renderbuffer_state;
class vogl_texture_state;
typedef vogl::vector<vogl_gl_object_state *> vogl_gl_object_state_ptr_vec;

class vogl_framebuffer_state;

namespace Ui
{
    class vogleditor_QFramebufferExplorer;
}

class vogleditor_QFramebufferExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QFramebufferExplorer(QWidget *parent = 0);
    ~vogleditor_QFramebufferExplorer();

    uint set_framebuffer_objects(vogl_context_snapshot *pContext, vogl::vector<vogl_context_snapshot *> sharingContexts, vogl_default_framebuffer_state *pDefaultFramebufferState);

    bool set_active_framebuffer(unsigned long long framebufferHandle);

    void clear();

private:
    Ui::vogleditor_QFramebufferExplorer *ui;
    vogl_gl_object_state_ptr_vec m_objects;
    vogl::vector<vogleditor_QTextureExplorer *> m_viewers;
    QVBoxLayout *m_colorExplorerLayout;
    QVBoxLayout *m_depthExplorerLayout;
    QVBoxLayout *m_stencilExplorerLayout;
    vogleditor_QTextureExplorer *m_depthExplorer;
    vogleditor_QTextureExplorer *m_stencilExplorer;
    vogl::vector<vogl_context_snapshot *> m_sharing_contexts;
    vogl_default_framebuffer_state *m_pDefaultFramebufferState;
    bool m_bDelayZoomFactorChanged;

    void clearViewers();

    uint set_default_framebuffer(vogl_default_framebuffer_state *pDefaultFramebufferState);
    uint add_framebuffer_objects(vogl::vector<vogl_context_snapshot *> sharingContexts, vogl_gl_object_state_ptr_vec objects);

    vogl_texture_state *get_texture_attachment(vogl_context_snapshot &context, unsigned int handle);
    vogl_renderbuffer_state *get_renderbuffer_attachment(vogl_context_snapshot &context, unsigned int handle);

private
slots:
    void selectedFramebufferIndexChanged(int index);
    void slot_zoomFactorChanged(double zoomFactor);
};

#endif // VOGLEDITOR_QFRAMEBUFFEREXPLORER_H
