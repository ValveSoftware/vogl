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

// File: vogl_entrypoints.h
#ifndef VOGL_ENTRYPOINTS_H
#define VOGL_ENTRYPOINTS_H

#include "vogl_hash_map.h"

//----------------------------------------------------------------------------------------------------------------------
// define GL/GLX function pointer typedefs, i.e. glXGetProcAddress_func_ptr_t
//----------------------------------------------------------------------------------------------------------------------

#define DEF_PROTO(exported, category, ret, ret_type, num_params, name, args, params) typedef ret(GLAPIENTRY *name##_func_ptr_t) args;
#define DEF_PROTO_VOID(exported, category, ret, ret_type, num_params, name, args, params) typedef ret(GLAPIENTRY *name##_func_ptr_t) args;
#include "gl_glx_cgl_wgl_protos.inc"
#undef DEF_PROTO
#undef DEF_PROTO_VOID

// structure containing the actual gl/glx driver entrypoints
struct actual_gl_entrypoints_t
{
// The "direct" func pointers refer DIRECTLY to the driver's GL entrypoints. DO NOT call them unless you know why you need to.
#define DEF_PROTO(exported, category, ret, ret_type, num_params, name, args, params) name##_func_ptr_t m_##name##_direct;
#define DEF_PROTO_VOID(exported, category, ret, ret_type, num_params, name, args, params) name##_func_ptr_t m_##name##_direct;
#include "gl_glx_cgl_wgl_protos.inc"

// The non-direct func pointers (use these by default) pointer to wrappers funcs in vogl_entrypoints.cpp which call our global prolog function before calling the direct func ptr, then the epilog.
#define DEF_PROTO(exported, category, ret, ret_type, num_params, name, args, params) name##_func_ptr_t m_##name;
#define DEF_PROTO_VOID(exported, category, ret, ret_type, num_params, name, args, params) name##_func_ptr_t m_##name;
#include "gl_glx_cgl_wgl_protos.inc"
};

extern actual_gl_entrypoints_t g_vogl_actual_gl_entrypoints;

#define GL_ENTRYPOINT(x) g_vogl_actual_gl_entrypoints.m_##x

// The "direct" entrypoints are the REAL driver entrypoints, i.e. they skip our GL func prolog/epilog callbacks.
// Don't use this unless you know exactly why you need to.
#define DIRECT_GL_ENTRYPOINT(x) g_vogl_actual_gl_entrypoints.m_##x##_direct

//----------------------------------------------------------------------------------------------------------------------
// gl/glx entrypoint enum, enum format is VOGL_ENTRYPOINT_##name
//----------------------------------------------------------------------------------------------------------------------
#define DEF_PROTO_EXPORTED_VOID
#define DEF_PROTO_EXPORTED
#define DEF_PROTO_INTERNAL_VOID
#define DEF_PROTO_INTERNAL
#define DEF_FUNCTION_BEGIN(exported, category, ret, ret_type, num_params, name, args, params) VOGL_ENTRYPOINT_##name,
#define DEF_FUNCTION_INFO(namespace_index, return_spectype, category, version, profile, deprecated, is_whitelisted, is_nullable, whitelisted_for_displaylists, listable)
#define DEF_FUNCTION_BEGIN_PARAMS
#define DEF_FUNCTION_IN_VALUE_PARAM(namespace_index, spectype, type, ctype, name)
#define DEF_FUNCTION_IN_REFERENCE_PARAM(namespace_index, spectype, type, ctype, name)
#define DEF_FUNCTION_IN_ARRAY_PARAM(namespace_index, spectype, type, ctype, name, size)
#define DEF_FUNCTION_OUT_REFERENCE_PARAM(namespace_index, spectype, type, ctype, name)
#define DEF_FUNCTION_OUT_ARRAY_PARAM(namespace_index, spectype, type, ctype, name, size)
#define DEF_FUNCTION_END_PARAMS
#define DEF_FUNCTION_RETURN(spectype, type, ctype)
#define DEF_FUNCTION_END(exported, category, ret, ret_type, num_params, name, args, params)

enum gl_entrypoint_id_t
{
    VOGL_ENTRYPOINT_INVALID = -1,

#include "gl_glx_cgl_wgl_func_descs.inc"

    VOGL_NUM_ENTRYPOINTS
};

//----------------------------------------------------------------------------------------------------------------------
// gl/glx entrypoint description/parameter table
//----------------------------------------------------------------------------------------------------------------------
enum gl_entrypoint_param_class_t
{
    VOGL_VALUE_PARAM,
    VOGL_REF_PARAM,
    VOGL_ARRAY_PARAM,
    VOGL_TOTAL_PARAM_CLASSES
};

