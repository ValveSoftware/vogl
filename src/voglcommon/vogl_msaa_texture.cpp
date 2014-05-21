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

// File: vogl_msaa_texture.cpp
#include "vogl_msaa_texture.h"
#include "vogl_vec.h"
#include "vogl_texture_format.h"

// Requires GL 3.2, GL_ARB_explicit_attrib_location, GL_ARB_separate_shader_objects, GL_ARB_sample_shading

// Passthrough vertex shader

// Requires GL 3.2, GL_ARB_explicit_attrib_location, GL_ARB_separate_shader_objects
static const char *g_passthrough_vert_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "layout(location = 0) in vec4 vVertex;\n"
    "layout(location = 1) in vec3 vTexCoord0;\n"
    "layout(location = 0) out vec3 vTex;\n"
    "void main(void)\n"
    "{ vTex = vTexCoord0;\n"
    "gl_Position = vVertex;\n"
    "}";

// Reading fragment shaders

static const char *g_read_color_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2DMS tex;\n"
    "uniform int tex_sample;\n"
    "void main(void) { fragColor0 = texelFetch(tex, ivec2(int(vTex0.x), int(vTex0.y)), tex_sample); }";

static const char *g_read_color_array_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2DMSArray tex;\n"
    "uniform int tex_sample;\n"
    "void main(void) { fragColor0 = texelFetch(tex, ivec3(int(vTex0.x), int(vTex0.y), int(vTex0.z)), tex_sample); }";

static const char *g_read_depth_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2DMS tex;\n"
    "uniform int tex_sample;\n"
    "void main(void) { fragColor0 = vec4(0,0,0,0); gl_FragDepth = texelFetch(tex, ivec2(int(vTex0.x), int(vTex0.y)), tex_sample).x; }";

static const char *g_read_depth_array_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2DMSArray tex;\n"
    "uniform int tex_sample;\n"
    "void main(void) { fragColor0 = vec4(0,0,0,1); gl_FragDepth = texelFetch(tex, ivec3(int(vTex0.x), int(vTex0.y), int(vTex0.z)), tex_sample).x; }";

// Writing fragment shaders

static const char *g_write_color_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "#extension GL_ARB_sample_shading : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2D tex;\n"
    "uniform int sample_mask;\n"
    "void main(void)\n"
    "{\n"
    "fragColor0 = texelFetch(tex, ivec2(int(vTex0.x), int(vTex0.y)), 0);\n"
    "gl_SampleMask[0] = sample_mask;\n"
    "}";

static const char *g_write_color_array_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "#extension GL_ARB_sample_shading : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2DArray tex;\n"
    "uniform int sample_mask;\n"
    "void main(void)\n"
    "{\n"
    "fragColor0 = texelFetch(tex, ivec3(int(vTex0.x), int(vTex0.y), int(vTex0.z)), 0);\n"
    "gl_SampleMask[0] = sample_mask;\n"
    "}";

static const char *g_write_depth_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "#extension GL_ARB_sample_shading : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2D tex;\n"
    "uniform int sample_mask;\n"
    "void main(void)\n"
    "{\n"
    "fragColor0 = vec4(0,0,0,1);\n"
    "gl_FragDepth = texelFetch(tex, ivec2(int(vTex0.x), int(vTex0.y)), 0).x;\n"
    "gl_SampleMask[0] = sample_mask;\n"
    "}";

static const char *g_write_depth_array_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "#extension GL_ARB_sample_shading : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2DArray tex;\n"
    "uniform int sample_mask;\n"
    "void main(void)\n"
    "{\n"
    "fragColor0 = vec4(0,0,0,1);\n"
    "gl_FragDepth = texelFetch(tex, ivec3(int(vTex0.x), int(vTex0.y), int(vTex0.z)), 0).x;\n"
    "gl_SampleMask[0] = sample_mask;\n"
    "}";

static const char *g_const_color_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "#extension GL_ARB_sample_shading : enable\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform vec4 color;\n"
    "void main(void)\n"
    "{\n"
    "fragColor0 = color;\n"
    "}";

static const char *g_write_stencil_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "#extension GL_ARB_sample_shading : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2D tex;\n"
    "uniform float stencil_comp_val;\n"
    "uniform int sample_mask;\n"
    "void main(void)\n"
    "{\n"
    "float stencil_val = texelFetch(tex, ivec2(int(vTex0.x), int(vTex0.y)), 0).x;\n"
    "if (abs(stencil_val - stencil_comp_val) > 0.125/255.0) discard;\n"
    "fragColor0 = vec4(0,0,0,1);\n"
    "gl_SampleMask[0] = sample_mask;\n"
    "}";

static const char *g_write_stencil_array_frag_shader =
    "#version 150 core\n"
    "#extension GL_ARB_explicit_attrib_location : enable\n"
    "#extension GL_ARB_separate_shader_objects : enable\n"
    "#extension GL_ARB_sample_shading : enable\n"
    "layout(location = 0) in vec3 vTex0;\n"
    "layout(location = 0) out vec4 fragColor0;\n"
    "uniform sampler2DArray tex;\n"
    "uniform float stencil_comp_val;\n"
    "uniform int sample_mask;\n"
    "void main(void)\n"
    "{\n"
    "float stencil_val = texelFetch(tex, ivec3(int(vTex0.x), int(vTex0.y), int(vTex0.z)), 0).x;\n"
    "if (abs(stencil_val - stencil_comp_val) > 0.125/255.0) discard;\n"
    "fragColor0 = vec4(0,0,0,1);\n"
    "gl_SampleMask[0] = sample_mask;\n"
    "}";

