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

// File: vogl_texenv_state.cpp
#include "vogl_texenv_state.h"

vogl_texenv_state::vogl_texenv_state()
    : m_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_texenv_state::~vogl_texenv_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_texenv_state::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    clear();

    VOGL_CHECK_GL_ERROR;

    bool any_gl_errors = false;

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_state_saver state_saver(cGSTActiveTexture);

    if (vogl_check_gl_error())
        any_gl_errors = true;

    //for (uint32_t texcoord_index = 0; texcoord_index < context_info.get_max_texture_coords(); texcoord_index++)
    for (uint32_t texcoord_index = 0; texcoord_index < context_info.get_max_texture_units(); texcoord_index++)
    {
        GL_ENTRYPOINT(glActiveTexture)(GL_TEXTURE0 + texcoord_index);

        if (vogl_check_gl_error())
            any_gl_errors = true;

#define GET_FLOAT(target, idx, pname)                                   \
    do                                                                  \
    {                                                                   \
        float values[4] = { 0, 0, 0, 0 };                               \
        GL_ENTRYPOINT(glGetTexEnvfv)(target, pname, values);            \
        if (vogl_check_gl_error())                                       \
            any_gl_errors = true;                                       \
        m_params[target].insert(pname, idx, values, sizeof(values[0])); \
    } while (0)
#define GET_INT(target, idx, pname)                                     \
    do                                                                  \
    {                                                                   \
        int values[4] = { 0, 0, 0, 0 };                                 \
        GL_ENTRYPOINT(glGetTexEnviv)(target, pname, values);            \
        if (vogl_check_gl_error())                                       \
            any_gl_errors = true;                                       \
        m_params[target].insert(pname, idx, values, sizeof(values[0])); \
    } while (0)

        GET_FLOAT(GL_TEXTURE_FILTER_CONTROL, texcoord_index, GL_TEXTURE_LOD_BIAS);
        GET_INT(GL_POINT_SPRITE, texcoord_index, GL_COORD_REPLACE);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_TEXTURE_ENV_MODE);
        GET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_TEXTURE_ENV_COLOR);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_COMBINE_RGB);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_COMBINE_ALPHA);
        GET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_RGB_SCALE);
        GET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_ALPHA_SCALE);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC0_RGB);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC1_RGB);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC2_RGB);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC0_ALPHA);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC1_ALPHA);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC2_ALPHA);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND0_RGB);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND1_RGB);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND2_RGB);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND0_ALPHA);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND1_ALPHA);
        GET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND2_ALPHA);

// TODO:
//{ "glGetTexEnv",	'E',	1,	"GL_SOURCE3_RGB_NV",  0x8583},
//{ "glGetTexEnv",	'E',	1,	"GL_SOURCE3_ALPHA_NV",  0x858B},
//{ "glGetTexEnv",	'E',	1,	"GL_OPERAND3_RGB_NV",  0x8593},
//{ "glGetTexEnv",	'E',	1,	"GL_OPERAND3_ALPHA_NV",  0x859B},
//{ "glGetTexEnv",	'E',	1,	"GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV",  0x86D9},
//{ "glGetTexEnv",	'E',	1,	"GL_SHADER_OPERATION_NV",  0x86DF},
//{ "glGetTexEnv",	'E',	4,	"GL_CULL_MODES_NV",  0x86E0},
//{ "glGetTexEnv",	'F',	4,	"GL_OFFSET_TEXTURE_MATRIX_NV",  0x86E1},
//{ "glGetTexEnv",	'F',	1,	"GL_OFFSET_TEXTURE_SCALE_NV",  0x86E2},
//{ "glGetTexEnv",	'F',	1,	"GL_OFFSET_TEXTURE_BIAS_NV",  0x86E3},
//{ "glGetTexEnv",	'E',	1,	"GL_PREVIOUS_TEXTURE_INPUT_NV",  0x86E4},
//{ "glGetTexEnv",	'F',	3,	"GL_CONST_EYE_NV",  0x86E5},
//{ "glGetTexEnv",	'E',	1,	"GL_BUMP_TARGET_ATI",  0x877C},
#undef GET_FLOAT
#undef GET_INT

#define GET_FLOAT(target, idx, pname)                                   \
    do                                                                  \
    {                                                                   \
        float values[4] = { 0, 0, 0, 0 };                               \
        GL_ENTRYPOINT(glGetTexGenfv)(target, pname, values);            \
        if (vogl_check_gl_error())                                       \
            any_gl_errors = true;                                       \
        m_params[target].insert(pname, idx, values, sizeof(values[0])); \
    } while (0)
