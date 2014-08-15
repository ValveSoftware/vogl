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

// File: vogl_fs_preprocessor.h
#ifndef VOGL_FS_PREPROCESSOR_H
#define VOGL_FS_PREPROCESSOR_H

#ifndef VOGL_FS_PREPROCESSOR
#define VOGL_FS_PREPROCESSOR
#endif

#include "vogl_common.h"
#include "vogl_dynamic_string.h"

#include "vogl_common.h"

class vogl_fs_preprocessor
{
public:
    static vogl_fs_preprocessor* get_instance()
    {
        if (!m_instance)
            m_instance = new vogl_fs_preprocessor;

        return m_instance;
    }

    void reset();
    bool run(); // execute pp w/ current cmd, opts & shader
    void set_shader(GLuint id, vogl::vector<const GLcharARB *> in_shader_vec, vogl::vector<GLint> lengths); // We'll internally fold this into a single string
    void set_shader(const GLchar* in_shader_str, int len)
    {
        m_in_shader.set(in_shader_str, len);
        m_in_length = len;
    }
    void set_shader(GLuint id, const GLchar* in_shader_str, int len)
    {
        m_shader_id = id;
        m_in_shader.set(in_shader_str, len);
        m_in_length = len;
    }
    void set_pp(const dynamic_string &pp)
    {
        if (0 != pp.get_len())
            m_enabled = true;
        m_pp_cmd = pp;
    }
    void set_pp_opts(const dynamic_string &pp_opts)
    {
        m_pp_opts = _check_opts(pp_opts);
    }
    void set_pp_prefix(const dynamic_string &pp_prefix)
    {
        m_pp_prefix = pp_prefix;
    }
    GLsizei get_count() const
    {
        return m_count;        
    }
    GLint get_length() const
    {
        return m_length;
    }
    const GLchar* get_output_shader() const
    {
        return reinterpret_cast<const GLchar *>(m_output_shader.get_ptr());
    }
    bool enabled() const
    {
        return m_enabled;
    }
    double get_pp_time() const
    {
        return m_time_to_run_pp;
    }
    void set_null(bool null_value)
    {
        m_null = null_value;
    }
    // Trace ID for a shader.  If NULL shader set and shader_id set to non-zero,
    //   PP will only run for specified shaderID, which will be NULL'd out.
    void set_shader_id(GLuint id)
    {
        m_shader_id = id;
    }
    void enable_null()
    {
        m_null = true;
    }
    void enable_null(float* null_color)
    {
        m_null = true;
        m_null_color[0] = null_color[0];
        m_null_color[1] = null_color[1];
        m_null_color[2] = null_color[2];
        m_null_color[3] = null_color[3];
    }

private:
    static void cleanup();
    vogl_fs_preprocessor();
    ~vogl_fs_preprocessor();
    vogl_fs_preprocessor(const vogl_fs_preprocessor&);
    vogl_fs_preprocessor& operator=(const vogl_fs_preprocessor&);
    bool _run();
    dynamic_string _check_opts(const dynamic_string &in_opts); // set internal state
    dynamic_string _set_null_output_color(dynamic_string in_shader);

    static vogl_fs_preprocessor* m_instance;
    
    dynamic_string m_pp_cmd;
    dynamic_string m_pp_opts;
    dynamic_string m_pp_prefix;
    GLsizei m_count;
    GLint m_length;
    GLint m_in_length;
    GLuint m_shader_id;
    GLuint m_null_shader_id;
    dynamic_string m_in_shader;
    dynamic_string m_output_shader;
    bool m_null;
    float m_null_color[4];
    timer m_tm;
    double m_time_to_run_pp;
    bool m_enabled; // is pp enabled?
};

#endif // VOGL_FS_PREPROCESSOR_H
