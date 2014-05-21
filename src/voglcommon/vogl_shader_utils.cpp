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

// File: vogl_shader_utils.cpp
#include "vogl_shader_utils.h"

GLuint vogl_create_program(const char *pVertex_shader_source, const char *pFragment_shader_source)
{
    GLuint program = 0, vert_shader = 0, frag_shader = 0;
    GLint status = 0;
    GLchar msg[4096];

    vert_shader = GL_ENTRYPOINT(glCreateShader)(GL_VERTEX_SHADER);
    VOGL_CHECK_GL_ERROR;

    frag_shader = GL_ENTRYPOINT(glCreateShader)(GL_FRAGMENT_SHADER);
    VOGL_CHECK_GL_ERROR;

    if ((!vert_shader) || (!frag_shader))
        goto failure;

    // Compile vertex shader
    GL_ENTRYPOINT(glShaderSource)(vert_shader, 1, (GLchar * const*)(&pVertex_shader_source), NULL);
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glCompileShader)(vert_shader);
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glGetShaderiv)(vert_shader, GL_COMPILE_STATUS, &status);
    if ((vogl_check_gl_error()) || (status == GL_FALSE))
    {
        GL_ENTRYPOINT(glGetShaderInfoLog)(vert_shader, sizeof(msg), NULL, msg);
        VOGL_CHECK_GL_ERROR;
        vogl_error_printf("Error compiling vertex shader:\n%s\n", msg);

        goto failure;
    }

    // Compile fragment shader
    GL_ENTRYPOINT(glShaderSource)(frag_shader, 1, (GLchar * const*)(&pFragment_shader_source), NULL);
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glCompileShader)(frag_shader);
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glGetShaderiv)(frag_shader, GL_COMPILE_STATUS, &status);
    if ((vogl_check_gl_error()) || (status == GL_FALSE))
    {
        GL_ENTRYPOINT(glGetShaderInfoLog)(frag_shader, sizeof(msg), NULL, msg);
        VOGL_CHECK_GL_ERROR;
        vogl_error_printf("Error compiling fragment shader:\n%s\n", msg);

        goto failure;
    }

    // Create and link program
    program = GL_ENTRYPOINT(glCreateProgram)();
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glAttachShader)(program, vert_shader);
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glAttachShader)(program, frag_shader);
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glLinkProgram)(program);
    if (vogl_check_gl_error())
        goto failure;

    GL_ENTRYPOINT(glDeleteShader)(vert_shader);
    VOGL_CHECK_GL_ERROR;
    vert_shader = 0;

    GL_ENTRYPOINT(glDeleteShader)(frag_shader);
    VOGL_CHECK_GL_ERROR;
    frag_shader = 0;

    GL_ENTRYPOINT(glGetProgramiv)(program, GL_LINK_STATUS, &status);
    if ((vogl_check_gl_error()) || (status == GL_FALSE))
    {
        GL_ENTRYPOINT(glGetProgramInfoLog)(program, sizeof(msg), NULL, msg);
        VOGL_CHECK_GL_ERROR;
        vogl_error_printf("Error linking program:\n%s\n", msg);

        goto failure;
    }

    return program;

failure:
    if (vert_shader)
    {
        GL_ENTRYPOINT(glDeleteShader)(vert_shader);
        VOGL_CHECK_GL_ERROR;
    }

    if (frag_shader)
    {
        GL_ENTRYPOINT(glDeleteShader)(frag_shader);
        VOGL_CHECK_GL_ERROR;
    }

    if (program)
    {
        GL_ENTRYPOINT(glDeleteProgram)(program);
        VOGL_CHECK_GL_ERROR;
    }

    vogl_error_printf("Failed creating program!\n");

    VOGL_CHECK_GL_ERROR;
    return 0;
}

vogl_simple_gl_program::vogl_simple_gl_program() :
    m_program(0)
{
}

vogl_simple_gl_program::~vogl_simple_gl_program()
{
    deinit();
}

void vogl_simple_gl_program::deinit()
{
    if (m_program)
    {
        GL_ENTRYPOINT(glDeleteProgram)(m_program);
        VOGL_CHECK_GL_ERROR;

        m_program = 0;
    }
}

bool vogl_simple_gl_program::init(const char *pVertex_shader, const char *pFragment_shader)
{
    deinit();

    m_program = vogl_create_program(pVertex_shader, pFragment_shader);

    return m_program != 0;
}

GLint vogl_simple_gl_program::get_uniform_location(const char *pName) const
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return -1;
    }

    return GL_ENTRYPOINT(glGetUniformLocation)(m_program, reinterpret_cast<const GLchar *>(pName));
}

bool vogl_simple_gl_program::has_uniform(const char *pName) const
{
    return get_uniform_location(pName) != -1;
}

void vogl_simple_gl_program::set_uniform(const char *pName, int s)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform1i)(get_uniform_location(pName), s);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, uint32_t s)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform1ui)(get_uniform_location(pName), s);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, float s)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform1f)(get_uniform_location(pName), s);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const vec2I &v)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform2i)(get_uniform_location(pName), v[0], v[1]);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const vec2F &v)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform2f)(get_uniform_location(pName), v[0], v[1]);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const vec3I &v)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform3i)(get_uniform_location(pName), v[0], v[1], v[2]);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const vec3F &v)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform3f)(get_uniform_location(pName), v[0], v[1], v[2]);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const vec4I &v)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform4i)(get_uniform_location(pName), v[0], v[1], v[2], v[3]);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const vec4F &v)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniform4f)(get_uniform_location(pName), v[0], v[1], v[2], v[3]);
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const matrix33F &m, bool transpose)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniformMatrix3fv)(get_uniform_location(pName), 1, transpose, m.get_ptr());
    VOGL_CHECK_GL_ERROR;
}

void vogl_simple_gl_program::set_uniform(const char *pName, const matrix44F &m, bool transpose)
{
    if (!m_program)
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    vogl_scoped_program_binder prog_binder(m_program);

    GL_ENTRYPOINT(glUniformMatrix4fv)(get_uniform_location(pName), 1, transpose, m.get_ptr());
    VOGL_CHECK_GL_ERROR;
}

