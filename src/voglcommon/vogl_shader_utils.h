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

// File: vogl_shader_utils.h
#ifndef VOGL_SHADER_UTILS_H
#define VOGL_SHADER_UTILS_H

#include "vogl_core.h"
#include "vogl_common.h"
#include "vogl_matrix.h"

GLuint vogl_create_program(const char *pVertex_shader, const char *pFragment_shader);

class vogl_scoped_program_binder
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_scoped_program_binder);

public:
    vogl_scoped_program_binder(GLuint new_prog) :
        m_prev_program(0)
    {
        GL_ENTRYPOINT(glGetIntegerv)(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&m_prev_program));
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glUseProgram)(new_prog);
        VOGL_CHECK_GL_ERROR;
    }

    ~vogl_scoped_program_binder()
    {
        GL_ENTRYPOINT(glUseProgram)(m_prev_program);
        VOGL_CHECK_GL_ERROR;
    }

private:
    GLuint m_prev_program;
};

class vogl_simple_gl_program
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_simple_gl_program);

public:
    vogl_simple_gl_program();
    vogl_simple_gl_program(const char *pVertex_shader, const char *pFragment_shader);
    ~vogl_simple_gl_program();

    bool init(const char *pVertex_shader, const char *pFragment_shader);
    void deinit();

    bool is_valid() const { return m_program != 0; }

    GLuint get_handle() const { return m_program; }

    GLint get_uniform_location(const char *pName) const;
    bool has_uniform(const char *pName) const;

    void set_uniform(const char *pName, int s);
    void set_uniform(const char *pName, uint32_t s);
    void set_uniform(const char *pName, float s);

    void set_uniform(const char *pName, const vec2I &v);
    void set_uniform(const char *pName, const vec2F &v);

    void set_uniform(const char *pName, const vec3I &v);
    void set_uniform(const char *pName, const vec3F &v);

    void set_uniform(const char *pName, const vec4I &v);
    void set_uniform(const char *pName, const vec4F &v);

    void set_uniform(const char *pName, const matrix33F &m, bool transpose);
    void set_uniform(const char *pName, const matrix44F &m, bool transpose);

private:
    GLuint m_program;
};

#endif // VOGL_SHADER_UTILS_H
