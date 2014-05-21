/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

// File: vogl_replay_window.cpp
#include "vogl_replay_window.h"

vogl_replay_window::vogl_replay_window()
: 
#if VOGL_PLATFORM_HAS_SDL
      m_win(NULL)
#elif VOGL_PLATFORM_HAS_X11
      m_win((Window)NULL)
    , m_dpy(NULL)
    , m_pFB_configs(NULL)
    , m_num_fb_configs(0)
#endif
, m_width(0)
, m_height(0)
{
    VOGL_FUNC_TRACER
}

vogl_replay_window::~vogl_replay_window()
{
    VOGL_FUNC_TRACER

    close();
}

bool vogl_replay_window::open(int width, int height, int samples)
{
    VOGL_FUNC_TRACER
    close();

    const char *pWindow_name = (sizeof(void *) == sizeof(uint32_t)) ? "voglreplay 32-bit" : "voglreplay 64-bit";

    #if (VOGL_PLATFORM_HAS_SDL)

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
        if (samples > 1) {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
        }

        m_win = SDL_CreateWindow(pWindow_name, 20, 20, width, height, SDL_WINDOW_OPENGL);
        // Don't actually create the context here, that will be done later.
        // TODO: Support better window creation modes, like with a particular backbuffer format.

    #elif (VOGL_PLATFORM_HAS_GLX)


        if (!check_glx_version())
            return false;

        // TODO: These attribs (especially the sizes) should be passed in by the caller!
        int fbAttribs[64];

        int *pAttribs = fbAttribs;

        *pAttribs++ = GLX_RENDER_TYPE;      *pAttribs++ = GLX_RGBA_BIT;
        *pAttribs++ = GLX_X_RENDERABLE;     *pAttribs++ = True;
        *pAttribs++ = GLX_DRAWABLE_TYPE;    *pAttribs++ = GLX_WINDOW_BIT;
        *pAttribs++ = GLX_DOUBLEBUFFER;     *pAttribs++ = True;
        *pAttribs++ = GLX_RED_SIZE;         *pAttribs++ = 8;
        *pAttribs++ = GLX_BLUE_SIZE;        *pAttribs++ = 8;
        *pAttribs++ = GLX_GREEN_SIZE;       *pAttribs++ = 8;
        *pAttribs++ = GLX_ALPHA_SIZE;       *pAttribs++ = 8;
        *pAttribs++ = GLX_DEPTH_SIZE;       *pAttribs++ = 24;
        *pAttribs++ = GLX_STENCIL_SIZE;     *pAttribs++ = 8;

        if (samples > 1)
        {
            *pAttribs++ = GLX_SAMPLE_BUFFERS; *pAttribs++ = 1;
            *pAttribs++ = GLX_SAMPLES;        *pAttribs++ = samples;
        }

        *pAttribs++ = 0;

        // Tell X we are going to use the display
        m_dpy = XOpenDisplay(NULL);
        if (!m_dpy)
        {
            console::error("%s: XOpenDisplay() failed!\n", VOGL_FUNCTION_INFO_CSTR);
            return false;
        }

        // Get a new fb config that meets our attrib requirements

        m_pFB_configs = GL_ENTRYPOINT(glXChooseFBConfig)(m_dpy, DefaultScreen(m_dpy), fbAttribs, &m_num_fb_configs);
        if ((!m_pFB_configs) || (!m_num_fb_configs))
        {
            console::error("%s: glXChooseFBConfig() failed!\n", VOGL_FUNCTION_INFO_CSTR);
            return false;
        }

        XVisualInfo *pVisual_info = GL_ENTRYPOINT(glXGetVisualFromFBConfig)(m_dpy, m_pFB_configs[0]);
        if (!pVisual_info)
        {
            console::error("%s: glXGetVisualFromFBConfig() failed!\n", VOGL_FUNCTION_INFO_CSTR);
            return false;
        }

        // Now create an X window
        XSetWindowAttributes winAttribs;
        winAttribs.event_mask = ExposureMask | VisibilityChangeMask |
                                KeyPressMask | PointerMotionMask |
                                StructureNotifyMask;

        winAttribs.border_pixel = 0;
        winAttribs.bit_gravity = StaticGravity;
        winAttribs.colormap = XCreateColormap(m_dpy,
                                              RootWindow(m_dpy, pVisual_info->screen),
                                              pVisual_info->visual, AllocNone);
        GLint winmask = CWBorderPixel | CWBitGravity | CWEventMask | CWColormap;

        m_win = XCreateWindow(m_dpy, DefaultRootWindow(m_dpy), 20, 20,
                              width, height, 0,
                              pVisual_info->depth, InputOutput,
                              pVisual_info->visual, winmask, &winAttribs);

        
        XStoreName(m_dpy, m_win, pWindow_name);
        XSetIconName(m_dpy, m_win, pWindow_name);

        XSizeHints sh;
        utils::zero_object(sh);
        sh.x = 0; // slam position up so when/if we resize the window glReadPixels still works as expected (this may be a bug in the NV driver, I dunno yet)
        sh.y = 0;
        sh.width = sh.min_width = sh.max_width = sh.base_width = width;
        sh.height = sh.min_height = sh.max_height = sh.base_height = height;
        sh.flags = PSize | PMinSize | PMaxSize | PBaseSize | PPosition;
        XSetWMNormalHints(m_dpy, m_win, &sh);

        XResizeWindow(m_dpy, m_win, width, height);

        XMapWindow(m_dpy, m_win);

        //glXWaitX();
    #else
        #error "Need vogl_replay_window::open for this platform"
        return false;
    #endif

    m_width = width;
    m_height = height;

    uint32_t actual_width = 0, actual_height = 0;
    vogl_replay_window::get_actual_dimensions(actual_width, actual_height);
    vogl_debug_printf("%s: Created window, requested dimensions %ux%u, actual dimensions %ux%u\n", VOGL_FUNCTION_INFO_CSTR, m_width, m_height, actual_width, actual_height);

    return true;
}

