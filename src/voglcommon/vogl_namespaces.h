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

// File: vogl_namespaces.h

#ifdef VOGL_NAMESPACES_HEADER

#define VOGL_SHADER_OBJECT 1
#define VOGL_PROGRAM_OBJECT 2

//----------------------------------------------------------------------------------------------------------------------
// GL namespace enums/table
// Important: If you change these enums, be sure to recompile and regenerate all .inc files using voglgen!
// Keep this in sync with g_vogl_namespace_names!
//----------------------------------------------------------------------------------------------------------------------
enum vogl_namespace_t
{
    VOGL_NAMESPACE_UNKNOWN = -2,
    VOGL_NAMESPACE_INVALID = -1,
    VOGL_NAMESPACE_FRAMEBUFFERS,
    VOGL_NAMESPACE_TEXTURES,
    VOGL_NAMESPACE_RENDER_BUFFERS,
    VOGL_NAMESPACE_QUERIES,
    VOGL_NAMESPACE_SAMPLERS,
    VOGL_NAMESPACE_PROGRAM_ARB,
    VOGL_NAMESPACE_PROGRAMS,
    VOGL_NAMESPACE_VERTEX_ARRAYS,
    VOGL_NAMESPACE_LISTS,
    VOGL_NAMESPACE_LOCATIONS,
    VOGL_NAMESPACE_FENCES, // TODO: Is this a alias to syncs?
    VOGL_NAMESPACE_SYNCS,
    VOGL_NAMESPACE_PIPELINES,
    VOGL_NAMESPACE_SHADERS,
    VOGL_NAMESPACE_BUFFERS,
    VOGL_NAMESPACE_FEEDBACKS,
    VOGL_NAMESPACE_VERTEX_ARRAYS_APPLE,
    VOGL_NAMESPACE_FRAGMENT_SHADER_ATI,
    VOGL_NAMESPACE_GLHANDLEARB,
    VOGL_TOTAL_NAMESPACES
};

struct vogl_gl_type_to_namespace_t
{
    const char *m_pType;
    vogl_namespace_t m_namespace;
};

extern const char *g_vogl_namespace_names[VOGL_TOTAL_NAMESPACES];

extern const char *vogl_get_namespace_name(vogl_namespace_t namespace_index);

// Note that g_vogl_gl_type_to_namespace[]'s size is != VOGL_TOTAL_NAMESPACES, because of namespace aliases.
extern vogl_gl_type_to_namespace_t g_vogl_gl_type_to_namespace[];
extern const uint32_t g_vogl_gl_type_to_namespace_array_size;

extern vogl_namespace_t vogl_find_namespace_from_gl_type(const char *pType);

#endif // VOGL_NAMESPACES_HEADER

#ifdef VOGL_NAMESPACES_IMPLEMENTATION

// Keep this in sync with vogl_namespace_t!
const char *g_vogl_namespace_names[VOGL_TOTAL_NAMESPACES] =
    {
        "framebuffers",
        "textures",
        "render_buffers",
        "queries",
        "samplers",
        "program_arb",
        "programs",
        "vertex_arrays",
        "lists",
        "locations",
        "fences",
        "syncs",
        "pipelines",
        "shaders",
        "buffers",
        "feedbacks",
        "vertex_arrays_apple",
        "fragment_shader_ati",
        "glhandlearb"
    };

const char *vogl_get_namespace_name(vogl_namespace_t namespace_index)
{
    VOGL_ASSERT(namespace_index >= VOGL_NAMESPACE_UNKNOWN);
    VOGL_ASSERT(namespace_index < VOGL_TOTAL_NAMESPACES);

    if ((namespace_index == VOGL_NAMESPACE_INVALID) || (namespace_index >= VOGL_TOTAL_NAMESPACES))
        return "invalid";
    else if (namespace_index == VOGL_NAMESPACE_UNKNOWN)
        return "unknown";

    const char *pName = g_vogl_namespace_names[namespace_index];
    VOGL_ASSERT(pName);
    return pName;
}
//----------------------------------------------------------------------------------------------------------------------
// GL namespace enums/table
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_type_to_namespace_t g_vogl_gl_type_to_namespace[] =
    {
        { "unknown", VOGL_NAMESPACE_UNKNOWN },
        { "invalid", VOGL_NAMESPACE_INVALID },
        { "GLframebuffer", VOGL_NAMESPACE_FRAMEBUFFERS },
        { "GLtexture", VOGL_NAMESPACE_TEXTURES },
        { "GLrenderbuffer", VOGL_NAMESPACE_RENDER_BUFFERS },
        { "GLquery", VOGL_NAMESPACE_QUERIES },
        { "GLsampler", VOGL_NAMESPACE_SAMPLERS },
        { "GLprogramARB", VOGL_NAMESPACE_PROGRAM_ARB },
        { "GLprogram", VOGL_NAMESPACE_PROGRAMS },
        { "GLarray", VOGL_NAMESPACE_VERTEX_ARRAYS },
        { "GLlist", VOGL_NAMESPACE_LISTS },
        { "GLlocation", VOGL_NAMESPACE_LOCATIONS },
        { "GLlocationARB", VOGL_NAMESPACE_LOCATIONS },
        { "GLfence", VOGL_NAMESPACE_FENCES },
        { "GLsync", VOGL_NAMESPACE_SYNCS },
        { "GLpipeline", VOGL_NAMESPACE_PIPELINES },
        { "GLshader", VOGL_NAMESPACE_SHADERS },
        { "GLbuffer", VOGL_NAMESPACE_BUFFERS },
        { "GLfeedback", VOGL_NAMESPACE_FEEDBACKS },
        { "GLarrayAPPLE", VOGL_NAMESPACE_VERTEX_ARRAYS_APPLE },
        { "GLfragmentShaderATI", VOGL_NAMESPACE_FRAGMENT_SHADER_ATI },
        { "GLhandleARB", VOGL_NAMESPACE_GLHANDLEARB },
    };

const uint32_t g_vogl_gl_type_to_namespace_array_size = VOGL_ARRAY_SIZE(g_vogl_gl_type_to_namespace);

vogl_namespace_t vogl_find_namespace_from_gl_type(const char *pType)
{
    for (uint32_t i = 0; i < g_vogl_gl_type_to_namespace_array_size; i++)
        if (!vogl::vogl_stricmp(g_vogl_gl_type_to_namespace[i].m_pType, pType))
            return g_vogl_gl_type_to_namespace[i].m_namespace;

    return VOGL_NAMESPACE_INVALID;
}

#endif // VOGL_NAMESPACES_IMPLEMENTATION
