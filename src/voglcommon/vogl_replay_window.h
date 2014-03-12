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
        return (m_width > 0) && (m_dpy != NULL);
    }

    bool open(int width, int height);

    void set_title(const char *pTitle);

    bool resize(int new_width, int new_height);

    void clear_window();

    void close();

    inline Display *get_display() const
    {
        return m_dpy;
    }
    inline Window get_xwindow() const
    {
        return m_win;
    }
    inline int get_width() const
    {
        return m_width;
    }
    inline int get_height() const
    {
        return m_height;
    }

    void update_dimensions();

    GLXFBConfig *get_fb_configs() const
    {
        return m_pFB_configs;
    }
    uint get_num_fb_configs() const
    {
        return m_num_fb_configs;
    }

    bool get_actual_dimensions(uint &width, uint &height) const;

private:
    Display *m_dpy;
    Window m_win;
    int m_width;
    int m_height;

    GLXFBConfig *m_pFB_configs;
    int m_num_fb_configs;

    bool check_glx_version();
};

#endif // VOGL_REPLAY_WINDOW_H
