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

// File: vogl_replay_window.h
#ifndef VOGL_REPLAY_WINDOW_H
#define VOGL_REPLAY_WINDOW_H

#include "vogl_common.h"

#if VOGL_PLATFORM_HAS_SDL
#define SDL_MAIN_HANDLED 1
#include "SDL.h"
#endif

#if (VOGL_PLATFORM_HAS_SDL)
    typedef SDL_GLContext GLReplayContextType;
#else
    #error "Need to define a suitable type for GLReplayContextType for this platform"
#endif


//----------------------------------------------------------------------------------------------------------------------
// class vogl_replay_window
//----------------------------------------------------------------------------------------------------------------------
class vogl_replay_window
{
public:
    vogl_replay_window();

    ~vogl_replay_window();


    bool is_opened() const
    {
        #if (VOGL_PLATFORM_HAS_SDL)
            return (m_width > 0) && (m_win != NULL);
        #else
            #error "Need is_opened for this platform."
        #endif
    }

    bool open(int width, int height, int samples = 1);

    void set_title(const char *pTitle);

    bool resize(int new_width, int new_height);

    void close();


    #if VOGL_PLATFORM_HAS_SDL
        inline SDL_Window* get_sdlwindow() const
        {
            return m_win;
        }
    #else
        #error "Need get_window for this platform."
    #endif

    inline int get_width() const
    {
        return m_width;
    }

    inline int get_height() const
    {
        return m_height;
    }

    void update_dimensions();

    bool get_actual_dimensions(uint32_t &width, uint32_t &height) const;

    // Create a simple context for this window (using glXCreateContext)
    GLReplayContextType create_context(GLReplayContextType replay_share_context, Bool direct);

    // Create a context using glXCreateNewContext, if available.
    GLReplayContextType create_new_context(GLReplayContextType replay_share_context, int render_type, Bool direct);

    // Create a context with CreateContextAttribs on this platform
    GLReplayContextType create_context_attrib(GLReplayContextType replay_share_context, Bool direct, const int *pAttrib_list);

    // Make the replay context current on this window
    bool make_current(GLReplayContextType context);

    // Destroy this context, which should be associated with this window.
    void destroy_context(GLReplayContextType context);

    Bool query_version(int *out_major, int *out_minor);

    Bool is_direct(GLReplayContextType replay_context);

    void swap_buffers();

private:
#if VOGL_PLATFORM_HAS_SDL
    SDL_Window* m_win;
#endif

    int m_width;
    int m_height;


    bool check_glx_version();
};

#endif // VOGL_REPLAY_WINDOW_H