void vogl_replay_window::set_title(const char *pTitle)
{
    VOGL_FUNC_TRACER
    #if (VOGL_PLATFORM_HAS_SDL)
        if (m_win)
        {
            SDL_SetWindowTitle(m_win, pTitle);
        }
    #elif (VOGL_PLATFORM_HAS_GLX)
        if (m_win)
        {
            XStoreName(m_dpy, m_win, pTitle);
        }
    #else
        VOGL_ASSERT(!"impl");
        #error "Need vogl_replay_window::set_title this platform."
    #endif
}

bool vogl_replay_window::resize(int new_width, int new_height)
{
    VOGL_FUNC_TRACER

    if (!is_opened())
        return open(new_width, new_height);

    if ((new_width == m_width) && (new_height == m_height))
        return true;

    #if (VOGL_PLATFORM_HAS_SDL)
        SDL_SetWindowSize(m_win, new_width, new_height);

    #elif (VOGL_PLATFORM_HAS_GLX)

        XSizeHints sh;
        utils::zero_object(sh);
        sh.width = sh.min_width = sh.max_width = sh.base_width = new_width;
        sh.height = sh.min_height = sh.max_height = sh.base_height = new_height;
        sh.flags = PSize | PMinSize | PMaxSize | PBaseSize;
        XSetWMNormalHints(m_dpy, m_win, &sh);
        //XMapWindow(dpy, win);

        int status = XResizeWindow(m_dpy, m_win, new_width, new_height);
        VOGL_ASSERT(status == True);
        VOGL_NOTE_UNUSED(status);


        //glXWaitX();

    #else
        #error "Need vogl_replay_window::resize this platform."
        return false;
    #endif

    m_width = new_width;
    m_height = new_height;
    return true;
}

void vogl_replay_window::close()
{
    VOGL_FUNC_TRACER
    #if (VOGL_PLATFORM_HAS_SDL)
        if (m_win)
        {
            SDL_DestroyWindow(m_win);
            m_win = NULL;
        }
    #elif (VOGL_PLATFORM_HAS_GLX)
        if (m_win)
        {
            XDestroyWindow(m_dpy, m_win);
            m_win = (Window)NULL;
        }

        if (m_dpy)
        {
            XCloseDisplay(m_dpy);
            m_dpy = NULL;
        }
    #else
        #error "Need vogl_replay_window::close this platform"
    #endif

    m_width = 0;
    m_height = 0;
}

void vogl_replay_window::update_dimensions()
{
    VOGL_FUNC_TRACER

    uint32_t w, h;
    get_actual_dimensions(w, h);
    m_width = w;
    m_height = h;
}

bool vogl_replay_window::get_actual_dimensions(uint32_t &width, uint32_t &height) const
{
    VOGL_FUNC_TRACER

    #if (VOGL_PLATFORM_HAS_SDL)
        if (m_win == NULL)
            return false;

        int w = 0, h = 0;

        SDL_GetWindowSize(m_win, &w, &h);
        width = (uint32_t)w;
        height = (uint32_t)h;

        return true;

    #elif (VOGL_PLATFORM_HAS_GLX)
        Window root;
        int x, y;
        unsigned int border_width, depth;
        return (XGetGeometry(m_dpy, m_win, &root, &x, &y, &width, &height, &border_width, &depth) != False);
    #else
        #error "Need vogl_replay_window::get_actual_dimensions this platform"
    #endif
}

GLReplayContextType vogl_replay_window::create_context(GLReplayContextType replay_share_context, Bool direct)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        if (replay_share_context) 
        {
            VOGL_VERIFY(!"we do not yet support replay of shared context applications on SDL");
            return NULL;
        }

        vogl_debug_printf("SDL ignores certain parameters, e.g. direct: %d!\n", direct);
        // TODO: This could call create_context instead.
        return SDL_GL_CreateContext(get_sdlwindow());

    #elif (VOGL_PLATFORM_HAS_X11)
        XVisualInfo *pVisual_info = GL_ENTRYPOINT(glXGetVisualFromFBConfig)(m_dpy, m_pFB_configs[0]);
        return GL_ENTRYPOINT(glXCreateContext)(dpy, pVisual_info, replay_share_context, direct);
    #else
        #error "Need to implement vogl_replay_window::create_context for this platform"
        return NULL;
    #endif
}

