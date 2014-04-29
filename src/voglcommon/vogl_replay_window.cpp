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
    : m_dpy(NULL),
      m_win((Window)NULL),
      m_width(0),
      m_height(0),
      m_pFB_configs(NULL),
      m_num_fb_configs(0)
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
    #if (VOGL_PLATFORM_HAS_GLX)

        close();

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
            console::error("%s: XOpenDisplay() failed!\n", VOGL_METHOD_NAME);
            return false;
        }

        // Get a new fb config that meets our attrib requirements

        m_pFB_configs = GL_ENTRYPOINT(glXChooseFBConfig)(m_dpy, DefaultScreen(m_dpy), fbAttribs, &m_num_fb_configs);
        if ((!m_pFB_configs) || (!m_num_fb_configs))
        {
            console::error("%s: glXChooseFBConfig() failed!\n", VOGL_METHOD_NAME);
            return false;
        }

        XVisualInfo *pVisual_info = GL_ENTRYPOINT(glXGetVisualFromFBConfig)(m_dpy, m_pFB_configs[0]);
        if (!pVisual_info)
        {
            console::error("%s: glXGetVisualFromFBConfig() failed!\n", VOGL_METHOD_NAME);
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

        const char *pWindow_name = (sizeof(void *) == sizeof(uint32)) ? "voglreplay 32-bit" : "voglreplay 64-bit";
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

        m_width = width;
        m_height = height;

        uint actual_width = 0, actual_height = 0;
        vogl_replay_window::get_actual_dimensions(actual_width, actual_height);
        vogl_debug_printf("%s: Created window, requested dimensions %ux%u, actual dimensions %ux%u\n", VOGL_METHOD_NAME, m_width, m_height, actual_width, actual_height);

        return true;
    #else
        VOGL_ASSERT(!"impl");
        return false;
    #endif
}

void vogl_replay_window::set_title(const char *pTitle)
{
    VOGL_FUNC_TRACER
    #if (VOGL_PLATFORM_HAS_GLX)
        if (m_win)
        {
            XStoreName(m_dpy, m_win, pTitle);
        }
    #else
        VOGL_ASSERT(!"impl");
    #endif
}

bool vogl_replay_window::resize(int new_width, int new_height)
{
    VOGL_FUNC_TRACER

    #if (VOGL_PLATFORM_HAS_GLX)

        if (!is_opened())
            return open(new_width, new_height);

        if ((new_width == m_width) && (new_height == m_height))
            return true;

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

        m_width = new_width;
        m_height = new_height;

        //glXWaitX();

        return true;
    #else
        VOGL_ASSERT(!"impl");
        return false;
    #endif
}

void vogl_replay_window::clear_window()
{
    VOGL_FUNC_TRACER
    #if (VOGL_PLATFORM_HAS_GLX)

        if (!m_dpy)
            return;

        XFillRectangle(m_dpy, m_win, DefaultGC(m_dpy, DefaultScreen(m_dpy)), 0, 0, m_width, m_height);
    #else
        VOGL_ASSERT(!"impl");
    #endif
}

void vogl_replay_window::close()
{
    VOGL_FUNC_TRACER
    #if (VOGL_PLATFORM_HAS_GLX)

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

        m_width = 0;
        m_height = 0;
    #else
        VOGL_ASSERT(!"impl");
    #endif
}

void vogl_replay_window::update_dimensions()
{
    VOGL_FUNC_TRACER

    //XWindowAttributes winData;
    //XGetWindowAttributes(m_dpy, m_win, &winData);

    //m_width = winData.width;
    //m_height = winData.height;
    uint w, h;
    get_actual_dimensions(w, h);
    m_width = w;
    m_height = h;
}

bool vogl_replay_window::get_actual_dimensions(uint &width, uint &height) const
{
    VOGL_FUNC_TRACER
    #if (VOGL_PLATFORM_HAS_GLX)
        Window root;
        int x, y;
        unsigned int border_width, depth;
        return (XGetGeometry(m_dpy, m_win, &root, &x, &y, &width, &height, &border_width, &depth) != False);
    #else
        return false;
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

#if 0
	// This always returns 0, don't know why yet.
	ACTUAL_GL_ENTRYPOINT(glXQueryVersion)(m_dpy, &nMajorVer, &nMinorVer);

	vogl_debug_printf("Supported GLX version - %d.%d\n", nMajorVer, nMinorVer);

	if (nMajorVer == 1 && nMinorVer < 2)
	{
		vogl_error_printf("GLX 1.2 or greater is necessary\n");
		XCloseDisplay(m_dpy);
		return false;
	}
#endif

    return true;
}
