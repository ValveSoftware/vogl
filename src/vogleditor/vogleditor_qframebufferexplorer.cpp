#include "vogleditor_qframebufferexplorer.h"
#include "ui_vogleditor_qframebufferexplorer.h"

#include "vogleditor_qtextureexplorer.h"

#include "vogl_gl_object.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_fbo_state.h"

typedef struct
{
    int index;
    vogl_framebuffer_state* pFBOState;
    vogl_default_framebuffer_state* pDefaultFBState;
} vogl_framebuffer_container;

Q_DECLARE_METATYPE(vogl_framebuffer_container);

vogleditor_QFramebufferExplorer::vogleditor_QFramebufferExplorer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::vogleditor_QFramebufferExplorer),
    m_pDefaultFramebufferState(NULL)
{
    ui->setupUi(this);

    m_colorExplorerLayout = new QVBoxLayout();
    m_depthExplorerLayout = new QVBoxLayout();
    m_stencilExplorerLayout = new QVBoxLayout();

    ui->colorBufferGroupBox->setLayout(m_colorExplorerLayout);

    m_depthExplorer = new vogleditor_QTextureExplorer(ui->depthBufferGroupBox);
    m_depthExplorerLayout->addWidget(m_depthExplorer);
    ui->depthBufferGroupBox->setLayout(m_depthExplorerLayout);

    m_stencilExplorer = new vogleditor_QTextureExplorer(ui->stencilBufferGroupBox);
    m_stencilExplorerLayout->addWidget(m_stencilExplorer);
    ui->stencilBufferGroupBox->setLayout(m_stencilExplorerLayout);

    connect(ui->framebufferObjectListbox, SIGNAL(currentIndexChanged(int)), this, SLOT(selectedFramebufferIndexChanged(int)));
}

vogleditor_QFramebufferExplorer::~vogleditor_QFramebufferExplorer()
{
    delete ui;

    delete m_colorExplorerLayout;
    delete m_depthExplorerLayout;
    delete m_stencilExplorerLayout;

    delete m_depthExplorer;
    m_depthExplorer = NULL;

    delete m_stencilExplorer;
    m_stencilExplorer = NULL;
}

void vogleditor_QFramebufferExplorer::clear()
{
    ui->framebufferObjectListbox->clear();

    clearViewers();
}

void vogleditor_QFramebufferExplorer::clearViewers()
{
    m_depthExplorer->clear();
    m_stencilExplorer->clear();

    for (vogleditor_QTextureExplorer** iter = m_viewers.begin(); iter != m_viewers.end(); iter++)
    {
        m_colorExplorerLayout->takeAt(0);
        (*iter)->clear();
        delete (*iter);
        *iter = NULL;
    }

    m_viewers.clear();
}

void vogleditor_QFramebufferExplorer::set_framebuffer_objects(vogl_gl_object_state_ptr_vec objects, vogl_context_snapshot& context, vogl_default_framebuffer_state& defaultFramebufferState)
{
    clear();

    m_context = &context;
    m_objects = objects;
    m_pDefaultFramebufferState = &defaultFramebufferState;

    // add default framebuffer
    vogl_framebuffer_container defaultContainer;
    defaultContainer.index = 0;
    defaultContainer.pFBOState = NULL;
    defaultContainer.pDefaultFBState = m_pDefaultFramebufferState;
    ui->framebufferObjectListbox->addItem("Framebuffer 0 - (default framebuffer)", QVariant::fromValue(defaultContainer));

    // add framebuffer objects
    QString valueStr;
    for (vogl_gl_object_state_ptr_vec::iterator iter = objects.begin(); iter != objects.end(); iter++)
    {
        vogl_framebuffer_state* pState = static_cast<vogl_framebuffer_state*>(*iter);

        if (pState->get_attachments().size() > 0)
        {
            unsigned int width = 0;
            unsigned int height = 0;
            const vogl_framebuffer_attachment* pAttachment = &(pState->get_attachments().begin()->second);
            if (pAttachment->get_type() == GL_TEXTURE)
            {
                vogl_texture_state* pTexState = this->get_texture_attachment(NULL, pAttachment->get_handle());
                if (pTexState != NULL)
                {
                    width = pTexState->get_texture().get_width();
                    height = pTexState->get_texture().get_height();
                }
            }
            else if (pAttachment->get_type() == GL_RENDERBUFFER)
            {
                vogl_renderbuffer_state* pRbState = this->get_renderbuffer_attachment(NULL, pAttachment->get_handle());
                if (pRbState != NULL)
                {
                    width = pRbState->get_texture().get_texture().get_width();
                    height = pRbState->get_texture().get_texture().get_height();
                }
            }

            valueStr = valueStr.sprintf("Framebuffer %" PRIu64 " - (%u x %u) %d attachments", pState->get_snapshot_handle(), width, height, pState->get_attachments().size());
        }
        else
        {
            valueStr = valueStr.sprintf("Framebuffer %" PRIu64 " - %d attachments", pState->get_snapshot_handle(), pState->get_attachments().size());
        }

        vogl_framebuffer_container defaultContainer;
        defaultContainer.index = 0;
        defaultContainer.pFBOState = pState;
        defaultContainer.pDefaultFBState = NULL;

        ui->framebufferObjectListbox->addItem(valueStr, QVariant::fromValue(defaultContainer));
    }
}