GLReplayContextType vogl_replay_window::create_new_context(GLReplayContextType replay_share_context, int render_type, Bool direct)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        if (replay_share_context) 
        {
            VOGL_VERIFY(!"we do not yet support replay of shared context applications on SDL");
            return NULL;
        }

        vogl_debug_printf("SDL ignores certain parameters, e.g. render_type: %d and direct: %d!\n", render_type, direct);
        // TODO: This could call create_context instead.
        return SDL_GL_CreateContext(get_sdlwindow());

    #elif (VOGL_PLATFORM_HAS_X11)
        return GL_ENTRYPOINT(glXCreateNewContext)(m_dpy, m_pFB_configs[0], render_type, replay_share_context, direct);       
    #else
        #error "Need to implement vogl_replay_window::create_context for this platform"
        return NULL;
    #endif
}


GLReplayContextType vogl_replay_window::create_context_attrib(GLReplayContextType replay_share_context, Bool direct, const int * pAttrib_list)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        // TODO: This is actually a bit complicated, we need to decode the the attrib list into SDL attribs. 
        #pragma message("warning Need to decode pAttrib_list into SDL attribs")
        if (replay_share_context)
        {
            VOGL_VERIFY(!"we do not yet support replay of shared context applications on SDL");
            return NULL;
        }

        return SDL_GL_CreateContext(get_sdlwindow());
    #elif (VOGL_PLATFORM_HAS_X11)
        GL_ENTRYPOINT(glXCreateContextAttribsARB)(m_dpy, m_pFB_configs[0], replay_share_context, direct, pAttrib_list);
    #else
        #error "Need to implement vogl_replay_window::create_context_attrib for this platform"
    #endif
}


bool vogl_replay_window::make_current(GLReplayContextType context)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        return SDL_GL_MakeCurrent(m_win, context) >= 0;
    #elif (VOGL_PLATFORM_HAS_GLX)
        GLXDrawable drawable = context ? get_xwindow() : (GLXDrawable)NULL;
        return GL_ENTRYPOINT(glXMakeCurrent)(m_dpy, drawable, context);
    #else
        #error "Need vogl_replay_window::make_current this platform"
        return false;
    #endif
}

void vogl_replay_window::destroy_context(GLReplayContextType context)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        SDL_GL_DeleteContext(context);
    #elif (VOGL_PLATFORM_HAS_GLX)
        GL_ENTRYPOINT(glXDestroyContext)(m_dpy, context);
    #else
        #error "Need vogl_replay_window::destroy_context this platform"
        return -1;
    #endif
}

Bool vogl_replay_window::is_direct(GLReplayContextType replay_context)
{
    #if (VOGL_PLATFORM_HAS_SDL && VOGL_PLATFORM_HAS_GLX)
        return GL_ENTRYPOINT(glXIsDirect)(dpy, replay_context);
    #elif (VOGL_PLATFORM_HAS_SDL)
        // We don't have a good way to ask here, so just say yes.
        return true ? 1 : 0;
    #else
        #error "Need vogl_replay_window::is_direct this platform (should probably just return true)"
        return -1;
    #endif
}

void vogl_replay_window::swap_buffers()
{
    #if (VOGL_PLATFORM_HAS_SDL)
        SDL_GL_SwapWindow(m_win);
    #elif (VOGL_PLATFORM_HAS_GLX)
        GL_ENTRYPOINT(glXSwapBuffers)(m_dpy, m_win);
    #else
        #error "Need vogl_replay_window::swap_buffers this platform"
        return -1;
    #endif
}


Bool vogl_replay_window::query_version(int *out_major, int *out_minor)
{
    VOGL_ASSERT(out_major != NULL);
    VOGL_ASSERT(out_minor != NULL);

    #if (VOGL_PLATFORM_HAS_GLX)
        return GL_ENTRYPOINT(glXQueryVersion)(out_major, out_minor);
    #else
        // Other platforms don't have this concept.
        (*out_major) = 0;
        (*out_minor) = 0;
        return 0;
    #endif

}


bool vogl_replay_window::check_glx_version()
{
    VOGL_FUNC_TRACER

    GLint nMajorVer = 0;
    VOGL_NOTE_UNUSED(nMajorVer);
    GLint nMinorVer = 0;
    VOGL_NOTE_UNUSED(nMinorVer);

    if ((!GL_ENTRYPOINT(glXQueryVersion)) || (!GL_ENTRYPOINT(glXChooseFBConfig)) || (!GL_ENTRYPOINT(glXGetVisualFromFBConfig)))
    {
        vogl_debug_printf("Failed checking GLX version!\n");
        return false;
    }

    // We can't actually check here. This function should be removed.

    return true;
}