#define GET_INT(target, idx, pname)                                     \
    do                                                                  \
    {                                                                   \
        int values[4] = { 0, 0, 0, 0 };                                 \
        GL_ENTRYPOINT(glGetTexGeniv)(target, pname, values);            \
        if (vogl_check_gl_error())                                       \
            any_gl_errors = true;                                       \
        m_params[target].insert(pname, idx, values, sizeof(values[0])); \
    } while (0)
        GET_INT(GL_S, texcoord_index, GL_TEXTURE_GEN_MODE);
        GET_FLOAT(GL_S, texcoord_index, GL_OBJECT_PLANE);
        GET_FLOAT(GL_S, texcoord_index, GL_EYE_PLANE);

        GET_INT(GL_T, texcoord_index, GL_TEXTURE_GEN_MODE);
        GET_FLOAT(GL_T, texcoord_index, GL_OBJECT_PLANE);
        GET_FLOAT(GL_T, texcoord_index, GL_EYE_PLANE);

        GET_INT(GL_R, texcoord_index, GL_TEXTURE_GEN_MODE);
        GET_FLOAT(GL_R, texcoord_index, GL_OBJECT_PLANE);
        GET_FLOAT(GL_R, texcoord_index, GL_EYE_PLANE);

        GET_INT(GL_Q, texcoord_index, GL_TEXTURE_GEN_MODE);
        GET_FLOAT(GL_Q, texcoord_index, GL_OBJECT_PLANE);
        GET_FLOAT(GL_Q, texcoord_index, GL_EYE_PLANE);
#undef GET_DBL
#undef GET_INT
    }

    if (vogl_check_gl_error())
        any_gl_errors = true;

    if (any_gl_errors)
    {
        clear();

        vogl_error_printf("GL error while enumerating texenv/texgen params\n");

        return false;
    }

    m_valid = true;

    return true;
}

bool vogl_texenv_state::set_texenv_parameter(GLenum target, uint32_t index, GLenum pname) const
{
    VOGL_FUNC_TRACER

    const vogl_state_vector *pState_vec = m_params.find_value(target);
    if (!pState_vec)
        return false;

    const vogl_state_data *pData = pState_vec->find(pname, index);
    if (!pData)
        return false;

    enum
    {
        cMaxElements = 4
    };
    if (pData->get_num_elements() > cMaxElements)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((pData->get_data_type() == cSTFloat) || (pData->get_data_type() == cSTDouble))
    {
        float fvals[cMaxElements];
        pData->get_float(fvals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glTexEnvf)(target, pname, fvals[0]);
        else
            GL_ENTRYPOINT(glTexEnvfv)(target, pname, fvals);
    }
    else
    {
        int ivals[cMaxElements];
        pData->get_int(ivals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glTexEnvi)(target, pname, ivals[0]);
        else
            GL_ENTRYPOINT(glTexEnviv)(target, pname, ivals);
    }

    return !vogl_check_gl_error();
}

bool vogl_texenv_state::set_texgen_parameter(GLenum coord, uint32_t index, GLenum pname) const
{
    VOGL_FUNC_TRACER

    const vogl_state_vector *pState_vec = m_params.find_value(coord);
    if (!pState_vec)
        return false;

    const vogl_state_data *pData = pState_vec->find(pname, index);
    if (!pData)
        return false;

    enum
    {
        cMaxElements = 4
    };
    if (pData->get_num_elements() > cMaxElements)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((pData->get_data_type() == cSTFloat) || (pData->get_data_type() == cSTDouble))
    {
        float fvals[cMaxElements];
        pData->get_float(fvals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glTexGenf)(coord, pname, fvals[0]);
        else
            GL_ENTRYPOINT(glTexGenfv)(coord, pname, fvals);
    }
    else
    {
        int ivals[cMaxElements];
        pData->get_int(ivals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glTexGeni)(coord, pname, ivals[0]);
        else
            GL_ENTRYPOINT(glTexGeniv)(coord, pname, ivals);
    }

    return !vogl_check_gl_error();
}

