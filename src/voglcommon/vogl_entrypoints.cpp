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

// File: vogl_entrypoints.cpp
#include "vogl_common.h"
#include "vogl_console.h"

//----------------------------------------------------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------------------------------------------------
static vogl_gl_get_proc_address_helper_func_ptr_t g_vogl_pGet_proc_address_helper_func;

actual_gl_entrypoints_t g_vogl_actual_gl_entrypoints;

// The "direct" pointers are not wrapped, they go STRAIGHT to the driver. The non-direct are wrapped with optional callbacks.
vogl_void_func_ptr_t g_vogl_actual_gl_entrypoint_direct_func_ptrs[VOGL_NUM_ENTRYPOINTS];
vogl_void_func_ptr_t g_vogl_actual_gl_entrypoint_func_ptrs[VOGL_NUM_ENTRYPOINTS];

static vogl_gl_func_prolog_epilog_func_t g_gl_func_prolog_func_ptr;
static void *g_gl_func_prolog_func_user_data;

static vogl_gl_func_prolog_epilog_func_t g_gl_func_epilog_func_ptr;
static void *g_gl_func_epilog_func_user_data;

//----------------------------------------------------------------------------------------------------------------------
// Define direct GL/GLX wrapper funcs, which allows us to intercept every GL/GLX call made be us, and optionally call
// a user provided prolog/epilog funcs. This is where the *actual* GL/GLX driver funcs are called.
//----------------------------------------------------------------------------------------------------------------------
#define DEF_PROTO(exported, category, ret, ret_type, num_params, func_name, args, params)                         \
    static ret GLAPIENTRY VOGL_GLUER(vogl_direct_wrapper_, func_name) args                                        \
    {                                                                                                             \
        void *pStack_data = NULL;                                                                                 \
        if (g_gl_func_prolog_func_ptr)                                                                            \
            g_gl_func_prolog_func_ptr(VOGL_ENTRYPOINT_##func_name, g_gl_func_prolog_func_user_data, &pStack_data); \
        ret result = g_vogl_actual_gl_entrypoints.m_##func_name##_direct params;                                   \
        if (g_gl_func_epilog_func_ptr)                                                                            \
            g_gl_func_epilog_func_ptr(VOGL_ENTRYPOINT_##func_name, g_gl_func_epilog_func_user_data, &pStack_data); \
        return result;                                                                                            \
    }

#define DEF_PROTO_VOID(exported, category, ret, ret_type, num_params, func_name, args, params)                    \
    static void GLAPIENTRY VOGL_GLUER(vogl_direct_wrapper_, func_name) args                                       \
    {                                                                                                             \
        void *pStack_data = NULL;                                                                                 \
        if (g_gl_func_prolog_func_ptr)                                                                            \
            g_gl_func_prolog_func_ptr(VOGL_ENTRYPOINT_##func_name, g_gl_func_prolog_func_user_data, &pStack_data); \
        g_vogl_actual_gl_entrypoints.m_##func_name##_direct params;                                                \
        if (g_gl_func_epilog_func_ptr)                                                                            \
            g_gl_func_epilog_func_ptr(VOGL_ENTRYPOINT_##func_name, g_gl_func_epilog_func_user_data, &pStack_data); \
    }

#include "gl_glx_cgl_wgl_protos.inc"

//----------------------------------------------------------------------------------------------------------------------
// Function vogl_init_actual_gl_entrypoints
//----------------------------------------------------------------------------------------------------------------------
void vogl_init_actual_gl_entrypoints(vogl_gl_get_proc_address_helper_func_ptr_t pGet_proc_address_helper_func, bool wrap_all_gl_calls)
{
    VOGL_VERIFY(pGet_proc_address_helper_func);

    g_vogl_pGet_proc_address_helper_func = pGet_proc_address_helper_func;

#define DEF_PROTO_UNIVERSAL(category, ret, ret_type, num_params, name, args, params)                                                                          \
    {                                                                                                                                                         \
        vogl_void_func_ptr_t pFunc = g_vogl_pGet_proc_address_helper_func(#name);                                                                             \
        g_vogl_actual_gl_entrypoint_direct_func_ptrs[VOGL_ENTRYPOINT_##name] = pFunc;                                                                         \
        g_vogl_actual_gl_entrypoints.m_##name##_direct = reinterpret_cast<name##_func_ptr_t>(pFunc);                                                          \
        if (pFunc)                                                                                                                                            \
        {                                                                                                                                                     \
            if (wrap_all_gl_calls)                                                                                                                            \
            {                                                                                                                                                 \
                g_vogl_actual_gl_entrypoint_func_ptrs[VOGL_ENTRYPOINT_##name] = reinterpret_cast<vogl_void_func_ptr_t>(VOGL_GLUER(vogl_direct_wrapper_, name)); \
                g_vogl_actual_gl_entrypoints.m_##name = VOGL_GLUER(vogl_direct_wrapper_, name);                                                               \
            }                                                                                                                                                 \
            else                                                                                                                                              \
            {                                                                                                                                                 \
                g_vogl_actual_gl_entrypoint_func_ptrs[VOGL_ENTRYPOINT_##name] = reinterpret_cast<vogl_void_func_ptr_t>(pFunc);                                   \
                g_vogl_actual_gl_entrypoints.m_##name = reinterpret_cast<name##_func_ptr_t>(pFunc);                                                            \
            }                                                                                                                                                 \
        }                                                                                                                                                     \
    }

#define DEF_PROTO_EXPORTED(category, ret, ret_type, num_params, name, args, params) DEF_PROTO_UNIVERSAL(category, ret, ret_type, num_params, name, arg, params)
#define DEF_PROTO_EXPORTED_VOID(category, ret, ret_type, num_params, name, args, params) DEF_PROTO_UNIVERSAL(category, ret, ret_type, num_params, name, arg, params)

#define DEF_PROTO_INTERNAL(category, ret, ret_type, num_params, name, args, params) DEF_PROTO_UNIVERSAL(category, ret, ret_type, num_params, name, arg, params)
#define DEF_PROTO_INTERNAL_VOID(category, ret, ret_type, num_params, name, args, params) DEF_PROTO_UNIVERSAL(category, ret, ret_type, num_params, name, arg, params)

#define DEF_PROTO(exported, category, ret, ret_type, num_params, name, args, params) exported(category, ret, ret_type, num_params, name, args, params)
#define DEF_PROTO_VOID(exported, category, ret, ret_type, num_params, name, args, params) exported(category, ret, ret_type, num_params, name, args, params)
#include "gl_glx_cgl_wgl_protos.inc"
#undef DEF_PROTO_UNIVERSAL
}

//----------------------------------------------------------------------------------------------------------------------
// Define gl/glx entrypoint desc tables
//----------------------------------------------------------------------------------------------------------------------
#define DEF_PROTO_EXPORTED_VOID true
#define DEF_PROTO_EXPORTED true
#define DEF_PROTO_INTERNAL_VOID false
#define DEF_PROTO_INTERNAL false
#define DEF_FUNCTION_BEGIN(exported, category, ret, ret_type, num_params, name, args, params) \
    {                                                                                         \
    #name, exported, ret_type, num_params, #args, NULL,
#define DEF_FUNCTION_INFO(namespace_index, return_spectype, category, version, profile, deprecated, whitelisted, is_nullable, whitelisted_for_displaylists, listable) (vogl_namespace_t) namespace_index, #return_spectype, #category, #version, #profile, #deprecated, whitelisted, is_nullable, whitelisted_for_displaylists, listable
#define DEF_FUNCTION_BEGIN_PARAMS
#define DEF_FUNCTION_IN_VALUE_PARAM(namespace_index, spectype, type, ctype, name)
#define DEF_FUNCTION_IN_REFERENCE_PARAM(namespace_index, spectype, type, ctype, name)
#define DEF_FUNCTION_IN_ARRAY_PARAM(namespace_index, spectype, type, ctype, name, size)
#define DEF_FUNCTION_OUT_REFERENCE_PARAM(namespace_index, spectype, type, ctype, name)
#define DEF_FUNCTION_OUT_ARRAY_PARAM(namespace_index, spectype, type, ctype, name, size)
#define DEF_FUNCTION_END_PARAMS
#define DEF_FUNCTION_RETURN(spectype, type, ctype)
#define DEF_FUNCTION_END(exported, category, ret, ret_type, num_params, name, args, params) \
    , false, false, false, 0, 0, NULL                                                       \
    }                                                                                       \
    ,

//$ TODO mikesart: backtrace code...
gl_entrypoint_desc_t g_vogl_entrypoint_descs[VOGL_NUM_ENTRYPOINTS] =
    {
#include "gl_glx_cgl_wgl_func_descs.inc"
    };

//----------------------------------------------------------------------------------------------------------------------
// Define gl/glx entrypoint parameter desc tables
//----------------------------------------------------------------------------------------------------------------------
gl_entrypoint_param_desc_t g_vogl_entrypoint_param_descs[VOGL_NUM_ENTRYPOINTS][VOGL_MAX_ENTRYPOINT_PARAMETERS];

static uint32_t g_custom_array_size_macro_indices[] =
    {
#include "gl_glx_cgl_wgl_array_size_macro_func_param_indices.inc"
    };

// This function casues the compiler to run for "awhile" on MSVC and GCC. We should move it to its own file and 
// then disable optimization for that file from the build script. In the short term, I just disable optimization
// on MSVC.
#if defined(COMPILER_MSVC)
    #pragma optimize("", off)
#endif
//----------------------------------------------------------------------------------------------------------------------
// Function vogl_init_gl_entrypoint_descs
//----------------------------------------------------------------------------------------------------------------------
void vogl_init_gl_entrypoint_descs()
{
    gl_entrypoint_param_desc_t *pDst = &g_vogl_entrypoint_param_descs[0][0];

#define DEF_FUNCTION_BEGIN(exported, category, ret, ret_type, num_params, name, args, params)
#define DEF_FUNCTION_INFO(namespace_index, return_spectype, category, version, profile, deprecated, is_whitelisted, is_nullable, whitelisted_for_displaylists, listable)
#define DEF_FUNCTION_BEGIN_PARAMS \
    {                             \
        gl_entrypoint_param_desc_t *pCur = pDst;
#define DEF_FUNCTION_IN_VALUE_PARAM(namespace_index, spectype, type, ctype, name) \
    {                                                                             \
        pCur->m_pSpec_type = #spectype;                                           \
        pCur->m_pName = #name;                                                    \
        pCur->m_ctype = ctype;                                                    \
        pCur->m_class = VOGL_VALUE_PARAM;                                          \
        pCur->m_input = true;                                                     \
        pCur->m_namespace = (vogl_namespace_t)namespace_index;                     \
        ++pCur;                                                                   \
    }
#define DEF_FUNCTION_IN_REFERENCE_PARAM(namespace_index, spectype, type, ctype, name) \
    {                                                                                 \
        pCur->m_pSpec_type = #spectype;                                               \
        pCur->m_pName = #name;                                                        \
        pCur->m_ctype = ctype;                                                        \
        pCur->m_class = VOGL_REF_PARAM;                                                \
        pCur->m_input = true;                                                         \
        pCur->m_namespace = (vogl_namespace_t)namespace_index;                         \
        ++pCur;                                                                       \
    }
#define DEF_FUNCTION_IN_ARRAY_PARAM(namespace_index, spectype, type, ctype, name, size) \
    {                                                                                   \
        pCur->m_pSpec_type = #spectype;                                                 \
        pCur->m_pName = #name;                                                          \
        pCur->m_ctype = ctype;                                                          \
        pCur->m_class = VOGL_ARRAY_PARAM;                                                \
        pCur->m_input = true;                                                           \
        pCur->m_pSize = #size;                                                          \
        pCur->m_namespace = (vogl_namespace_t)namespace_index;                           \
        ++pCur;                                                                         \
    }
#define DEF_FUNCTION_OUT_REFERENCE_PARAM(namespace_index, spectype, type, ctype, name) \
    {                                                                                  \
        pCur->m_pSpec_type = #spectype;                                                \
        pCur->m_pName = #name;                                                         \
        pCur->m_ctype = ctype;                                                         \
        pCur->m_class = VOGL_ARRAY_PARAM;                                               \
        pCur->m_input = false;                                                         \
        pCur->m_namespace = (vogl_namespace_t)namespace_index;                          \
        ++pCur;                                                                        \
    }
#define DEF_FUNCTION_OUT_ARRAY_PARAM(namespace_index, spectype, type, ctype, name, size) \
    {                                                                                    \
        pCur->m_pSpec_type = #spectype;                                                  \
        pCur->m_pName = #name;                                                           \
        pCur->m_ctype = ctype;                                                           \
        pCur->m_class = VOGL_ARRAY_PARAM;                                                 \
        pCur->m_input = false;                                                           \
        pCur->m_pSize = #size;                                                           \
        pCur->m_namespace = (vogl_namespace_t)namespace_index;                            \
        ++pCur;                                                                          \
    }
#define DEF_FUNCTION_END_PARAMS                                    \
    VOGL_ASSERT((pCur - pDst) <= VOGL_MAX_ENTRYPOINT_PARAMETERS); \
    }
#define DEF_FUNCTION_RETURN(spectype, type, ctype)
#define DEF_FUNCTION_END(exported, category, ret, ret_type, num_params, name, args, params) pDst += VOGL_MAX_ENTRYPOINT_PARAMETERS;

#include "gl_glx_cgl_wgl_func_descs.inc"

    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(g_custom_array_size_macro_indices); i++)
    {
        uint32_t idx = g_custom_array_size_macro_indices[i];
        uint32_t func = idx >> 16;
        uint32_t param = idx & 0xFFFF;
        VOGL_ASSERT(func < VOGL_NUM_ENTRYPOINTS);
        VOGL_ASSERT(param < g_vogl_entrypoint_descs[func].m_num_params);

        g_vogl_entrypoint_param_descs[func][param].m_has_custom_array_size_macro = true;
    }

    for (uint32_t i = 0; i < VOGL_NUM_ENTRYPOINTS; i++)
    {
        gl_entrypoint_desc_t &desc = g_vogl_entrypoint_descs[i];
        VOGL_ASSERT(desc.m_return_namespace >= VOGL_NAMESPACE_UNKNOWN);
        VOGL_ASSERT(desc.m_return_namespace < VOGL_TOTAL_NAMESPACES);

        if (strcmp(desc.m_pName, "glGenTextures") == 0)
        {
            if (g_vogl_entrypoint_param_descs[i][1].m_namespace != VOGL_NAMESPACE_TEXTURES)
            {
                vogl_error_printf("vogl_namespace_t enum is bad, please rebuild");
                exit(EXIT_FAILURE);
            }
        }

        if (desc.m_return_ctype != VOGL_VOID)
        {
            if ((size_t)get_vogl_process_gl_ctypes()[desc.m_return_ctype].m_size < 1)
                vogl_warning_printf("function %s's return ctype %s is too small\n", desc.m_pName, get_vogl_process_gl_ctypes()[desc.m_return_ctype].m_pName);

            if ((size_t)get_vogl_process_gl_ctypes()[desc.m_return_ctype].m_size > sizeof(uint64_t))
                vogl_warning_printf("function %s's return ctype %s is too large\n", desc.m_pName, get_vogl_process_gl_ctypes()[desc.m_return_ctype].m_pName);
        }

        for (uint32_t j = 0; j < desc.m_num_params; j++)
        {
            if ((size_t)get_vogl_process_gl_ctypes()[g_vogl_entrypoint_param_descs[i][j].m_ctype].m_size < 1)
                vogl_warning_printf("param %u of function %s ctype %s is too small\n", j, desc.m_pName, get_vogl_process_gl_ctypes()[g_vogl_entrypoint_param_descs[i][j].m_ctype].m_pName);

            if ((size_t)get_vogl_process_gl_ctypes()[g_vogl_entrypoint_param_descs[i][j].m_ctype].m_size > sizeof(uint64_t))
                vogl_warning_printf("param %u of function %s ctype %s is too large\n", j, desc.m_pName, get_vogl_process_gl_ctypes()[g_vogl_entrypoint_param_descs[i][j].m_ctype].m_pName);
        }

        desc.m_pAPI_prefix = "GL";

        // FIXME: Add more prefixes as we add platforms. voglgen should probably supply this.
        // They must correspond with the GL enum prefixes in glx_enum_desc.inc, gl_enum_desc.inc, etc.
        if ((desc.m_pName[0] == 'w') && (desc.m_pName[1] == 'g') && (desc.m_pName[2] == 'l'))
            desc.m_pAPI_prefix = "WGL";
        else if ((desc.m_pName[0] == 'C') && (desc.m_pName[1] == 'G') && (desc.m_pName[2] == 'L'))
            desc.m_pAPI_prefix = "CGL";
        else if ((desc.m_pName[0] == 'g') && (desc.m_pName[1] == 'l') && (desc.m_pName[2] == 'X'))
            desc.m_pAPI_prefix = "GLX";
        else if ((desc.m_pName[0] == 'g') && (desc.m_pName[1] == 'l'))
            desc.m_pAPI_prefix = "GL";
        else
        {
            vogl_warning_printf("Unknown function prefix: %s\n", desc.m_pName);
        }
    }

    for (uint32_t i = 0; i < VOGL_NUM_ENTRYPOINTS; i++)
    {
        get_vogl_entrypoint_hashmap().insert(g_vogl_entrypoint_descs[i].m_pName, static_cast<gl_entrypoint_id_t>(i));
    }
}
#if defined(COMPILER_MSVC)
    #pragma optimize("", on)
#endif


//----------------------------------------------------------------------------------------------------------------------
// Function vogl_find_entrypoint
//----------------------------------------------------------------------------------------------------------------------
gl_entrypoint_id_t vogl_find_entrypoint(const dynamic_string &name)
{
    entrypoint_name_hash_map_t::const_iterator it(get_vogl_entrypoint_hashmap().find(name));
    if (it == get_vogl_entrypoint_hashmap().end())
        return VOGL_ENTRYPOINT_INVALID;
    return it->second;
}

//----------------------------------------------------------------------------------------------------------------------
// Function vogl_does_entrypoint_refer_to_namespace
//----------------------------------------------------------------------------------------------------------------------
bool vogl_does_entrypoint_refer_to_namespace(gl_entrypoint_id_t entrypoint_id, vogl_namespace_t namespace_id)
{
    VOGL_ASSERT(entrypoint_id < VOGL_NUM_ENTRYPOINTS);
    VOGL_ASSERT(namespace_id < VOGL_TOTAL_NAMESPACES);

    const gl_entrypoint_desc_t &desc = g_vogl_entrypoint_descs[entrypoint_id];
    if (desc.m_return_namespace == namespace_id)
        return true;

    for (uint32_t i = 0; i < desc.m_num_params; i++)
        if (g_vogl_entrypoint_param_descs[entrypoint_id][i].m_namespace == namespace_id)
            return true;

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_set_direct_gl_func_prolog
//----------------------------------------------------------------------------------------------------------------------
void vogl_set_direct_gl_func_prolog(vogl_gl_func_prolog_epilog_func_t pFunc, void *pUser_data)
{
    g_gl_func_prolog_func_ptr = pFunc;
    g_gl_func_prolog_func_user_data = pUser_data;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_set_direct_gl_func_epilog
//----------------------------------------------------------------------------------------------------------------------
void vogl_set_direct_gl_func_epilog(vogl_gl_func_prolog_epilog_func_t pFunc, void *pUser_data)
{
    g_gl_func_epilog_func_ptr = pFunc;
    g_gl_func_epilog_func_user_data = pUser_data;
}