bool vogleditor_QFramebufferExplorer::set_active_framebuffer(unsigned long long framebufferHandle)
{
    bool bDisplayedFramebuffer = false;

    if (framebufferHandle == 0)
    {
        ui->framebufferObjectListbox->setCurrentIndex(0);
        bDisplayedFramebuffer = true;
    }
    else
    {
        int index = 1;
        for (vogl_gl_object_state_ptr_vec::iterator iter = m_objects.begin(); iter != m_objects.end(); iter++)
        {
            vogl_framebuffer_state* pState = static_cast<vogl_framebuffer_state*>(*iter);
            if (pState->get_snapshot_handle() == framebufferHandle)
            {
                ui->framebufferObjectListbox->setCurrentIndex(index);
                bDisplayedFramebuffer = true;
                break;
            }

            ++index;
        }
    }
    return bDisplayedFramebuffer;
}

void vogleditor_QFramebufferExplorer::selectedFramebufferIndexChanged(int index)
{
    clearViewers();

    if (index < 0 || index >= ui->framebufferObjectListbox->count())
    {
        return;
    }

    vogl_gl_object_state_ptr_vec colorVec;
    vogl_gl_object_state_ptr_vec depthVec;
    vogl_gl_object_state_ptr_vec stencilVec;

#define ADD_COLOR_BUFFER_VIEWER vogleditor_QTextureExplorer* pViewer = new vogleditor_QTextureExplorer(ui->colorBufferGroupBox); m_colorExplorerLayout->addWidget(pViewer); m_viewers.push_back(pViewer);

    vogl_framebuffer_container container = ui->framebufferObjectListbox->itemData(index).value<vogl_framebuffer_container>();
    if (index == 0)
    {
        vogl_default_framebuffer_state* pDefaultState = container.pDefaultFBState;

        vogl_texture_state& rFrontLeft = pDefaultState->get_texture(cDefFramebufferFrontLeft);
        vogl_texture_state& rBackLeft = pDefaultState->get_texture(cDefFramebufferBackLeft);
        vogl_texture_state& rFrontRight = pDefaultState->get_texture(cDefFramebufferFrontRight);
        vogl_texture_state& rBackRight = pDefaultState->get_texture(cDefFramebufferBackRight);
        vogl_texture_state& rDepthStencil = pDefaultState->get_texture(cDefFramebufferDepthStencil);

        if (rFrontLeft.is_valid())
        {
            colorVec.push_back(&rFrontLeft);
            ADD_COLOR_BUFFER_VIEWER
            pViewer->add_texture_object(rFrontLeft, "GL_FRONT_LEFT");
        }
        if (rBackLeft.is_valid())
        {
            colorVec.push_back(&rBackLeft);
            ADD_COLOR_BUFFER_VIEWER
            pViewer->add_texture_object(rBackLeft, "GL_BACK_LEFT");
        }
        if (rFrontRight.is_valid())
        {
            colorVec.push_back(&rFrontRight);
            ADD_COLOR_BUFFER_VIEWER
            pViewer->add_texture_object(rFrontRight, "GL_FRONT_RIGHT");
        }
        if (rBackRight.is_valid())
        {
            colorVec.push_back(&rBackRight);
            ADD_COLOR_BUFFER_VIEWER
            pViewer->add_texture_object(rBackRight, "GL_BACK_RIGHT");
        }

        if (rDepthStencil.is_valid()) { depthVec.push_back(&rDepthStencil); m_depthExplorer->add_texture_object(rDepthStencil, "GL_DEPTH"); }
        if (rDepthStencil.is_valid()) { stencilVec.push_back(&rDepthStencil); m_stencilExplorer->add_texture_object(rDepthStencil, "GL_STENCIL"); }
    }
    else
    {
        vogl_framebuffer_state* pState = container.pFBOState;
        if (pState != NULL)
        {
            vogl_gl_object_state_ptr_vec textureVec;
            m_context->get_all_objects_of_category(cGLSTTexture, textureVec);

            vogl_gl_object_state_ptr_vec renderbufferVec;
            m_context->get_all_objects_of_category(cGLSTRenderbuffer, renderbufferVec);

            const vogl_framebuffer_state::GLenum_to_attachment_map& rAttachments = pState->get_attachments();
            for (vogl_framebuffer_state::GLenum_to_attachment_map::const_iterator iter = rAttachments.begin(); iter != rAttachments.end(); iter++)
            {
                const vogl_framebuffer_attachment* pAttachment = &(iter->second);
                if (pAttachment->get_type() == GL_TEXTURE)
                {
                    vogl_texture_state* pTexState = this->get_texture_attachment(&textureVec, pAttachment->get_handle());
                    if (pTexState != NULL)
                    {
                        if (iter->first == GL_DEPTH_ATTACHMENT ||
                            iter->first == GL_DEPTH)
                        {
                            depthVec.push_back(pTexState);
                        }
                        else if (iter->first == GL_STENCIL_ATTACHMENT ||
                                 iter->first == GL_STENCIL)
                        {
                            stencilVec.push_back(pTexState);
                        }
                        else
                        {
                            colorVec.push_back(pTexState);

                            ADD_COLOR_BUFFER_VIEWER
                        }
                    }
                }
                else if (pAttachment->get_type() == GL_RENDERBUFFER)
                {
                    vogl_renderbuffer_state* pRbState = this->get_renderbuffer_attachment(&renderbufferVec, pAttachment->get_handle());
                    if (pRbState != NULL)
                    {
                        if (iter->first == GL_DEPTH_ATTACHMENT ||
                            iter->first == GL_DEPTH)
                        {
                            depthVec.push_back(pRbState);
                        }
                        else if (iter->first == GL_STENCIL_ATTACHMENT ||
                                 iter->first == GL_STENCIL)
                        {
                            stencilVec.push_back(pRbState);
                        }
                        else
                        {
                            colorVec.push_back(pRbState);

                            ADD_COLOR_BUFFER_VIEWER
                        }
                    }
                }
                else
                {
                    VOGL_ASSERT(!"Unhandled framebuffer attachment type");
                }
            }
        }
    }

#undef ADD_COLOR_BUFFER_VIEWER

    if (colorVec.size() == 0)
    {
        ui->colorBufferGroupBox->setMinimumHeight(50);
    }
    else
    {
        uint totalHeight = 0;
        uint viewerIndex = 0;
        for (vogleditor_QTextureExplorer** iter = m_viewers.begin(); iter != m_viewers.end(); iter++)
        {
            (*iter)->set_zoom_factor(0.2);

            // use index which was passed in as parameter
            if (index != 0)
            {
                (*iter)->set_texture_objects(colorVec);
                (*iter)->set_active_texture(colorVec[viewerIndex]->get_snapshot_handle());
            }

            // for better visibility, adjust height based on combined preferred heights
            totalHeight += (*iter)->get_preferred_height();
            (*iter)->setFixedHeight((*iter)->get_preferred_height());
            ++viewerIndex;

            connect(*iter, SIGNAL(zoomFactorChanged(double)), this, SLOT(slot_zoomFactorChanged(double)));
        }
    }

    if (depthVec.size() == 0)
    {
        m_depthExplorer->setVisible(false);
        ui->depthBufferGroupBox->setMinimumHeight(50);
    }
    else
    {
        m_depthExplorer->setVisible(true);
        m_depthExplorer->set_zoom_factor(0.2);
        // use index which was passed in as parameter
        if (index != 0)
        {
            m_depthExplorer->set_texture_objects(depthVec);
            m_depthExplorer->set_active_texture(depthVec[0]->get_snapshot_handle());
        }
        connect(m_depthExplorer, SIGNAL(zoomFactorChanged(double)), this, SLOT(slot_zoomFactorChanged(double)));
        m_depthExplorer->setMinimumHeight(m_depthExplorer->get_preferred_height());
    }

    if (stencilVec.size() == 0)
    {
        m_stencilExplorer->setVisible(false);
        ui->stencilBufferGroupBox->setMinimumHeight(50);
    }
    else
    {
        m_stencilExplorer->setVisible(true);
        m_stencilExplorer->set_zoom_factor(0.2);
        // use index which was passed in as parameter
        if (index != 0)
        {
            m_stencilExplorer->set_texture_objects(stencilVec);
            m_stencilExplorer->set_active_texture(stencilVec[0]->get_snapshot_handle());
        }
        connect(m_stencilExplorer, SIGNAL(zoomFactorChanged(double)), this, SLOT(slot_zoomFactorChanged(double)));
        m_stencilExplorer->setMinimumHeight(m_stencilExplorer->get_preferred_height());
    }
}