struct vogl_quad_vertex
{
    vec4F m_pos;
    vec3F m_uv0;

    vogl_quad_vertex() { }

    vogl_quad_vertex(float x, float y, float z, float w, float u, float v, float q) :
        m_pos(x, y, z, w), m_uv0(u, v, q)
    {
    }
};

static const vogl_quad_vertex *get_quad_tri_verts()
{
    static const vogl_quad_vertex s_quad_tri_verts[3*2] =
    {
        vogl_quad_vertex(-1.0f,  1.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f ), // top left
        vogl_quad_vertex( 1.0f,  1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f ), // top right
        vogl_quad_vertex( 1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f ), // bottom right

        vogl_quad_vertex(-1.0f,  1.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f ), // top left
        vogl_quad_vertex( 1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f ), // bottom right
        vogl_quad_vertex(-1.0f, -1.0f, 0.0f, 1.0f,  0.0f, 0.0f, 0.0f )  // bottom left
    };
    return s_quad_tri_verts;
}

vogl_msaa_texture_splitter::vogl_msaa_texture_splitter() :
    m_vao_handle(0),
    m_vertex_buffer(0),
    m_orig_context(0),
    m_work_context(0),
    m_cur_display(0),
    m_cur_fb_config(0),
    m_cur_drawable(0),
    m_valid(false)
{
}

bool vogl_msaa_texture_splitter::init()
{
    deinit();

    // Create work context
    m_orig_context = vogl_get_current_context();
    m_cur_display = vogl_get_current_display();
    m_cur_fb_config = vogl_get_current_fb_config();
    m_cur_drawable = vogl_get_current_drawable();

    vogl_context_desc context_desc;
    m_work_context = vogl_create_context(m_cur_display, m_cur_fb_config, m_orig_context, 3, 2, eCHCDebugContextFlag | cCHCCoreProfileFlag, &context_desc);
    if (!m_work_context)
    {
        vogl_error_printf("Failed creating temporary context!\n");
        return false;
    }

    vogl_make_current(m_cur_display, m_cur_drawable, m_work_context);

    vogl_enable_generic_context_debug_messages();

    m_context_info.init(context_desc);

    GL_ENTRYPOINT(glGenVertexArrays)(1, &m_vao_handle);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindVertexArray)(m_vao_handle);
    VOGL_CHECK_GL_ERROR;

    // Create programs

    // Reading from MSAA
    if (!m_read_color_program.init(g_passthrough_vert_shader, g_read_color_frag_shader))
        return false;
    m_read_color_program.set_uniform("tex", 0);

    if (!m_read_color_array_program.init(g_passthrough_vert_shader, g_read_color_array_frag_shader))
        return false;
    m_read_color_array_program.set_uniform("tex", 0);

    if (!m_read_depth_program.init(g_passthrough_vert_shader, g_read_depth_frag_shader))
        return false;
    m_read_depth_program.set_uniform("tex", 0);

    if (!m_read_depth_array_program.init(g_passthrough_vert_shader, g_read_depth_array_frag_shader))
        return false;
    m_read_depth_array_program.set_uniform("tex", 0);

    // Writing to MSAA
    if (!m_write_color_program.init(g_passthrough_vert_shader, g_write_color_frag_shader))
        return false;
    m_write_color_program.set_uniform("tex", 0);

    if (!m_write_color_array_program.init(g_passthrough_vert_shader, g_write_color_array_frag_shader))
        return false;
    m_write_color_array_program.set_uniform("tex", 0);

    if (!m_write_depth_program.init(g_passthrough_vert_shader, g_write_depth_frag_shader))
        return false;
    m_write_depth_program.set_uniform("tex", 0);

    if (!m_write_depth_array_program.init(g_passthrough_vert_shader, g_write_depth_array_frag_shader))
        return false;
    m_write_depth_array_program.set_uniform("tex", 0);

    // Copying stencil to color
    if (!m_const_color_program.init(g_passthrough_vert_shader, g_const_color_frag_shader))
        return false;

    // Copying color to stencil
    if (!m_write_stencil_program.init(g_passthrough_vert_shader, g_write_stencil_frag_shader))
        return false;
    m_write_stencil_program.set_uniform("tex", 0);

    if (!m_write_stencil_array_program.init(g_passthrough_vert_shader, g_write_stencil_array_frag_shader))
        return false;
    m_write_stencil_array_program.set_uniform("tex", 0);

    // Create vertex buffer and bind it
    GL_ENTRYPOINT(glGenBuffers)(1, &m_vertex_buffer);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindBuffer)(GL_ARRAY_BUFFER, m_vertex_buffer);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glVertexAttribPointer)(0, 4, GL_FLOAT, GL_FALSE, sizeof(vogl_quad_vertex), (const GLvoid *)(uint64_t)VOGL_OFFSETOF(vogl_quad_vertex, m_pos));
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glVertexAttribPointer)(1, 3, GL_FLOAT, GL_FALSE, sizeof(vogl_quad_vertex), (const GLvoid *)(uint64_t)VOGL_OFFSETOF(vogl_quad_vertex, m_uv0));
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glEnableVertexAttribArray)(0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glEnableVertexAttribArray)(1);
    VOGL_CHECK_GL_ERROR;

    vogl_reset_pixel_store_states();

    // Set state (some of this is probably not necessary)
    GL_ENTRYPOINT(glDisable)(GL_BLEND);
    GL_ENTRYPOINT(glDisable)(GL_CULL_FACE);
    GL_ENTRYPOINT(glDisable)(GL_DEPTH_TEST);
    GL_ENTRYPOINT(glDisable)(GL_STENCIL_TEST);
    GL_ENTRYPOINT(glDepthMask)(GL_FALSE);
    GL_ENTRYPOINT(glStencilMask)(0);
    GL_ENTRYPOINT(glActiveTexture)(GL_TEXTURE0);
    VOGL_CHECK_GL_ERROR;

    m_valid = true;

    return true;
}