//----------------------------------------------------------------------------------------------------------------------
// struct gl_entrypoint_param_desc_t
//----------------------------------------------------------------------------------------------------------------------
struct gl_entrypoint_param_desc_t
{
    // Be careful modifying the order of these members - they are initialized via macros in vogl_entrypoints.cpp.
    const char *m_pName;
    const char *m_pSpec_type;
    vogl_ctype_t m_ctype;
    // Important: params marked as "reference" are not always to be trusted, the spec file sometimes marks things as ref params (like glxgetprocaddres's procname) that are really arrays.
    // Also, the serialized class values may differ from the param classes (i.e. an array of ubytes will be serialized as an array, not a ref)
    gl_entrypoint_param_class_t m_class;
    bool m_input;
    const char *m_pSize;
    vogl_namespace_t m_namespace;
    bool m_has_custom_array_size_macro;
    bool m_custom_array_size_macro_is_missing;
};

#define VOGL_MAX_ENTRYPOINT_PARAMETERS 24

//----------------------------------------------------------------------------------------------------------------------
// enum gl_entrypoint_flags_t
//----------------------------------------------------------------------------------------------------------------------
enum gl_entrypoint_flags_t
{
    cGLEFPrintedUnimplementedWarning = 1
};

//----------------------------------------------------------------------------------------------------------------------
// struct gl_entrypoint_desc_t
// Beware, order matters in here!
// IMPORTANT: If you modify this struct, also modify the DEF_FUNCTION_BEGIN/DEF_FUNCTION_END macros.
// TODO:
//  Add entrypoint prefix enum (glX, wgl, egl, gl) and member field here, like voglgen uses. Right now we have to keep on poking at the API prefix string, which is silly at runtime.
//  Split this struct up into two structs: static stuff (from the .inc file) and dynamic members (modified at runtime)
//----------------------------------------------------------------------------------------------------------------------
struct gl_entrypoint_desc_t
{
    const char *m_pName;
    bool m_exported;
    vogl_ctype_t m_return_ctype; // will be VOGL_VOID if the func has no return
    uint32_t m_num_params;
    const char *m_pParam_str;
    vogl_void_func_ptr_t m_pWrapper_func;

    vogl_namespace_t m_return_namespace;
    const char *m_pReturn_spec_type;
    const char *m_pCategory;
    const char *m_pVersion;
    const char *m_pProfile;
    const char *m_pDeprecated;
    bool m_is_whitelisted;
    bool m_is_nullable;
    bool m_whitelisted_for_displaylists;
    bool m_is_listable;

    bool m_has_custom_func_handler;
    bool m_custom_array_size_macro_is_missing;
    bool m_custom_return_param_array_size_macro_is_missing;
    atomic64_t m_trace_call_counter;
    uint64_t m_flags; // gl_entrypoint_flags_t

    const char *m_pAPI_prefix; // "GLX", "GL", "WGL", "EGL", etc.
};

//----------------------------------------------------------------------------------------------------------------------
// Global entrypoint/parameter description tables
//----------------------------------------------------------------------------------------------------------------------
extern gl_entrypoint_desc_t g_vogl_entrypoint_descs[VOGL_NUM_ENTRYPOINTS];

extern gl_entrypoint_param_desc_t g_vogl_entrypoint_param_descs[VOGL_NUM_ENTRYPOINTS][VOGL_MAX_ENTRYPOINT_PARAMETERS];

typedef vogl::hash_map<dynamic_string, gl_entrypoint_id_t, hasher<dynamic_string>, dynamic_string_equal_to_case_sensitive> entrypoint_name_hash_map_t;

inline entrypoint_name_hash_map_t &get_vogl_entrypoint_hashmap()
{
    static entrypoint_name_hash_map_t s_vogl_entrypoint_hashmap;
    return s_vogl_entrypoint_hashmap;
}

extern vogl_void_func_ptr_t g_vogl_actual_gl_entrypoint_direct_func_ptrs[VOGL_NUM_ENTRYPOINTS];
extern vogl_void_func_ptr_t g_vogl_actual_gl_entrypoint_func_ptrs[VOGL_NUM_ENTRYPOINTS];

//----------------------------------------------------------------------------------------------------------------------
// Typedefs/Functions
//----------------------------------------------------------------------------------------------------------------------
typedef vogl_void_func_ptr_t (*vogl_gl_get_proc_address_helper_func_ptr_t)(const char *pName);

// pGet_proc_address_helper_func must be valid
void vogl_init_actual_gl_entrypoints(vogl_gl_get_proc_address_helper_func_ptr_t pGet_proc_address_helper_func, bool wrap_all_gl_calls = true);

typedef void (*vogl_gl_func_prolog_epilog_func_t)(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **pStack_data);
void vogl_set_direct_gl_func_prolog(vogl_gl_func_prolog_epilog_func_t pFunc, void *pUser_data);
void vogl_set_direct_gl_func_epilog(vogl_gl_func_prolog_epilog_func_t pFunc, void *pUser_data);

void vogl_init_gl_entrypoint_descs();

// Returns VOGL_ENTRYPOINT_INVALID if the func name can't be found.
gl_entrypoint_id_t vogl_find_entrypoint(const dynamic_string &name);

bool vogl_does_entrypoint_refer_to_namespace(gl_entrypoint_id_t entrypoint_id, vogl_namespace_t namespace_id);

#endif // VOGL_ENTRYPOINTS_H