vogl_texture_state* vogleditor_QFramebufferExplorer::get_texture_attachment(vogl_gl_object_state_ptr_vec* pObjectVec, unsigned int handle)
{
    vogl_gl_object_state_ptr_vec textureVec;
    if (pObjectVec == NULL)
    {
        m_context->get_all_objects_of_category(cGLSTTexture, textureVec);
        pObjectVec = &textureVec;
    }

    vogl_texture_state* pTexState = NULL;
    for (vogl_gl_object_state_ptr_vec::iterator texIter = pObjectVec->begin(); texIter != pObjectVec->end(); texIter++)
    {
        if ((*texIter)->get_snapshot_handle() == handle)
        {
            pTexState = static_cast<vogl_texture_state*>(*texIter);
            break;
        }
    }

    return pTexState;
}

vogl_renderbuffer_state* vogleditor_QFramebufferExplorer::get_renderbuffer_attachment(vogl_gl_object_state_ptr_vec* pObjectVec, unsigned int handle)
{
    vogl_gl_object_state_ptr_vec renderbufferVec;
    if (pObjectVec == NULL)
    {
        m_context->get_all_objects_of_category(cGLSTRenderbuffer, renderbufferVec);
        pObjectVec = &renderbufferVec;
    }

    vogl_renderbuffer_state* pRenderbufferState = NULL;
    for (vogl_gl_object_state_ptr_vec::iterator texIter = pObjectVec->begin(); texIter != pObjectVec->end(); texIter++)
    {
        if ((*texIter)->get_snapshot_handle() == handle)
        {
            pRenderbufferState = static_cast<vogl_renderbuffer_state*>(*texIter);
            break;
        }
    }

    return pRenderbufferState;
}

void vogleditor_QFramebufferExplorer::slot_zoomFactorChanged(double zoomFactor)
{
    VOGL_NOTE_UNUSED(zoomFactor);

    uint totalHeight = 0;
    for (vogleditor_QTextureExplorer** iter = m_viewers.begin(); iter != m_viewers.end(); iter++)
    {
        // for better visibility, adjust height based on combined preferred heights
        totalHeight += (*iter)->get_preferred_height();
        (*iter)->setFixedHeight((*iter)->get_preferred_height());
    }

    m_depthExplorer->setMinimumHeight(m_depthExplorer->get_preferred_height());
    m_stencilExplorer->setMinimumHeight(m_stencilExplorer->get_preferred_height());
}