void vogl_msaa_texture_splitter::deinit()
{
    if (!m_work_context)
        return;

    // We're switching contexts so serialize here (not sure if this is necessary, best to be safe).
    GL_ENTRYPOINT(glFinish)();
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindVertexArray)(0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glUseProgram)(0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D_MULTISAMPLE, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindBuffer)(GL_ARRAY_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    if (m_vao_handle)
    {
        GL_ENTRYPOINT(glDeleteVertexArrays)(1, &m_vao_handle);
        VOGL_CHECK_GL_ERROR;

        m_vao_handle = 0;
    }

    if (m_vertex_buffer)
    {
        GL_ENTRYPOINT(glDeleteBuffers)(1, &m_vertex_buffer);
        VOGL_CHECK_GL_ERROR;

        m_vertex_buffer = 0;
    }

    m_read_color_program.deinit();
    m_read_color_array_program.deinit();
    m_read_depth_program.deinit();
    m_read_depth_array_program.deinit();

    m_write_color_program.deinit();
    m_write_color_array_program.deinit();
    m_write_depth_program.deinit();
    m_write_depth_array_program.deinit();

    m_const_color_program.deinit();

    m_write_stencil_program.deinit();
    m_write_stencil_array_program.deinit();

    vogl_make_current(m_cur_display, m_cur_drawable, m_orig_context);

    vogl_destroy_context(m_cur_display, m_work_context);
    m_work_context = 0;

    m_orig_context = 0;
    m_cur_display = 0;
    m_cur_fb_config = 0;
    m_cur_drawable = 0;

    m_valid = false;
}

vogl_msaa_texture_splitter::~vogl_msaa_texture_splitter()
{
    deinit();
}