bool vogl_texenv_state::restore(const vogl_context_info &context_info) const
{
    VOGL_FUNC_TRACER

    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_state_saver state_saver(cGSTActiveTexture, cGSTMatrixMode);

    // So we can set the texgen GL_EYE_PLANE matrix without it getting transformed.
    GL_ENTRYPOINT(glMatrixMode)(GL_MODELVIEW);
    GL_ENTRYPOINT(glPushMatrix)();
    GL_ENTRYPOINT(glLoadIdentity)();

    bool any_gl_errors = false;
    if (vogl_check_gl_error())
        any_gl_errors = true;

    for (uint32_t texcoord_index = 0; texcoord_index < context_info.get_max_texture_coords(); texcoord_index++)
    {
        GL_ENTRYPOINT(glActiveTexture)(GL_TEXTURE0 + texcoord_index);

        if (vogl_check_gl_error())
            any_gl_errors = true;

#define SET_FLOAT(target, idx, pname) set_texenv_parameter(target, idx, pname);
#define SET_INT(target, idx, pname) set_texenv_parameter(target, idx, pname);

        SET_FLOAT(GL_TEXTURE_FILTER_CONTROL, texcoord_index, GL_TEXTURE_LOD_BIAS);
        SET_INT(GL_POINT_SPRITE, texcoord_index, GL_COORD_REPLACE);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_TEXTURE_ENV_MODE);
        SET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_TEXTURE_ENV_COLOR);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_COMBINE_RGB);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_COMBINE_ALPHA);
        SET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_RGB_SCALE);
        SET_FLOAT(GL_TEXTURE_ENV, texcoord_index, GL_ALPHA_SCALE);

        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC0_RGB);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC1_RGB);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC2_RGB);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC0_ALPHA);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC1_ALPHA);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_SRC2_ALPHA);

        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND0_RGB);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND1_RGB);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND2_RGB);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND0_ALPHA);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND1_ALPHA);
        SET_INT(GL_TEXTURE_ENV, texcoord_index, GL_OPERAND2_ALPHA);

#undef SET_FLOAT
#undef SET_INT

        if (vogl_check_gl_error())
            any_gl_errors = true;

#define SET_DBL(target, idx, pname) set_texgen_parameter(target, idx, pname);
#define SET_INT(target, idx, pname) set_texgen_parameter(target, idx, pname);

        SET_INT(GL_S, texcoord_index, GL_TEXTURE_GEN_MODE);
        SET_DBL(GL_S, texcoord_index, GL_OBJECT_PLANE);
        SET_DBL(GL_S, texcoord_index, GL_EYE_PLANE);

        SET_INT(GL_T, texcoord_index, GL_TEXTURE_GEN_MODE);
        SET_DBL(GL_T, texcoord_index, GL_OBJECT_PLANE);
        SET_DBL(GL_T, texcoord_index, GL_EYE_PLANE);

        SET_INT(GL_R, texcoord_index, GL_TEXTURE_GEN_MODE);
        SET_DBL(GL_R, texcoord_index, GL_OBJECT_PLANE);
        SET_DBL(GL_R, texcoord_index, GL_EYE_PLANE);

        SET_INT(GL_Q, texcoord_index, GL_TEXTURE_GEN_MODE);
        SET_DBL(GL_Q, texcoord_index, GL_OBJECT_PLANE);
        SET_DBL(GL_Q, texcoord_index, GL_EYE_PLANE);

#undef SET_DBL
#undef SET_INT

        if (vogl_check_gl_error())
            any_gl_errors = true;
    }

    GL_ENTRYPOINT(glPopMatrix)();

    if (vogl_check_gl_error())
        any_gl_errors = true;

    return !any_gl_errors;
}

void vogl_texenv_state::clear()
{
    VOGL_FUNC_TRACER

    m_params.clear();
    m_valid = false;
}

bool vogl_texenv_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    for (texenv_map::const_iterator it = m_params.begin(); it != m_params.end(); ++it)
        if (!it->second.serialize(node.add_object(get_gl_enums().find_gl_name(it->first)), blob_manager))
            return false;

    return true;
}

bool vogl_texenv_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    if (!node.is_object() || !node.are_all_children_objects_or_arrays())
        return false;

    for (uint32_t i = 0; i < node.size(); i++)
    {
        const dynamic_string &name = node.get_key(i);
        uint64_t enum_val = get_gl_enums().find_enum(name);
        if (enum_val == gl_enums::cUnknownEnum)
            return false;

        if (!m_params[static_cast<GLenum>(enum_val)].deserialize(*node.get_child(i), blob_manager))
            return false;
    }

    m_valid = true;

    return true;
}

const vogl_state_vector &vogl_texenv_state::get_state(GLenum target) const
{
    texenv_map::const_iterator iter = m_params.find(target);
    VOGL_ASSERT(iter != m_params.end());

    return iter->second;
}
