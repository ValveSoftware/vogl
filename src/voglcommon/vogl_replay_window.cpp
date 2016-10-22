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

    #else
        #error "Need vogl_replay_window::open for this platform"
        return false;
    #endif

    m_width = width;
    m_height = height;

    uint32_t actual_width = 0, actual_height = 0;
    vogl_replay_window::get_actual_dimensions(actual_width, actual_height);
    vogl_debug_printf("Created window, requested dimensions %ux%u, actual dimensions %ux%u\n", m_width, m_height, actual_width, actual_height);

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
    #else
        #error "Need to implement vogl_replay_window::create_context for this platform"
        return NULL;
    #endif
}


GLReplayContextType vogl_replay_window::create_context_attrib(GLReplayContextType replay_share_context, Bool direct, const int * pAttrib_list)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        while (*pAttrib_list) {
            int key = *pAttrib_list++;
            int value = *pAttrib_list++;

            switch (key) {
                case GLX_CONTEXT_MAJOR_VERSION_ARB:
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, value);
                    break;
                case GLX_CONTEXT_MINOR_VERSION_ARB:
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, value);
                    break;
                case GLX_CONTEXT_PROFILE_MASK_ARB: {
                    int mask = 0;
                    switch (value) {
                        case GLX_CONTEXT_CORE_PROFILE_BIT_ARB:
                            mask = SDL_GL_CONTEXT_PROFILE_CORE;
                            break;
                        case GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB:
                            mask = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
                            break;
                        case GLX_CONTEXT_ES_PROFILE_BIT_EXT:
                        //case GLX_CONTEXT_ES2_PROFILE_BIT_EXT:
                            mask = SDL_GL_CONTEXT_PROFILE_ES;
                            break;
                        default:
                            vogl_warning_printf("Unknown profile mask value: %d", value);
                            break;
                    }
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, mask);
                    break;
                }
                case GLX_CONTEXT_FLAGS_ARB: {
                    int flags = 0;
                    if (value & GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB)
                        flags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
                    if (value & GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB)
                        flags |= SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG;
                    if (value & GLX_CONTEXT_RESET_ISOLATION_BIT_ARB)
                        flags |= SDL_GL_CONTEXT_RESET_ISOLATION_FLAG;
                    if (value & GLX_CONTEXT_DEBUG_BIT_ARB)
                        flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, flags);
                    break;
                }
                default:
                    vogl_warning_printf("Unknown context attribute: %d = %d", key, value);
                    break;
            }
        }

        if (replay_share_context)
        {
            VOGL_VERIFY(!"we do not yet support replay of shared context applications on SDL");
            return NULL;
        }

        return SDL_GL_CreateContext(get_sdlwindow());
    #else
        #error "Need to implement vogl_replay_window::create_context_attrib for this platform"
        return NULL;
    #endif
}


bool vogl_replay_window::make_current(GLReplayContextType context)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        return SDL_GL_MakeCurrent(m_win, context) >= 0;
    #else
        #error "Need vogl_replay_window::make_current this platform"
        return false;
    #endif
}

void vogl_replay_window::destroy_context(GLReplayContextType context)
{
    #if (VOGL_PLATFORM_HAS_SDL)
        SDL_GL_DeleteContext(context);
    #else
        #error "Need vogl_replay_window::destroy_context this platform"
        return -1;
    #endif
}

Bool vogl_replay_window::is_direct(GLReplayContextType replay_context)
{
    #if (VOGL_PLATFORM_HAS_SDL)
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
    #else
        #error "Need vogl_replay_window::swap_buffers this platform"
        return -1;
    #endif
}


Bool vogl_replay_window::query_version(int *out_major, int *out_minor)
{
    VOGL_ASSERT(out_major != NULL);
    VOGL_ASSERT(out_minor != NULL);

    // Platforms other than GLX don't have this concept.
    (*out_major) = 0;
    (*out_minor) = 0;
    return 0;
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