// One multisampled source, multiple non-multisampled destinations created by method.
bool vogl_msaa_texture_splitter::split(GLenum src_target, GLuint src_texture, vogl::vector<GLuint> &dest_textures)
{
    dest_textures.resize(0);

    if (!m_valid)
        return false;

    if ((src_target != GL_TEXTURE_2D_MULTISAMPLE) && (src_target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
        return false;

    GLenum dest_target = (src_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

    GL_ENTRYPOINT(glBindTexture)(src_target, src_texture);
    VOGL_CHECK_GL_ERROR;

    GLint mip_level = 0, base_level = 0, max_level = 0;

    GLenum internal_fmt = 0;
    int base_width = 0, base_height = 0, base_depth = 0, samples = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, base_level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_fmt));
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, base_level, GL_TEXTURE_WIDTH, &base_width);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, base_level, GL_TEXTURE_HEIGHT, &base_height);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, base_level, GL_TEXTURE_DEPTH, &base_depth);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, base_level, GL_TEXTURE_SAMPLES, &samples);
    VOGL_CHECK_GL_ERROR;

    if ((base_width < 1) || (base_height < 1) || (base_depth < 1) || (samples < 1))
        return false;

    const vogl_internal_tex_format *pInternal_tex_fmt = vogl_find_internal_texture_format(internal_fmt);
    if ((!pInternal_tex_fmt) || (pInternal_tex_fmt->m_optimum_get_image_fmt == GL_NONE) || (pInternal_tex_fmt->m_optimum_get_image_type == GL_NONE))
        return false;

    GLenum attachment_target = GL_COLOR_ATTACHMENT0;

    if ((pInternal_tex_fmt->m_comp_sizes[cTCDepth]) && (pInternal_tex_fmt->m_comp_sizes[cTCStencil]))
        attachment_target = GL_DEPTH_STENCIL_ATTACHMENT;
    else if (pInternal_tex_fmt->m_comp_sizes[cTCDepth])
        attachment_target = GL_DEPTH_ATTACHMENT;
    else if (pInternal_tex_fmt->m_comp_sizes[cTCStencil])
        return false;

    if (attachment_target == GL_COLOR_ATTACHMENT0)
    {
        GL_ENTRYPOINT(glDepthMask)(GL_FALSE);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDisable)(GL_DEPTH_TEST);
        VOGL_CHECK_GL_ERROR;
    }
    else
    {
        GL_ENTRYPOINT(glDepthMask)(GL_TRUE);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glEnable)(GL_DEPTH_TEST);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDepthFunc)(GL_ALWAYS);
        VOGL_CHECK_GL_ERROR;
    }

    GL_ENTRYPOINT(glDisable)(GL_STENCIL_TEST);
    VOGL_CHECK_GL_ERROR;

    uint32_t width = base_width << base_level;
    uint32_t height = base_height << base_level;
    uint32_t depth = base_depth;

    uint32_t max_possible_mip_levels = utils::compute_max_mips(width, height, depth);
    int num_actual_mip_levels = math::minimum<int>(max_possible_mip_levels, max_level + 1);

    for (int sample_index = 0; sample_index < samples; sample_index++)
    {
        m_read_color_program.set_uniform("tex_sample", sample_index);
        m_read_color_array_program.set_uniform("tex_sample", sample_index);
        m_read_depth_program.set_uniform("tex_sample", sample_index);
        m_read_depth_array_program.set_uniform("tex_sample", sample_index);

        GLuint dest_texture = 0;
        GL_ENTRYPOINT(glGenTextures)(1, &dest_texture);
        VOGL_CHECK_GL_ERROR;

        dest_textures.push_back(dest_texture);

        GL_ENTRYPOINT(glBindTexture)(dest_target, dest_texture);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glTexParameteri)(dest_target, GL_TEXTURE_BASE_LEVEL, base_level);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glTexParameteri)(dest_target, GL_TEXTURE_MAX_LEVEL, num_actual_mip_levels - 1);
        VOGL_CHECK_GL_ERROR;

        int mip_width = 0, mip_height = 0, mip_depth = 0;
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, mip_level, GL_TEXTURE_WIDTH, &mip_width);
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, mip_level, GL_TEXTURE_HEIGHT, &mip_height);
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, mip_level, GL_TEXTURE_DEPTH, &mip_depth);
        if ((vogl_check_gl_error()) || (mip_width < 1) || (mip_height < 1) || (mip_depth < 1))
            goto failure;

        VOGL_ASSERT(mip_depth == base_depth);
        VOGL_NOTE_UNUSED(mip_depth);

        vogl::vector<uint8_t> ones;
        ones.resize(static_cast<uint32_t>(vogl_get_image_size(pInternal_tex_fmt->m_optimum_get_image_fmt, pInternal_tex_fmt->m_optimum_get_image_type, mip_width, mip_height, mip_depth)));
        ones.set_all(255);

        if (src_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
            GL_ENTRYPOINT(glTexImage3D)(dest_target,
                mip_level,
                internal_fmt,
                mip_width,
                mip_height,
                base_depth,
                0,
                pInternal_tex_fmt->m_optimum_get_image_fmt,
                pInternal_tex_fmt->m_optimum_get_image_type,
                ones.get_ptr());
            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            GL_ENTRYPOINT(glTexImage2D)(dest_target,
                mip_level,
                internal_fmt,
                mip_width,
                mip_height,
                0,
                pInternal_tex_fmt->m_optimum_get_image_fmt,
                pInternal_tex_fmt->m_optimum_get_image_type,
                ones.get_ptr());
            VOGL_CHECK_GL_ERROR;
        }

        for (int layer = 0; layer < base_depth; layer++)
        {
            // Create FBO
            GLuint fbo_handle = 0;
            GL_ENTRYPOINT(glGenFramebuffers)(1, &fbo_handle);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, fbo_handle);
            VOGL_CHECK_GL_ERROR;

            if (dest_target == GL_TEXTURE_2D_ARRAY)
            {
                GL_ENTRYPOINT(glFramebufferTextureLayer)(GL_DRAW_FRAMEBUFFER, attachment_target, dest_texture, mip_level, layer);
                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, attachment_target, dest_target, dest_texture, mip_level);
                VOGL_CHECK_GL_ERROR;
            }

            if (attachment_target == GL_COLOR_ATTACHMENT0)
            {
                GLenum draw_buf = GL_COLOR_ATTACHMENT0;
                GL_ENTRYPOINT(glDrawBuffers)(1, &draw_buf);
                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                GL_ENTRYPOINT(glDrawBuffer)(GL_NONE);
                VOGL_CHECK_GL_ERROR;
            }

            GLenum fbo_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
            VOGL_CHECK_GL_ERROR;

            bool status = false;

            if (fbo_status == GL_FRAMEBUFFER_COMPLETE)
            {
                // This method reads from a multisampled texture, which does NOT support GL_SKIP_DECODE_EXT on these targets.
                // Instead, we enable sRGB writes to work around this. The source and dest use the same formats, and if the dest is not sRGB then the source shouldn't be.
                GL_ENTRYPOINT(glDisable)(GL_FRAMEBUFFER_SRGB);
                VOGL_CHECK_GL_ERROR;

                if (attachment_target == GL_COLOR_ATTACHMENT0)
                {
                    GLint encoding = 0;
                    GL_ENTRYPOINT(glGetFramebufferAttachmentParameteriv)(GL_DRAW_FRAMEBUFFER, attachment_target, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
                    VOGL_CHECK_GL_ERROR;

                    if (encoding == GL_SRGB)
                    {
                        GL_ENTRYPOINT(glEnable)(GL_FRAMEBUFFER_SRGB);
                        VOGL_CHECK_GL_ERROR;
                    }
                }

                // Render quad to FBO
                GL_ENTRYPOINT(glViewport)(0, 0, mip_width, mip_height);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glClearColor)(1.0f, 0.3f, 1.0f, 1.0f);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glClearDepth)(.25f);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glClearStencil)(64);
                VOGL_CHECK_GL_ERROR;

                if (attachment_target == GL_COLOR_ATTACHMENT0)
                {
                    GL_ENTRYPOINT(glClear)(GL_COLOR_BUFFER_BIT);
                    VOGL_CHECK_GL_ERROR;
                }
                else if (attachment_target == GL_DEPTH_ATTACHMENT)
                {
                    GL_ENTRYPOINT(glClear)(GL_DEPTH_BUFFER_BIT);
                    VOGL_CHECK_GL_ERROR;
                }
                else
                {
                    GL_ENTRYPOINT(glClear)(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                    VOGL_CHECK_GL_ERROR;
                }

                vogl_quad_vertex quad_tri_verts[3*2];
                memcpy(quad_tri_verts, get_quad_tri_verts(), sizeof(quad_tri_verts));

                for (uint32_t i = 0; i < 6; i++)
                {
                    quad_tri_verts[i].m_uv0[0] *= mip_width;
                    quad_tri_verts[i].m_uv0[1] *= mip_height;
                    quad_tri_verts[i].m_uv0[2] = 1.0f * layer;
                }

                GL_ENTRYPOINT(glBufferData)(GL_ARRAY_BUFFER, sizeof(quad_tri_verts), quad_tri_verts, GL_STATIC_DRAW);
                VOGL_CHECK_GL_ERROR;

                if (attachment_target == GL_COLOR_ATTACHMENT0)
                {
                    GL_ENTRYPOINT(glUseProgram)((src_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? m_read_color_array_program.get_handle() : m_read_color_program.get_handle());
                    VOGL_CHECK_GL_ERROR;
                }
                else
                {
                    GL_ENTRYPOINT(glUseProgram)((src_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? m_read_depth_array_program.get_handle() : m_read_depth_program.get_handle());
                    VOGL_CHECK_GL_ERROR;
                }

                GL_ENTRYPOINT(glDrawArrays)(GL_TRIANGLES, 0, 6);
                VOGL_CHECK_GL_ERROR;

                status = true;
            } // fbo_status

            // Delete FBO
            GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDeleteFramebuffers)(1, &fbo_handle);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDrawBuffer)(GL_FRONT_LEFT);
            VOGL_CHECK_GL_ERROR;

            if (!status)
                goto failure;
        } // layer
    } // sample_index

    GL_ENTRYPOINT(glBindTexture)(src_target, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(dest_target, 0);
    VOGL_CHECK_GL_ERROR;

    return true;

failure:
    GL_ENTRYPOINT(glBindTexture)(src_target, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(dest_target, 0);
    VOGL_CHECK_GL_ERROR;

    if (dest_textures.size())
    {
        GL_ENTRYPOINT(glDeleteTextures)(dest_textures.size(), dest_textures.get_ptr());
        VOGL_CHECK_GL_ERROR;

        dest_textures.resize(0);
    }

    return false;
}

// Multiple sources, one multisampled destination. Caller creates destination.
bool vogl_msaa_texture_splitter::combine(GLuint src_texture, uint32_t dest_sample_index, GLenum dest_target, GLuint dest_texture)
{
    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((dest_target != GL_TEXTURE_2D_MULTISAMPLE) && (dest_target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    m_write_color_program.set_uniform("sample_mask", static_cast<int>(1U << dest_sample_index));
    m_write_color_array_program.set_uniform("sample_mask", static_cast<int>(1U << dest_sample_index));
    m_write_depth_program.set_uniform("sample_mask", static_cast<int>(1U << dest_sample_index));
    m_write_depth_array_program.set_uniform("sample_mask", static_cast<int>(1U << dest_sample_index));

    const GLenum src_target = (dest_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

    GL_ENTRYPOINT(glBindTexture)(src_target, src_texture);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(dest_target, dest_texture);
    VOGL_CHECK_GL_ERROR;

    const int mip_level = 0;

    GLenum internal_fmt = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, mip_level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_fmt));

    int mip_width = 0, mip_height = 0, mip_depth = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, mip_level, GL_TEXTURE_WIDTH, &mip_width);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, mip_level, GL_TEXTURE_HEIGHT, &mip_height);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(src_target, mip_level, GL_TEXTURE_DEPTH, &mip_depth);

    if ((vogl_check_gl_error()) || (mip_width < 1) || (mip_height < 1) || (mip_depth < 1))
        return false;

    const vogl_internal_tex_format *pInternal_tex_fmt = vogl_find_internal_texture_format(internal_fmt);
    if ((!pInternal_tex_fmt) || (pInternal_tex_fmt->m_optimum_get_image_fmt == GL_NONE) || (pInternal_tex_fmt->m_optimum_get_image_type == GL_NONE))
        return false;

    GLenum attachment_target = GL_COLOR_ATTACHMENT0;
    if ((pInternal_tex_fmt->m_comp_sizes[cTCDepth]) && (pInternal_tex_fmt->m_comp_sizes[cTCStencil]))
        attachment_target = GL_DEPTH_STENCIL_ATTACHMENT;
    else if (pInternal_tex_fmt->m_comp_sizes[cTCDepth])
        attachment_target = GL_DEPTH_ATTACHMENT;
    else if (pInternal_tex_fmt->m_comp_sizes[cTCStencil])
        return false;

    if (attachment_target == GL_COLOR_ATTACHMENT0)
    {
        GL_ENTRYPOINT(glDepthMask)(GL_FALSE);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDisable)(GL_DEPTH_TEST);
        VOGL_CHECK_GL_ERROR;
    }
    else
    {
        GL_ENTRYPOINT(glDepthMask)(GL_TRUE);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glEnable)(GL_DEPTH_TEST);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDepthFunc)(GL_ALWAYS);
        VOGL_CHECK_GL_ERROR;
    }

    GL_ENTRYPOINT(glDisable)(GL_STENCIL_TEST);
    VOGL_CHECK_GL_ERROR;

    // This method disables sRGB reads and writes (we can do this because the source is non-multisampled and the dest is a multisampled render target.)
    GLint orig_srgb_decode = 0;
    if (m_context_info.supports_extension("GL_EXT_texture_sRGB_decode"))
    {
        GL_ENTRYPOINT(glGetTexParameteriv)(src_target, GL_TEXTURE_SRGB_DECODE_EXT, &orig_srgb_decode);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glTexParameteri)(src_target, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT);
        VOGL_CHECK_GL_ERROR;
    }

    for (int layer = 0; layer < mip_depth; layer++)
    {
        // Create FBO
        GLuint fbo_handle = 0;
        GL_ENTRYPOINT(glGenFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, fbo_handle);
        VOGL_CHECK_GL_ERROR;

        if (dest_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
            GL_ENTRYPOINT(glFramebufferTextureLayer)(GL_DRAW_FRAMEBUFFER, attachment_target, dest_texture, mip_level, layer);
            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, attachment_target, dest_target, dest_texture, mip_level);
            VOGL_CHECK_GL_ERROR;
        }

        if (attachment_target == GL_COLOR_ATTACHMENT0)
        {
            GLenum draw_buf = GL_COLOR_ATTACHMENT0;
            GL_ENTRYPOINT(glDrawBuffers)(1, &draw_buf);
            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            GL_ENTRYPOINT(glDrawBuffer)(GL_NONE);
            VOGL_CHECK_GL_ERROR;
        }

        GLenum fbo_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
        VOGL_CHECK_GL_ERROR;

        bool status = false;

        if (fbo_status == GL_FRAMEBUFFER_COMPLETE)
        {
            // Render quad to FBO
            GL_ENTRYPOINT(glViewport)(0, 0, mip_width, mip_height);
            VOGL_CHECK_GL_ERROR;

            vogl_quad_vertex quad_tri_verts[3*2];
            memcpy(quad_tri_verts, get_quad_tri_verts(), sizeof(quad_tri_verts));

            for (uint32_t i = 0; i < 6; i++)
            {
                quad_tri_verts[i].m_uv0[0] *= mip_width;
                quad_tri_verts[i].m_uv0[1] *= mip_height;
                quad_tri_verts[i].m_uv0[2] = 1.0f * layer;
            }

            GL_ENTRYPOINT(glBufferData)(GL_ARRAY_BUFFER, sizeof(quad_tri_verts), quad_tri_verts, GL_STATIC_DRAW);
            VOGL_CHECK_GL_ERROR;

            if (attachment_target == GL_COLOR_ATTACHMENT0)
            {
                GL_ENTRYPOINT(glUseProgram)((src_target == GL_TEXTURE_2D_ARRAY) ? m_write_color_array_program.get_handle() : m_write_color_program.get_handle());
                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                GL_ENTRYPOINT(glUseProgram)((src_target == GL_TEXTURE_2D_ARRAY) ? m_write_depth_array_program.get_handle() : m_write_depth_program.get_handle());
                VOGL_CHECK_GL_ERROR;
            }

            GL_ENTRYPOINT(glDisable)(GL_FRAMEBUFFER_SRGB);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDrawArrays)(GL_TRIANGLES, 0, 6);
            VOGL_CHECK_GL_ERROR;

            status = true;
        } // fbo_status

        // Delete FBO
        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDrawBuffer)(GL_FRONT_LEFT);
        VOGL_CHECK_GL_ERROR;

        if (!status)
            goto failure;
    } // layer

    GL_ENTRYPOINT(glBindTexture)(src_target, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(dest_target, 0);
    VOGL_CHECK_GL_ERROR;

    return true;

failure:
    GL_ENTRYPOINT(glBindTexture)(src_target, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(dest_target, 0);
    VOGL_CHECK_GL_ERROR;

    if (m_context_info.supports_extension("GL_EXT_texture_sRGB_decode"))
    {
        GL_ENTRYPOINT(glTexParameteri)(src_target, GL_TEXTURE_SRGB_DECODE_EXT, orig_srgb_decode);
        VOGL_CHECK_GL_ERROR;
    }

    return false;
}

// How to retrieve MSAA stencil info:
// Step 1: Get MSAA depth/stencil texture info
// Step 2: Create new MSAA color texture - same res and # of samples, 32-bit RGBA format
// Step 3: Create new temp FBO, attach new MSAA color texture + MSAA depth/stencil texture
// Step 4: Clear FBO's color attachment
// Step 5: Disable depth mask, set depth func to always
// Step 6: For i = 0 to 255:
//  Enable stencil testing, set ref to i, reject when stencil != i
//  Draw a quad, have it output color value i
// Step 7: Now split this MSAA color texture into sep non-MSAA textures
// Step 8: Return these MSAA color textures to the caller
// Step 9: The caller now can combine the stencil data with the retrieved depth data

// Also see http://www.opengl.org/wiki/Texture#Stencil_texturing, but this is GL v4.3

bool vogl_msaa_texture_splitter::copy_stencil_samples_to_color(GLenum target, GLuint src_texture, GLuint &dest_texture)
{
    dest_texture = 0;

    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((target != GL_TEXTURE_2D_MULTISAMPLE) && (target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    GL_ENTRYPOINT(glBindTexture)(target, src_texture);
    VOGL_CHECK_GL_ERROR;

    const int mip_level = 0;

    GLenum internal_fmt = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, mip_level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_fmt));

    int width = 0, height = 0, depth = 0, samples = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, mip_level, GL_TEXTURE_WIDTH, &width);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, mip_level, GL_TEXTURE_HEIGHT, &height);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, mip_level, GL_TEXTURE_DEPTH, &depth);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, mip_level, GL_TEXTURE_SAMPLES, &samples);

    if ((vogl_check_gl_error()) || (width < 1) || (height < 1) || (depth < 1) || (samples < 1))
        return false;

    const vogl_internal_tex_format *pInternal_tex_fmt = vogl_find_internal_texture_format(internal_fmt);
    if ((!pInternal_tex_fmt) || (pInternal_tex_fmt->m_optimum_get_image_fmt == GL_NONE) || (pInternal_tex_fmt->m_optimum_get_image_type == GL_NONE))
        return false;

    if (!pInternal_tex_fmt->m_comp_sizes[cTCStencil])
        return false;

    GLenum fbo_attachment_target = GL_STENCIL_ATTACHMENT;
    if (pInternal_tex_fmt->m_comp_sizes[cTCDepth])
        fbo_attachment_target = GL_DEPTH_STENCIL_ATTACHMENT;

    GL_ENTRYPOINT(glGenTextures)(1, &dest_texture);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(target, dest_texture);
    VOGL_CHECK_GL_ERROR;

    const GLenum dest_internal_fmt = GL_RGBA;
    const GLboolean fixed_sample_locations = GL_TRUE;

    if (target == GL_TEXTURE_2D_MULTISAMPLE)
    {
        GL_ENTRYPOINT(glTexImage2DMultisample)(target, samples, dest_internal_fmt, width, height, fixed_sample_locations);
        VOGL_CHECK_GL_ERROR;
    }
    else
    {
        GL_ENTRYPOINT(glTexImage3DMultisample)(target, samples, dest_internal_fmt, width, height, depth, fixed_sample_locations);
        VOGL_CHECK_GL_ERROR;
    }

    GL_ENTRYPOINT(glUseProgram)(m_const_color_program.get_handle());
    VOGL_CHECK_GL_ERROR;

    for (int layer = 0; layer < depth; layer++)
    {
        GLuint temp_fbo = 0;
        GL_ENTRYPOINT(glGenFramebuffers)(1, &temp_fbo);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, temp_fbo);
        VOGL_CHECK_GL_ERROR;

        if (target == GL_TEXTURE_2D_MULTISAMPLE)
        {
            GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, dest_texture, mip_level);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, fbo_attachment_target, target, src_texture, mip_level);
            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            GL_ENTRYPOINT(glFramebufferTextureLayer)(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dest_texture, mip_level, layer);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glFramebufferTextureLayer)(GL_DRAW_FRAMEBUFFER, fbo_attachment_target, src_texture, mip_level, layer);
            VOGL_CHECK_GL_ERROR;
        }

        GLenum draw_buf = GL_COLOR_ATTACHMENT0;
        GL_ENTRYPOINT(glDrawBuffers)(1, &draw_buf);
        VOGL_CHECK_GL_ERROR;

        GLenum fbo_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
        VOGL_CHECK_GL_ERROR;

        bool status = false;

        if (fbo_status == GL_FRAMEBUFFER_COMPLETE)
        {
            GL_ENTRYPOINT(glDisable)(GL_FRAMEBUFFER_SRGB);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glViewport)(0, 0, width, height);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glClearColor)(0, 0, 0, 0);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glClear)(GL_COLOR_BUFFER_BIT);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glColorMask)(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glEnable)(GL_DEPTH_TEST);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDepthMask)(GL_FALSE);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDepthFunc)(GL_ALWAYS);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glEnable)(GL_STENCIL_TEST);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glStencilOp)(GL_KEEP, GL_KEEP, GL_REPLACE);
            VOGL_CHECK_GL_ERROR;

            vogl_quad_vertex quad_tri_verts[3*2];
            memcpy(quad_tri_verts, get_quad_tri_verts(), sizeof(quad_tri_verts));

            for (uint32_t i = 0; i < 6; i++)
            {
                quad_tri_verts[i].m_uv0[0] *= width;
                quad_tri_verts[i].m_uv0[1] *= height;
                quad_tri_verts[i].m_uv0[2] = 1.0f * layer;
            }

            GL_ENTRYPOINT(glBufferData)(GL_ARRAY_BUFFER, sizeof(quad_tri_verts), quad_tri_verts, GL_STATIC_DRAW);
            VOGL_CHECK_GL_ERROR;

            for (uint32_t stencil_val = 0; stencil_val <= 255; stencil_val++)
            {
                m_const_color_program.set_uniform("color", vec4F(stencil_val * (1.0f / 255.0f)));

                GL_ENTRYPOINT(glStencilFunc)(GL_EQUAL, stencil_val, 0xFF);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDrawArrays)(GL_TRIANGLES, 0, 6);
                VOGL_CHECK_GL_ERROR;

            } // stencil_val

            status = true;
        } // fbo_status

        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteFramebuffers)(1, &temp_fbo);
        VOGL_CHECK_GL_ERROR;

        temp_fbo = 0;

        if (!status)
            goto failure;

    } // layer

    GL_ENTRYPOINT(glBindTexture)(target, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glDisable)(GL_STENCIL_TEST);
    VOGL_CHECK_GL_ERROR;
    return true;

failure:
    GL_ENTRYPOINT(glBindTexture)(target, 0);
    VOGL_CHECK_GL_ERROR;

    if (dest_texture)
    {
        GL_ENTRYPOINT(glDeleteTextures)(1, &dest_texture);
        VOGL_CHECK_GL_ERROR;

        dest_texture = 0;
    }

    GL_ENTRYPOINT(glDisable)(GL_STENCIL_TEST);
    VOGL_CHECK_GL_ERROR;

    return false;
}

// Copies MSAA color samples from an MSAA 2D to 2D_ARRAY texture to the stencil buffer of a MSAA 2D or 2D_ARRAY depth/stencil texture.
// Source is NOT multisampled, dest is multisampled.
bool vogl_msaa_texture_splitter::copy_color_sample_to_stencil(
        GLuint src_texture, uint32_t dest_sample_index, GLenum dest_target, GLuint dest_texture)
{
    bool exit_status = false;

    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((dest_target != GL_TEXTURE_2D_MULTISAMPLE) && (dest_target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    const GLenum src_target = (dest_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

    GL_ENTRYPOINT(glBindTexture)(src_target, src_texture);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(dest_target, dest_texture);
    VOGL_CHECK_GL_ERROR;

    const int mip_level = 0;

    GLenum internal_fmt = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(dest_target, mip_level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_fmt));

    int width = 0, height = 0, depth = 0, samples = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(dest_target, mip_level, GL_TEXTURE_WIDTH, &width);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(dest_target, mip_level, GL_TEXTURE_HEIGHT, &height);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(dest_target, mip_level, GL_TEXTURE_DEPTH, &depth);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(dest_target, mip_level, GL_TEXTURE_SAMPLES, &samples);

    GL_ENTRYPOINT(glBindTexture)(dest_target, 0);
    VOGL_CHECK_GL_ERROR;

    if ((vogl_check_gl_error()) || (width < 1) || (height < 1) || (depth < 1) || (samples < 1))
        return false;

    const vogl_internal_tex_format *pInternal_tex_fmt = vogl_find_internal_texture_format(internal_fmt);
    if ((!pInternal_tex_fmt) || (pInternal_tex_fmt->m_optimum_get_image_fmt == GL_NONE) || (pInternal_tex_fmt->m_optimum_get_image_type == GL_NONE))
        return false;

    if (!pInternal_tex_fmt->m_comp_sizes[cTCStencil])
        return false;

    GLenum fbo_attachment_target = GL_STENCIL_ATTACHMENT;
    if (pInternal_tex_fmt->m_comp_sizes[cTCDepth])
        fbo_attachment_target = GL_DEPTH_STENCIL_ATTACHMENT;

    vogl_simple_gl_program &program = (dest_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? m_write_stencil_array_program : m_write_stencil_program;
    GL_ENTRYPOINT(glUseProgram)(program.get_handle());
    VOGL_CHECK_GL_ERROR;

    program.set_uniform("sample_mask", static_cast<int>(1 << dest_sample_index));

    GL_ENTRYPOINT(glColorMask)(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glStencilMask)(255);
    VOGL_CHECK_GL_ERROR;

    for (int layer = 0; layer < depth; layer++)
    {
        // Create FBO
        GLuint fbo_handle = 0;
        GL_ENTRYPOINT(glGenFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, fbo_handle);
        VOGL_CHECK_GL_ERROR;

        if (dest_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
            GL_ENTRYPOINT(glFramebufferTextureLayer)(GL_DRAW_FRAMEBUFFER, fbo_attachment_target, dest_texture, mip_level, layer);
            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, fbo_attachment_target, dest_target, dest_texture, mip_level);
            VOGL_CHECK_GL_ERROR;
        }

        GLenum draw_bufs[1] = { GL_NONE };
        GL_ENTRYPOINT(glDrawBuffers)(1, draw_bufs);
        VOGL_CHECK_GL_ERROR;

        GLenum fbo_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
        VOGL_CHECK_GL_ERROR;

        bool status = false;

        if (fbo_status == GL_FRAMEBUFFER_COMPLETE)
        {
            // Render quad to FBO
            GL_ENTRYPOINT(glViewport)(0, 0, width, height);
            VOGL_CHECK_GL_ERROR;

            if (!dest_sample_index)
            {
                GL_ENTRYPOINT(glClearStencil)(0);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glClear)(GL_STENCIL_BUFFER_BIT);
                VOGL_CHECK_GL_ERROR;
            }

            vogl_quad_vertex quad_tri_verts[3*2];
            memcpy(quad_tri_verts, get_quad_tri_verts(), sizeof(quad_tri_verts));

            for (uint32_t i = 0; i < 6; i++)
            {
                quad_tri_verts[i].m_uv0[0] *= width;
                quad_tri_verts[i].m_uv0[1] *= height;
                quad_tri_verts[i].m_uv0[2] = 1.0f * layer;
            }

            GL_ENTRYPOINT(glBufferData)(GL_ARRAY_BUFFER, sizeof(quad_tri_verts), quad_tri_verts, GL_STATIC_DRAW);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glEnable)(GL_DEPTH_TEST);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDepthMask)(GL_FALSE);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDepthFunc)(GL_ALWAYS);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glEnable)(GL_STENCIL_TEST);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glStencilOp)(GL_KEEP, GL_KEEP, GL_REPLACE);
            VOGL_CHECK_GL_ERROR;

            for (uint32_t stencil_val = 0; stencil_val <= 255; stencil_val++)
            {
                program.set_uniform("stencil_comp_val", stencil_val / 255.0f);

                GL_ENTRYPOINT(glStencilFunc)(GL_ALWAYS, stencil_val, 0xFF);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDrawArrays)(GL_TRIANGLES, 0, 6);
                VOGL_CHECK_GL_ERROR;

            } // stencil_val

            status = true;
        } // fbo_status

        // Delete FBO
        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDrawBuffer)(GL_FRONT_LEFT);
        VOGL_CHECK_GL_ERROR;

        if (!status)
            goto failure;

    } // layer

    exit_status = true;

failure:
    GL_ENTRYPOINT(glBindTexture)(src_target, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindTexture)(dest_target, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glColorMask)(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glDisable)(GL_STENCIL_TEST);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glStencilMask)(0);
    VOGL_CHECK_GL_ERROR;

    return exit_status;
}










































