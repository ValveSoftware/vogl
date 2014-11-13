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

// File: voglgen.cpp
// Important: If compiling with gcc, be sure strict aliasing is disabled: -fno-strict-aliasing

#include "vogl_core.h"
#include "vogl_console.h"
#include "vogl_colorized_console.h"
#include "vogl_find_files.h"
#include "vogl_file_utils.h"
#include "vogl_command_line_params.h"
#include "vogl_cfile_stream.h"
#include "vogl_vector2d.h"
#include "vogl_json.h"
#include "vogl_vec_interval.h"
#include "vogl_radix_sort.h"
#include "vogl_regex.h"
#include "vogl_hash_map.h"
#include <set>

#include "vogl_port.h"

#include "tinyxml2/tinyxml2.h"

#define VOGL_NAMESPACES_HEADER
#define VOGL_NAMESPACES_IMPLEMENTATION
#include "../voglcommon/vogl_namespaces.h"

using namespace vogl;

FILE *fopen_and_log_generic(const dynamic_string& outDirectory, const char* filename, const char* mode);

// If you want to debug a function as it flows through codegen, set this and the debugger will break at various
// points of interest for you.
const char* gDebugFunctionName = NULL;

#define VOGL_DEBUG_BREAK_ON_FUNCTION(_funcNameVar, _funcToBreakOn) \
    VOGL_CONDITIONAL_BREAKPOINT( (_funcToBreakOn != NULL && (_funcNameVar.begins_with(_funcToBreakOn))) )

//-------------------------------------------------------------------------------------------------------------------------------
// Command line options
//-------------------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs[] =
{
    { "quiet", 0, false, "Disable warning, verbose, and debug output" },
    { "verbose", 0, false, "Enable verbose output" },
    { "debug", 0, false, "Enable verbose debug information" },
    { "find", 0, false, NULL },
    { "func_regex", 1, false, NULL },
    { "param_regex", 1, false, NULL },
    { "category_regex", 1, false, NULL },
    { "ctype_regex", 1, false, NULL },
    { "namespace_regex", 1, false, NULL },
    { "srcdir", 1, false, NULL },
    { "specdir", 1, false, NULL },
    { "outinc", 1, false, NULL },
    { "outlinker", 1, false, NULL },
};

//-----------------------------------------------------------------------------------------------------------------------
// Function prefixes
//-------------------------------------------------------------------------------------------------------------------------------
enum gl_lib_t
{
    cGL,
    cGLX,
    cCGL,
    cWGL,
    cGLU
};

static const char *g_lib_api_prefixes[] =
{
    "gl",
    "glX",
    "CGL",
    "wgl",
    "glu"
};

//-------------------------------------------------------------------------------------------------------------------------------
// Vendor suffixes
//-------------------------------------------------------------------------------------------------------------------------------
static const char *g_gl_vendor_suffixes[] =
{
    "NV", "INTEL", "SGIS", "SGIX", "SUN", "NVX", "OES", "AMD", "ATI", "OES", "3DFX", "PGI", "INGR", "IBM", "APPLE"
};

//-------------------------------------------------------------------------------------------------------------------------------
// GL/GLX/CGL/WGL/etc. function specs
//-------------------------------------------------------------------------------------------------------------------------------
struct gl_function_param
{
    dynamic_string m_name;
    dynamic_string m_type;
    dynamic_string m_ctype;
    dynamic_string m_ctype_enum;
    dynamic_string m_direction;
    dynamic_string m_semantic; // array, reference, value
    dynamic_string m_array_size;
    bool m_retained;

    gl_function_param()
        : m_retained(false)
    {
    }

    void clear()
    {
        m_name.clear();
        m_type.clear();
        m_ctype.clear();
        m_ctype_enum.clear();
        m_direction.clear();
        m_semantic.clear();
        m_array_size.clear();
        m_retained = false;
    }

    void print(FILE *pFile) const
    {
        vogl_fprintf(pFile, "Name: \"%s\", Type: \"%s\", Direction: \"%s\", Semantic: \"%s\", ArraySize: \"%s\", Retained: %u\n", m_name.get_ptr(), m_type.get_ptr(), m_direction.get_ptr(), m_semantic.get_ptr(), m_array_size.get_ptr(), m_retained);
    }
};

//-----------------------------------------------------------------------------------------------------------------------
// struct gl_function_def
//-----------------------------------------------------------------------------------------------------------------------
struct gl_function_def
{
    dynamic_string m_name;      // func name from spec file, without the gl/glx/cgl/wgl/etc. prefix
    dynamic_string m_full_name; // full func name, with prefix
    vogl::vector<dynamic_string> m_param_names;

    dynamic_string m_return;
    dynamic_string m_return_ctype;
    dynamic_string m_return_ctype_enum;

    vogl::vector<gl_function_param> m_params;
    dynamic_string m_category;
    dynamic_string m_version;
    dynamic_string m_profile;
    dynamic_string m_deprecated;
    gl_lib_t m_lib;
    bool m_notlistable;

    gl_function_def()
    {
        clear();
    }

    void clear()
    {
        m_name.clear();
        m_param_names.clear();
        m_return.clear();
        m_return_ctype.clear();
        m_return_ctype_enum.clear();
        m_params.clear();
        m_category.clear();
        m_version.clear();
        m_profile.clear();
        m_deprecated.clear();
        m_lib = cGL;
        m_notlistable = false;
    }

    void print(FILE *pFile) const
    {
        vogl_fprintf(pFile, "Name: \"%s\", ParamNames: %u, Params: %u, Return: \"%s\", Category: \"%s\", Version: \"%s\", Profile: \"%s\", Deprecated: \"%s\", Lib: \"%s\" notlistable: %u\n",
                    m_name.get_ptr(),
                    m_param_names.size(),
                    m_params.size(),
                    m_return.get_ptr(),
                    m_category.get_ptr(),
                    m_version.get_ptr(),
                    m_profile.get_ptr(),
                    m_deprecated.get_ptr(),
                    g_lib_api_prefixes[m_lib],
                    m_notlistable);

        if (m_params.size())
        {
            for (uint32_t i = 0; i < m_params.size(); i++)
            {
                vogl_fprintf(pFile, "  ");
                m_params[i].print(pFile);
            }
            vogl_fprintf(pFile, "\n");
        }
    }

    dynamic_string get_param_proto() const
    {
        dynamic_string proto;
        for (uint32_t param_index = 0; param_index < m_params.size(); param_index++)
        {
            const gl_function_param &param = m_params[param_index];

            proto.format_append("%s%s%s", param.m_ctype.get_ptr(), param.m_ctype.ends_with("*") ? "" : " ", param.m_name.get_ptr());
            if (param_index != m_params.size() - 1)
                proto.format_append(", ");
        }
        return proto;
    }

    dynamic_string get_param_args() const
    {
        dynamic_string proto;
        for (uint32_t param_index = 0; param_index < m_params.size(); param_index++)
        {
            proto.format_append("%s", m_params[param_index].m_name.get_ptr());
            if (param_index != m_params.size() - 1)
                proto.format_append(", ");
        }
        return proto;
    }

    const char *get_lib_name() const
    {
        return g_lib_api_prefixes[m_lib];
    }
};

typedef vogl::vector<gl_function_def> gl_function_def_vec;

//-----------------------------------------------------------------------------------------------------------------------
// struct gl_string_key_comparer
//-----------------------------------------------------------------------------------------------------------------------
struct gl_string_key_comparer
{
    bool operator()(const dynamic_string &lhs, const dynamic_string &rhs) const
    {
        return lhs.compare(rhs, true) < 0;
    }
};

typedef std::map<dynamic_string, dynamic_string, gl_string_key_comparer> gl_string_map;
typedef std::set<dynamic_string, gl_string_key_comparer> gl_string_set;

//-----------------------------------------------------------------------------------------------------------------------
// class gl_function_specs
//-----------------------------------------------------------------------------------------------------------------------
class gl_function_specs
{
public:
    gl_function_specs()
    {
    }

    void clear()
    {
        m_funcs.clear();
    }

    gl_function_specs &operator+=(const gl_function_specs &other)
    {
        m_funcs.append(other.m_funcs);
        return *this;
    }

    uint32_t size() const
    {
        return m_funcs.size();
    }

    const gl_function_def &operator[](uint32_t i) const
    {
        return m_funcs[i];
    }

    gl_function_def &operator[](uint32_t i)
    {
        return m_funcs[i];
    }

    const gl_function_def_vec &get_funcs_vec() const
    {
        return m_funcs;
    }

    gl_function_def_vec &get_funcs_vec()
    {
        return m_funcs;
    }

    int find_index(const char *pName) const
    {
        for (uint32_t i = 0; i < m_funcs.size(); i++)
            if (m_funcs[i].m_name.compare(pName, true) == 0)
                return i;

        return -1;
    }

    gl_function_def *find(const char *pName)
    {
        for (uint32_t i = 0; i < m_funcs.size(); i++)
            if (m_funcs[i].m_name.compare(pName, true) == 0)
                return &m_funcs[i];

        return NULL;
    }

    const gl_function_def *find(const char *pName) const
    {
        for (uint32_t i = 0; i < m_funcs.size(); i++)
            if (m_funcs[i].m_name.compare(pName, true) == 0)
                return &m_funcs[i];

        return NULL;
    }

    bool exists(const char *pName) const
    {
        return find(pName) != NULL;
    }

    void dump_to_file(const char *pFilename) const
    {
        FILE *pFile = vogl_fopen(pFilename, "w");
        if (pFile)
        {
            vogl_printf("--- Dumping %u GL function specs to file \"%s\"\n", m_funcs.size(), pFilename);

            vogl_fprintf(pFile, "Functions:\n");
            for (uint32_t i = 0; i < m_funcs.size(); i++)
            {
                m_funcs[i].print(pFile);
            }

            vogl_fprintf(pFile, "Categories:\n");

            gl_string_set categories;
            for (uint32_t i = 0; i < m_funcs.size(); i++)
                categories.insert(m_funcs[i].m_category);

            for (gl_string_set::const_iterator it = categories.begin(); it != categories.end(); ++it)
                vogl_fprintf(pFile, "\"%s\"\n", it->get_ptr());

            vogl_fclose(pFile);
        }
    }

    bool parse_spec_file(const char *pSpec_filename, gl_lib_t default_gl_lib_type)
    {
        cfile_stream spec_file(pSpec_filename, cDataStreamReadable);
        if (!spec_file.is_opened())
        {
            vogl_error_printf("Unable to open file \"%s\"!\n", pSpec_filename);
            return false;
        }

        dynamic_string cur_function_def;
        vogl::vector<dynamic_string> cur_function_param_names;
        gl_function_def *pCur_func_def = NULL;

        uint32_t max_num_params = 0;
        uint32_t total_functions = 0;

        dynamic_string line_str;
        while (spec_file.get_remaining())
        {
            if (!spec_file.read_line(line_str))
                break;

            line_str.trim_end();

            VOGL_DEBUG_BREAK_ON_FUNCTION(line_str, gDebugFunctionName);

            int comment_ofs = line_str.find_left('#');
            if (comment_ofs >= 0)
            {
                line_str.truncate(comment_ofs);
                line_str.trim_end();
            }

            if (line_str.is_empty())
                continue;

            if (line_str.find_left(':') >= 0)
            {
                cur_function_def.clear();
                pCur_func_def = NULL;
                cur_function_param_names.clear();

                continue;
            }

            //printf("%s\n", line_str.get_ptr());

            int paren_start = line_str.find_left('(');
            int paren_end = line_str.find_right(')');
            bool is_func_def = !vogl_isspace(line_str[0]) && ((paren_start >= 0) && (paren_end >= 0) && (paren_end > paren_start));

            if (is_func_def)
            {
                cur_function_def.set(line_str).left(paren_start).trim();
                if (cur_function_def.is_empty())
                {
                    vogl_error_printf("Skipping unrecognized line: %s\n", line_str.get_ptr());
                    continue;
                }
                cur_function_def = cur_function_def;

                dynamic_string param_names(line_str);
                param_names.mid(paren_start + 1, paren_end - paren_start - 1).trim();

                cur_function_param_names.resize(0);

                int cur_ofs = 0;
                while (cur_ofs < (int)param_names.get_len())
                {
                    int next_comma_ofs = param_names.find_left(',', cur_ofs);

                    dynamic_string cur_param_name(param_names);
                    if (next_comma_ofs < 0)
                    {
                        cur_param_name.right(cur_ofs).trim();
                        cur_ofs = param_names.get_len();
                    }
                    else
                    {
                        cur_param_name.mid(cur_ofs, next_comma_ofs - cur_ofs).trim();
                        cur_ofs = next_comma_ofs + 1;
                    }

                    if (!cur_param_name.is_empty())
                    {
                        cur_function_param_names.push_back(cur_param_name);
                    }
                    else
                    {
                        vogl_error_printf("Empty parameter name: %s\n", line_str.get_ptr());
                    }
                }

#if 0
				printf("Function: \"%s\", Params %u:\n", cur_function_def.get_ptr(), cur_function_param_names.size());
				for (uint32_t i = 0; i < cur_function_param_names.size(); i++)
				{
					printf("  %s\n", cur_function_param_names[i].get_ptr());
				}
				printf("\n");
#endif

                max_num_params = math::maximum(max_num_params, cur_function_param_names.size());
                total_functions++;

                pCur_func_def = m_funcs.enlarge(1);
                pCur_func_def->m_name = cur_function_def;
                pCur_func_def->m_param_names = cur_function_param_names;
                pCur_func_def->m_lib = default_gl_lib_type;
            }
            else if ((!cur_function_def.is_empty()) && pCur_func_def && vogl_isspace(line_str[0]))
            {
                vogl::vector<dynamic_string> func_attribute_tokens;
                line_str.tokenize(" \t", func_attribute_tokens);

                if (func_attribute_tokens.is_empty())
                {
                    vogl_error_printf("Skipping unrecognized line: %s\n", line_str.get_ptr());
                }
                else
                {
                    if (func_attribute_tokens[0] == "return")
                    {
                        if (func_attribute_tokens.size() != 2)
                        {
                            vogl_error_printf("Expected return type on line: %s\n", line_str.get_ptr());
                            return false;
                        }
                        pCur_func_def->m_return = func_attribute_tokens[1];
                    }
                    else if (func_attribute_tokens[0] == "param")
                    {
                        if ((func_attribute_tokens.size() < 5) || (func_attribute_tokens.size() > 7))
                        {
                            vogl_error_printf("Unexpected number of param tokens on line: %s\n", line_str.get_ptr());
                            return false;
                        }

                        if ((func_attribute_tokens[3] != "in") && (func_attribute_tokens[3] != "out"))
                        {
                            vogl_error_printf("Unexpected param direction on line: %s\n", line_str.get_ptr());
                            return false;
                        }

                        if (func_attribute_tokens[4].find_left("array[") == 0)
                        {
                            // special case for glx.spec
                            int open_bracket = func_attribute_tokens[4].find_left('[');
                            int close_bracket = func_attribute_tokens[4].find_left(']');
                            if (open_bracket < close_bracket)
                            {
                                func_attribute_tokens.push_back(dynamic_string(func_attribute_tokens[4]));
                                func_attribute_tokens.back().right(open_bracket);
                                func_attribute_tokens[4].truncate(open_bracket);
                            }
                        }

                        if ((func_attribute_tokens[4] != "value") && (func_attribute_tokens[4] != "array") && (func_attribute_tokens[4] != "reference"))
                        {
                            vogl_error_printf("Unexpected param semantic on line: %s\n", line_str.get_ptr());
                            return false;
                        }

                        gl_function_param *pParam = pCur_func_def->m_params.enlarge(1);

                        if (func_attribute_tokens.back() == "retained")
                        {
                            pParam->m_retained = true;
                            func_attribute_tokens.resize(func_attribute_tokens.size() - 1);
                        }

                        //param		pname		GetTextureParameter in value
                        //param		params		Float32 out array [COMPSIZE(pname)]
                        pParam->m_name = func_attribute_tokens[1];
                        pParam->m_type = func_attribute_tokens[2];
                        pParam->m_direction = func_attribute_tokens[3];
                        pParam->m_semantic = func_attribute_tokens[4];
                        if (pParam->m_semantic == "array")
                        {
                            if (func_attribute_tokens.size() > 5) 
                            {
                                if ((func_attribute_tokens[5][0] != '[') || (func_attribute_tokens[5].back() != ']'))
                                {
                                    vogl_error_printf("Unexpected array size on line: %s\n", line_str.get_ptr());
                                    return false;
                                }


                                pParam->m_array_size = func_attribute_tokens[5];
                                for (;;)
                                {
                                    int slash_ofs = pParam->m_array_size.find_left('/');
                                    if (slash_ofs < 0)
                                        break;
                                    pParam->m_array_size.set_char(slash_ofs, ',');
                                }
                            }
                            else
                            {
                                pParam->m_array_size.set("");
                            }
                        }
                        else if (func_attribute_tokens.size() != 5)
                        {
                            vogl_error_printf("Unexpected number of tokens on line: %s\n", line_str.get_ptr());
                            return false;
                        }
                    }
                    else if (func_attribute_tokens[0] == "category")
                    {
                        pCur_func_def->m_category = func_attribute_tokens[1];

                        if (pCur_func_def->m_category == "gl")
                            pCur_func_def->m_lib = cGL;
                        else if (pCur_func_def->m_category == "glX")
                            pCur_func_def->m_lib = cGLX;
                        else if (pCur_func_def->m_category == "CGL")
                            pCur_func_def->m_lib = cCGL;
                        else if (pCur_func_def->m_category == "wgl")
                            pCur_func_def->m_lib = cWGL;
                        else if (pCur_func_def->m_category == "glu")
                            pCur_func_def->m_lib = cGLU;
                        else
                        {
                            // lib is default_gl_lib_type
                        }
                    }
                    else if (func_attribute_tokens[0] == "version")
                    {
                        if (func_attribute_tokens.size() != 2)
                        {
                            vogl_error_printf("Invalid version param on line: %s\n", line_str.get_ptr());
                            return false;
                        }

                        pCur_func_def->m_version = func_attribute_tokens[1];
                    }
                    else if (func_attribute_tokens[0] == "profile")
                    {
                        if (func_attribute_tokens.size() != 2)
                        {
                            vogl_error_printf("Invalid profile param on line: %s\n", line_str.get_ptr());
                            return false;
                        }

                        pCur_func_def->m_profile = func_attribute_tokens[1];
                    }
                    else if (func_attribute_tokens[0] == "deprecated")
                    {
                        if (func_attribute_tokens.size() != 2)
                        {
                            vogl_error_printf("Invalid deprecated param on line: %s\n", line_str.get_ptr());
                            return false;
                        }

                        pCur_func_def->m_deprecated = func_attribute_tokens[1];
                    }
                    else if (func_attribute_tokens[0] == "dlflags")
                    {
                        if (func_attribute_tokens.size() < 2)
                        {
                            vogl_error_printf("Empty dlflags on line: %s. What?\n", line_str.get_ptr());
                            return false;
                        }

                        for (uint32_t i = 1; i < func_attribute_tokens.size(); ++i) 
                        {
                            if ((func_attribute_tokens[i] != "notlistable") 
                                && (func_attribute_tokens[i] != "handcode") 
                                && (func_attribute_tokens[i] != "prepad")
                                && (func_attribute_tokens[i] != "nop"))
                            {
                                vogl_error_printf("Unrecognized dlflags (%s) on line: %s\n", func_attribute_tokens[i].get_ptr(), line_str.get_ptr());
                                return false;
                            }

                            pCur_func_def->m_notlistable = pCur_func_def->m_notlistable || (func_attribute_tokens[i] == "notlistable");
                        }
                    }
                }
            }
            else
            {
                vogl_error_printf("Skipping unrecognized line: %s\n", line_str.get_ptr());
            }
        }

        for (uint32_t i = 0; i < m_funcs.size(); i++)
        {
            m_funcs[i].m_full_name.format("%s%s", g_lib_api_prefixes[m_funcs[i].m_lib], m_funcs[i].m_name.get_ptr());
        }

        return true;
    }

    bool parse_gl_xml_file(const char *pFilename)
    {
        tinyxml2::XMLDocument gl_xml;

        if (gl_xml.LoadFile(pFilename) != tinyxml2::XML_SUCCESS)
        {
            vogl_error_printf("Failed loading %s!\n", pFilename);
            return false;
        }

        const tinyxml2::XMLElement *pRoot = gl_xml.RootElement();
        const char *pName = pRoot->Attribute("name");
        if ((!pName) || (strcmp(pName, "gl")))
        {
            vogl_error_printf("Invalid root element in %s!\n", pFilename);
            return false;
        }

        const tinyxml2::XMLElement *pLibraries = pRoot->FirstChildElement("libraries");
        const tinyxml2::XMLElement *pExtensions = pRoot->FirstChildElement("extensions");
        if ((!pLibraries) || (!pExtensions))
        {
            vogl_error_printf("Couldn't find libraries and/or extensions elements in %s!\n", pFilename);
            return false;
        }

        const tinyxml2::XMLElement *pLibrary = pLibraries->FirstChildElement();
        while (pLibrary)
        {
            const char *pName2 = pLibrary->Attribute("name");
            if (pName2)
            {
                if (!parse_gl_xml_function_defs(pFilename, pName2, pLibrary->FirstChildElement()))
                    return false;
            }

            pLibrary = pLibrary->NextSiblingElement();
        }

        const tinyxml2::XMLElement *pCur_extension = pExtensions->FirstChildElement();
        while (pCur_extension)
        {
            const char *pExt_name = pCur_extension->Attribute("name");
            if (!pExt_name)
            {
                vogl_error_printf("Couldn't find extension name attribute in %s!\n", pFilename);
                return false;
            }

            if (!parse_gl_xml_function_defs(pFilename, pExt_name, pCur_extension->FirstChildElement()))
                return false;

            pCur_extension = pCur_extension->NextSiblingElement();
        }

        return true;
    }

protected:
    gl_function_def_vec m_funcs;

    bool parse_gl_xml_function_defs(const char *pFilename, const char *pCategory, const tinyxml2::XMLElement *pFunctions)
    {
        while (pFunctions)
        {
            if ((pFunctions->Value()) && (strcmp(pFunctions->Value(), "functions") == 0))
            {
                const tinyxml2::XMLElement *pFunction = pFunctions->FirstChildElement();
                while (pFunction)
                {
                    if ((pFunction->Value()) && (strcmp(pFunction->Value(), "function") == 0))
                    {
                        const char *pType = pFunction->Attribute("type");
                        const char *pName = pFunction->Attribute("name");
                        if ((!pType) || (!pName))
                        {
                            vogl_error_printf("Expected function name and type in file %s\n", pFilename);
                            return false;
                        }

                        gl_function_def gl_func;

                        gl_func.clear();
                        gl_func.m_full_name = pName;
                        gl_func.m_name = pName;

                        if (gl_func.m_name.begins_with("glX", true))
                        {
                            gl_func.m_name.right(3);
                            gl_func.m_lib = cGLX;
                        }
                        else if (gl_func.m_name.begins_with("glu", true))
                        {
                            gl_func.m_name.right(3);
                            gl_func.m_lib = cGLU;
                        }
                        else if (gl_func.m_name.begins_with("CGL", true))
                        {
                            gl_func.m_name.right(3);
                            gl_func.m_lib = cCGL;
                        }
                        else if (gl_func.m_name.begins_with("wgl", true))
                        {
                            gl_func.m_name.right(3);
                            gl_func.m_lib = cWGL;
                        }
                        else if (gl_func.m_name.begins_with("gl", true))
                        {
                            gl_func.m_name.right(2);
                            gl_func.m_lib = cGL;
                        }
                        else
                        {
                            // vogl_warning_printf("Unknown function prefix from XML, assuming \"gl\": %s\n", gl_func.m_name.get_ptr());
                            gl_func.m_lib = cGL;
                            gl_func.m_full_name.format("%s%s", "gl", pName);
                        }

                        gl_func.m_category = pCategory;
                        gl_func.m_return = pType;

                        const tinyxml2::XMLElement *pParams = pFunction->FirstChildElement();
                        while (pParams)
                        {
                            if (strcmp(pParams->Value(), "param") == 0)
                            {
                                const char *pParam_type = pParams->Attribute("type");
                                const char *pParam_name = pParams->Attribute("name");
                                if ((!pParam_type) || (!pParam_name))
                                {
                                    vogl_error_printf("Expected parameter type and name in file %s\n", pFilename);
                                    return false;
                                }

                                gl_function_param param;
                                param.clear();
                                param.m_name = pParam_name;
                                param.m_type = pParam_type;
                                gl_func.m_params.push_back(param);
                            }
                            pParams = pParams->NextSiblingElement();
                        }

                        if ((gl_func.m_params.size() == 1) && (gl_func.m_params[0].m_type == "") && (gl_func.m_params[0].m_name == "void"))
                            gl_func.m_params.resize(0);

                        m_funcs.push_back(gl_func);
                    }

                    pFunction = pFunction->NextSiblingElement();
                }
            }

            pFunctions = pFunctions->NextSiblingElement();
        }

        return true;
    }
};

//-----------------------------------------------------------------------------------------------------------------------
// apitrace glapi.py parsing
//-----------------------------------------------------------------------------------------------------------------------
static const struct
{
    const char *m_pSpec_return_type;
    const char *m_pApitrace_return_type;
    vogl_namespace_t m_namespace;
} g_apitrace_return_type_aliases[] =
{
    { "void", "Void", VOGL_NAMESPACE_INVALID },
    { "GLenum", "GLenum_error", VOGL_NAMESPACE_INVALID },
    { "const GLubyte *", "String(Const(GLubyte))", VOGL_NAMESPACE_INVALID },
    { "GLuint", "Handle(\"list\", GLuint, \"range\")", VOGL_NAMESPACE_LISTS },
    { "GLint", "Alias(\"GLint\", GLenum)", VOGL_NAMESPACE_INVALID },
    { "GLvoid*", "GLmap", VOGL_NAMESPACE_INVALID },
    { "GLuint", "GLprogram", VOGL_NAMESPACE_PROGRAMS },
    { "GLuint", "GLshader", VOGL_NAMESPACE_SHADERS },
    { "GLint", "GLlocation", VOGL_NAMESPACE_LOCATIONS },
    { "GLubyte *", "String(Const(GLubyte))", VOGL_NAMESPACE_INVALID },
    { "GLint", "GLlocationARB", VOGL_NAMESPACE_LOCATIONS },
    { "GLuint", "Handle(\"fragmentShaderATI\", GLuint, \"range\")", VOGL_NAMESPACE_FRAGMENT_SHADER_ATI }
};

const uint32_t APITRACE_RETURN_TYPE_ALIASES_ARRAY_SIZE = sizeof(g_apitrace_return_type_aliases) / sizeof(g_apitrace_return_type_aliases[0]);

//-----------------------------------------------------------------------------------------------------------------------
// g_apitrace_param_type_aliases array
//-----------------------------------------------------------------------------------------------------------------------

static const struct
{
    const char *m_pSpec_type;
    const char *m_pApitrace_type;
} g_apitrace_param_type_aliases[] =
{
    { "GLbitfield", "GLbitfield_access" },
    { "GLbitfield", "GLbitfield_attrib" },
    { "GLbitfield", "GLbitfield_barrier" },
    { "GLbitfield", "GLbitfield_client_attrib" },
    { "GLbitfield", "GLbitfield_shader" },
    { "GLbitfield", "GLbitfield_sync_flush" },
    { "GLuint", "GLpipeline" },
    { "GLuint", "GLarray" },
    { "GLuint", "GLarrayAPPLE" },
    { "GLuint", "GLbuffer" },
    { "GLuint", "GLfeedback" },
    { "GLuint", "GLfence" },
    { "GLuint", "GLframebuffer" },
    { "GLuint", "GLpipeline" },
    { "GLuint", "GLprogramARB" },
    { "GLuint", "GLquery" },
    { "GLuint", "GLrenderbuffer" },
    { "GLuint", "GLsampler" },
    { "GLuint", "GLtexture" },
    { "GLuint", "GLarray" },
    { "GLuint", "GLarrayAPPLE" },
    { "GLuint", "GLbuffer" },
    { "GLuint", "GLfeedback" },
    { "GLuint", "GLfence" },
    { "GLuint", "GLfragmentShaderATI" },
    { "GLuint", "GLframebuffer" },
    { "GLuint", "GLlist" },
    { "GLuint", "GLpipeline" },
    { "GLuint", "GLprogram" },
    { "GLuint", "GLprogramARB" },
    { "GLuint", "GLquery" },
    { "GLuint", "GLrenderbuffer" },
    { "GLuint", "GLsampler" },
    { "GLuint", "GLshader" },
    { "GLuint", "GLtexture" },
    { "GLvoid *", "GLpointer" },
    { "GLvoid", "GLIndexBuffer" },
    { "GLchar *", "GLstring" },
    { "const GLvoid *", "GLpointerConst" },
    { "GLhalf", "GLhalfNV" },
    { "const GLchar *", "GLstringConst" },
    { "const GLchar *", "GLstringConstARB" },
    { "GLchar *", "GLstringARB" },
    { "GLchar", "GLcharARB" },
    { "GLint", "GLlocation" },
    { "GLint", "GLlocationARB" },
    { "GLenum", "GLenum_int" },
    { "GLint", "size_bgra" },
    { "GLenum", "GLenum_mode" },
};

static const uint32_t APITRACE_PARAM_TYPE_ALIASES_ARRAY_SIZE = sizeof(g_apitrace_param_type_aliases) / sizeof(g_apitrace_param_type_aliases[0]);

//-----------------------------------------------------------------------------------------------------------------------
// struct apitrace_gl_func_param_def
//-----------------------------------------------------------------------------------------------------------------------
struct apitrace_gl_func_param_def
{
    apitrace_gl_func_param_def()
    {
        clear();
    }

    void clear()
    {
        m_name.clear();
        m_type.clear();
        m_gl_type.clear();

        m_array_count_str.clear();
        m_index_count_str.clear();
        m_string_count_str.clear();
        m_index_type_str.clear();

        m_is_out = false;
        m_is_pointer = false;
        m_is_const = false;
        m_is_opaque = false;
        m_is_array = false;
        m_is_blob = false;
        m_is_index_buffer = false;
        m_is_string = false;
        m_is_opaque_ctype = false;
    }

    dynamic_string m_name;
    dynamic_string m_type;
    dynamic_string m_gl_type; // actual C type (but without const stuff)
    vogl_namespace_t m_namespace;

    dynamic_string m_array_count_str;
    dynamic_string m_index_count_str;
    dynamic_string m_string_count_str;
    dynamic_string m_index_type_str;

    bool m_is_out;
    bool m_is_pointer;
    bool m_is_const;
    bool m_is_opaque;
    bool m_is_array;
    bool m_is_blob;
    bool m_is_index_buffer;
    bool m_is_string;
    bool m_is_opaque_ctype;
};

typedef vogl::vector<apitrace_gl_func_param_def> apitrace_gl_func_param_def_array;

//-----------------------------------------------------------------------------------------------------------------------
// struct apitrace_gl_func_def
//-----------------------------------------------------------------------------------------------------------------------
struct apitrace_gl_func_def
{
    apitrace_gl_func_def()
    {
        clear();
    }

    void clear()
    {
        m_name.clear();
        m_return_type.clear();
        m_return_gl_type.clear();
        m_has_side_effects = false;
        m_params.clear();
    }

    dynamic_string m_name;
    dynamic_string m_return_type;
    dynamic_string m_return_gl_type; // actual C type (but without const stuff)
    vogl_namespace_t m_return_namespace;

    bool m_has_side_effects;
    apitrace_gl_func_param_def_array m_params;
};

typedef vogl::vector<apitrace_gl_func_def> apitrace_gl_func_def_array;

//-----------------------------------------------------------------------------------------------------------------------
// class apitrace_func_specs
//-----------------------------------------------------------------------------------------------------------------------

//#define APITRACE_FUNC_SPECS_DEBUG

class apitrace_func_specs
{
public:
    apitrace_func_specs()
    {
    }

    void clear()
    {
        m_funcs.clear();
    }

    uint32_t size() const
    {
        return m_funcs.size();
    }

    const apitrace_gl_func_def &operator[](uint32_t i) const
    {
        return m_funcs[i];
    }

    apitrace_gl_func_def &operator[](uint32_t i)
    {
        return m_funcs[i];
    }

    const apitrace_gl_func_def_array &get_funcs_vec() const
    {
        return m_funcs;
    }

    apitrace_gl_func_def_array &get_funcs_vec()
    {
        return m_funcs;
    }

    int find_index(const char *pName) const
    {
        for (uint32_t i = 0; i < m_funcs.size(); i++)
            if (m_funcs[i].m_name.compare(pName, true) == 0)
                return i;

        return -1;
    }

    const apitrace_gl_func_def *find(const char *pName) const
    {
        for (uint32_t i = 0; i < m_funcs.size(); i++)
            if (m_funcs[i].m_name.compare(pName, true) == 0)
                return &m_funcs[i];

        return NULL;
    }

    bool exists(const char *pName) const
    {
        return find(pName) != NULL;
    }

    // Yes this guy is ugly, but it's simple and works on the unmodified Python source glapi.py from apitrace.
    // glapi.py was automatically generated, then hand edited. It contains a lot of useful type and array size info, but is still
    // incomplete (see glReadPixels() for example, which is missing its array size).
    bool parse(const char *pFilename = "apitrace_gl_param_info.txt")
    {
        clear();

        dynamic_string_array param_info_file;
        if (!file_utils::read_text_file(pFilename, param_info_file, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFIgnoreCommentedLines | file_utils::cRTFPrintErrorMessages))
            return false;

        dynamic_string_array unique_types;
        dynamic_string_array unique_array_counts;

        for (uint32_t line_index = 0; line_index < param_info_file.size(); line_index++)
        {
            const dynamic_string &orig_line(param_info_file[line_index]);
            dynamic_string line(orig_line);

            if (line.begins_with("#"))
                continue;

            int comment_ofs = line.find_right('#');
            if (comment_ofs >= 0)
                line.left(comment_ofs).trim();

            if (!line.begins_with("GlFunction("))
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            line.replace("GlFunction(", "", true, NULL, 1);

            if (line.ends_with(","))
                line.shorten(1);

            if (!line.ends_with(")"))
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            dynamic_string return_type;

            int first_comma = line.find_left(',');
            if (first_comma < 0)
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            int first_paren = line.find_left('(');
            if ((first_paren >= 0) && (first_paren < first_comma))
            {
                int matching_paren = find_matching_paren(line, first_paren);
                if (matching_paren < 0)
                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                return_type = dynamic_string(line).left(matching_paren + 1);
                line.right(matching_paren + 1).trim();
                if (!line.begins_with(","))
                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                line.right(1).trim();
            }
            else
            {
                return_type = dynamic_string(line).left(first_comma);
                line.right(first_comma + 1).trim();
            }

            if (!line.begins_with("\""))
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            int end_quote_ofs = line.find_left('\"', 1);
            if (end_quote_ofs < 0)
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            dynamic_string func_name(line);
            func_name.substring(1, end_quote_ofs);

            line.right(end_quote_ofs + 1).trim();
            if (!line.begins_with(","))
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);
            line.right(1).trim();

            if (!line.begins_with("["))
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            int params_end = find_matching_paren(line, 0, '[', ']');
            if (params_end < 0)
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            dynamic_string params(line);
            params.substring(1, params_end);

            line.right(params_end + 1).trim();

            if ((!line.begins_with(",") && !line.begins_with(")")) || !line.ends_with(")"))
                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

            bool func_has_side_effects = true;
            if (line.contains("sideeffects=False"))
                func_has_side_effects = false;

            apitrace_gl_func_def func_def;
            func_def.m_name = func_name;
            func_def.m_has_side_effects = func_has_side_effects;
            func_def.m_return_type = return_type;

            func_def.m_return_gl_type = return_type;

            uint32_t a;
            for (a = 0; a < APITRACE_RETURN_TYPE_ALIASES_ARRAY_SIZE; a++)
            {
                if (func_def.m_return_gl_type == g_apitrace_return_type_aliases[a].m_pApitrace_return_type)
                {
                    func_def.m_return_gl_type = g_apitrace_return_type_aliases[a].m_pSpec_return_type;
                    func_def.m_return_namespace = g_apitrace_return_type_aliases[a].m_namespace;
                    break;
                }
            }
            if (a == APITRACE_RETURN_TYPE_ALIASES_ARRAY_SIZE)
            {
                func_def.m_return_namespace = vogl_find_namespace_from_gl_type(func_def.m_return_gl_type.get_ptr());
                if (func_def.m_return_namespace == VOGL_NAMESPACE_INVALID)
                {
// TODO: Here's the list of return/param types that can't be mapped right now - make exception list?
#if 0
					size_bgra
					const GLboolean *
					Void
					GLvoid
					GLushort
					GLuint64EXT
					GLuint64
					GLuint
					GLubyte
					GLstringConstARB
					GLstringConst
					GLstringARB
					GLstring
					GLsizeiptrARB
					GLsizeiptr
					GLsizei
					GLshort
					GLregion
					GLpointerConst
					GLpointer
					GLintptrARB
					GLintptr
					GLint64EXT
					GLint64
					GLint
					GLhalfNV
					GLfloat
					GLenum_mode
					GLenum_int
					GLenum
					GLdouble
					GLclampf
					GLclampd
					GLcharARB
					GLchar
					GLbyte
					GLboolean
					GLbitfield_sync_flush
					GLbitfield_shader
					GLbitfield_client_attrib
					GLbitfield_barrier
					GLbitfield_attrib
					GLbitfield_access
					GLbitfield
					GLIndexBuffer
					GLDEBUGPROCARB
					GLDEBUGPROCAMD
					GLDEBUGPROC
#endif

                    // vogl_warning_printf("Couldn't map namespace %s\n", func_def.m_return_gl_type.get_ptr());
                }
            }

#ifdef APITRACE_FUNC_SPECS_DEBUG
            printf("--- Func: %s\nReturn Type: %s, HasSideEffects: %u, Namespace: %s\n", func_name.get_ptr(), return_type.get_ptr(), func_has_side_effects, vogl_get_namespace_name(func_def.m_return_namespace));
#endif

            for (;;)
            {
                bool is_out = false;
                bool is_pointer = false;
                bool is_const = false;
                bool is_opaque = false;
                bool is_array = false;
                bool is_blob = false;
                bool is_index_buffer = false;
                bool is_string = false;
                bool is_opaque_ctype = false;
                dynamic_string array_count_str;
                dynamic_string index_count_str;
                dynamic_string string_count_str;
                dynamic_string index_type_str;

                if (params.is_empty())
                    break;

                if (params.begins_with("Out("))
                    is_out = true;
                else if (!params.begins_with("("))
                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                int first_paren2 = params.find_left("(");
                if (first_paren2 < 0)
                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                int param_end_ofs = find_matching_paren(params, first_paren2);
                if (param_end_ofs < 0)
                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                dynamic_string cur_param;
                cur_param = params;
                cur_param.substring(first_paren2 + 1, param_end_ofs);

                params.right(param_end_ofs + 1).trim();
                if (params.begins_with(","))
                    params.right(1).trim();

                int right_comma = cur_param.find_right(",");
                if (right_comma < 0)
                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                dynamic_string param_name(cur_param);
                param_name.right(right_comma + 1).trim();
                cur_param.left(right_comma).trim();

                int paren_ofs;
                while ((paren_ofs = cur_param.find_left('(')) >= 0)
                {
                    bool get_count_string = false;
                    bool count_string_is_optional = false;
                    dynamic_string *pCount_str = &array_count_str;

                    if (cur_param.begins_with("Out("))
                        is_out = true;
                    else if (cur_param.begins_with("Pointer("))
                        is_pointer = true;
                    else if (cur_param.begins_with("OpaquePointer("))
                    {
                        is_opaque = true;
                        is_pointer = true;
                    }
                    else if (cur_param.begins_with("Const("))
                        is_const = true;
                    else if (cur_param.begins_with("OpaqueBlob("))
                    {
                        is_opaque = true;
                        is_blob = true;
                        get_count_string = true;
                    }
                    else if (cur_param.begins_with("Blob("))
                    {
                        is_blob = true;
                        get_count_string = true;
                    }
                    else if (cur_param.begins_with("GLIndexBuffer("))
                    {
                        is_index_buffer = true;
                        get_count_string = true;
                        pCount_str = &index_count_str;
                    }
                    else if (cur_param.begins_with("OpaqueArray("))
                    {
                        is_opaque = true;
                        is_array = true;
                        get_count_string = true;
                    }
                    else if (cur_param.begins_with("Array("))
                    {
                        is_array = true;
                        get_count_string = true;
                    }
                    else if (cur_param.begins_with("String("))
                    {
                        is_string = true;
                        get_count_string = true;
                        count_string_is_optional = true;
                        pCount_str = &string_count_str;
                    }
                    else if (cur_param.begins_with("Opaque("))
                    {
                        is_opaque_ctype = true;
                    }
                    else
                        return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                    int end_paren_ofs = find_matching_paren(cur_param, paren_ofs);
                    if (end_paren_ofs < 0)
                        return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                    cur_param.substring(paren_ofs + 1, end_paren_ofs);

                    if (get_count_string)
                    {
                        if (cur_param.ends_with("\""))
                        {
                            int start_of_count = cur_param.find_right('\"', cur_param.get_len() - 2);
                            if (start_of_count < 0)
                                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                            if (!pCount_str->is_empty())
                                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                            *pCount_str = cur_param;
                            pCount_str->right(start_of_count).trim();

                            cur_param.left(start_of_count).trim();
                            if (!cur_param.ends_with(","))
                                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                            cur_param.shorten(1).trim();
                        }
                        else
                        {
                            int comma_ofs = cur_param.find_right(',');
                            if ((comma_ofs < 0) && (!count_string_is_optional))
                                return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                            if (comma_ofs >= 0)
                            {
                                if (!pCount_str->is_empty())
                                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);
                                *pCount_str = cur_param;
                                pCount_str->right(comma_ofs + 1).trim();

                                cur_param.left(comma_ofs).trim();
                            }
                        }
                    }

                    if (is_index_buffer)
                    {
                        index_type_str = *pCount_str;
                        *pCount_str = cur_param;
                        cur_param.truncate(0);
                        if (index_type_str.contains('(') || index_type_str.contains(','))
                            return parse_apitrace_gl_param_info_error(param_info_file, line_index);
                        break;
                    }
                }

                if (is_index_buffer)
                    cur_param = "GLIndexBuffer";

                if (cur_param.is_empty())
                    return parse_apitrace_gl_param_info_error(param_info_file, line_index);

                if (unique_types.find(cur_param) < 0)
                    unique_types.push_back(cur_param);

                if (unique_array_counts.find(array_count_str) < 0)
                    unique_array_counts.push_back(array_count_str);

                apitrace_gl_func_param_def param_def;
                param_def.m_name = param_name;
                param_def.m_type = cur_param;
                param_def.m_array_count_str = array_count_str;
                param_def.m_index_count_str = index_count_str;
                param_def.m_string_count_str = string_count_str;
                param_def.m_index_type_str = index_type_str;
                param_def.m_is_out = is_out;
                param_def.m_is_pointer = is_pointer;
                param_def.m_is_const = is_const;
                param_def.m_is_opaque = is_opaque;
                param_def.m_is_array = is_array;
                param_def.m_is_blob = is_blob;
                param_def.m_is_index_buffer = is_index_buffer;
                param_def.m_is_string = is_string;
                param_def.m_is_opaque_ctype = is_opaque_ctype;

                dynamic_string gl_param_type(param_def.m_type);

                if (param_def.m_is_opaque_ctype)
                    gl_param_type.unquote();

                param_def.m_namespace = vogl_find_namespace_from_gl_type(gl_param_type.get_ptr());
                if (param_def.m_namespace == VOGL_NAMESPACE_INVALID)
                {
                    // vogl_warning_printf("Couldn't map namespace %s\n", gl_param_type.get_ptr());
                }

                for (uint32_t k = 0; k < APITRACE_PARAM_TYPE_ALIASES_ARRAY_SIZE; k++)
                {
                    if (gl_param_type == g_apitrace_param_type_aliases[k].m_pApitrace_type)
                    {
                        gl_param_type = g_apitrace_param_type_aliases[k].m_pSpec_type;
                        break;
                    }
                }

                if ((param_def.m_is_string) || (param_def.m_is_index_buffer))
                    gl_param_type.append(" *");

                if ((param_def.m_is_array) || (param_def.m_is_blob) || (param_def.m_is_pointer))
                {
                    gl_param_type.append(" *");
                }

                param_def.m_gl_type = gl_param_type;

#ifdef APITRACE_FUNC_SPECS_DEBUG
                printf("Name: %s, Type: %s, Out: %u, Pointer: %u, Const: %u, IsOpaque: %u, Array: %u, Blob: %u, IndexBuf: %u, String: %u, OpaqueCType: %u, Namespace: %s\n",
                       param_name.get_ptr(),
                       cur_param.get_ptr(),
                       is_out, is_pointer, is_const, is_opaque, is_array, is_blob, is_index_buffer, is_string, is_opaque_ctype, vogl_get_namespace_name(param_def.m_namespace));
                if (array_count_str.get_len())
                    printf("   ArrayCountStr: %s\n", array_count_str.get_ptr());
                if (index_count_str.get_len())
                    printf("   IndexCountStr: %s\n", index_count_str.get_ptr());
                if (string_count_str.get_len())
                    printf("   StringCountStr: %s\n", string_count_str.get_ptr());
                if (index_type_str.get_len())
                    printf("   IndexTypeStr: %s\n", index_type_str.get_ptr());
#endif

                func_def.m_params.push_back(param_def);
            }

            m_funcs.push_back(func_def);

#ifdef APITRACE_FUNC_SPECS_DEBUG
            printf("\n\n");
#endif
        }

#ifdef APITRACE_FUNC_SPECS_DEBUG
        printf("Unique types:\n");
        unique_types.sort();
        for (uint32_t i = 0; i < unique_types.size(); i++)
            printf("%s\n", unique_types[i].get_ptr());

        printf("\n");

        printf("Unique array counts:\n");
        unique_array_counts.sort();
        for (uint32_t i = 0; i < unique_array_counts.size(); i++)
            printf("%s\n", unique_array_counts[i].get_ptr());
#endif

        return true;
    }

private:
    apitrace_gl_func_def_array m_funcs;

    bool parse_apitrace_gl_param_info_error(const dynamic_string_array &param_info_file, uint32_t line_index)
    {
        vogl_error_printf("Unrecognized line: %s\n", param_info_file[line_index].get_ptr());
        return false;
    }

    static int find_matching_paren(const dynamic_string &str, uint32_t ofs, uint32_t open_char = '(', uint32_t close_char = ')')
    {
        bool in_quoted_string = false;
        uint32_t param_count = 0;

        while (ofs < str.get_len())
        {
            uint32_t c = str[ofs];

            if (in_quoted_string)
            {
                if (c == '"')
                {
                    in_quoted_string = false;
                }
            }
            else if (c == '"')
            {
                in_quoted_string = true;
            }
            else if (c == open_char)
                param_count++;
            else if (c == close_char)
            {
                param_count--;
                if (!param_count)
                    return ofs;
            }

            ofs++;
        }

        return -1;
    }
};

//-----------------------------------------------------------------------------------------------------------------------
// class gl_types
//-----------------------------------------------------------------------------------------------------------------------
class gl_types
{
public:
    gl_types()
    {
    }

    void clear()
    {
        m_tm.clear();
    }

    const gl_string_map &get_type_map() const
    {
        return m_tm;
    }
    gl_string_map &get_type_map()
    {
        return m_tm;
    }

    uint32_t size() const
    {
        return static_cast<uint32_t>(m_tm.size());
    }

    bool is_present(const dynamic_string &type_str) const
    {
        return m_tm.find(type_str) != m_tm.end();
        //return false;
    }

    const dynamic_string *find(const dynamic_string &type_str) const
    {
        gl_string_map::const_iterator it(m_tm.find(type_str));
        if (it == m_tm.end())
            return NULL;
        return &it->second;
    }

    bool parse_file(const char *pFilename)
    {
        cfile_stream tm_file(pFilename, cDataStreamReadable);
        if (!tm_file.is_opened())
        {
            vogl_error_printf("Unable to open file \"%s\"!\n", pFilename);
            return false;
        }

        dynamic_string line_str;
        while (tm_file.get_remaining())
        {
            if (!tm_file.read_line(line_str))
                break;

            line_str.trim_end();

            int comment_ofs = line_str.find_left('#');
            if (comment_ofs >= 0)
            {
                line_str.truncate(comment_ofs);
                line_str.trim_end();
            }

            if (line_str.is_empty())
                continue;

            // AccumOp,*,*,			    GLenum,*,*

            dynamic_string_array tokens;
            line_str.tokenize(",", tokens);
            if (tokens.size() != 6)
            {
                vogl_error_printf("Unexpected number of tokens on line: %s\n", line_str.get_ptr());
                return false;
            }

            for (uint32_t i = 0; i < tokens.size(); i++)
                tokens[i].trim();

            if ((tokens[0].is_empty()) || (tokens[1] != "*") || (tokens[2] != "*") ||
                (tokens[3].is_empty()) || (tokens[4] != "*") || (tokens[5] != "*"))
            {
                vogl_error_printf("Unexpected token on line: %s\n", line_str.get_ptr());
                return false;
            }

            if (m_tm.find(tokens[0]) != m_tm.end())
            {
                vogl_error_printf("Duplicate spec type on line: %s\n", line_str.get_ptr());
                return false;
            }

            m_tm.insert(std::make_pair(tokens[0], tokens[3]));
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------

    void dump_to_file(const char *pFilename) const
    {
        FILE *pFile = vogl_fopen(pFilename, "w");
        if (pFile)
        {
            vogl_printf("--- Dumping %u GL types to text file \"%s\"\n", static_cast<uint32_t>(m_tm.size()), pFilename);

            for (gl_string_map::const_iterator it = m_tm.begin(); it != m_tm.end(); ++it)
                vogl_fprintf(pFile, "\"%s\" = \"%s\"\n", it->first.get_ptr(), it->second.get_ptr());

            vogl_fclose(pFile);
        }
        else
        {
            vogl_error_printf("Failed writing GL types to file \"%s\"!\n", pFilename);
        }
    }

protected:
    gl_string_map m_tm;
};

//-----------------------------------------------------------------------------------------------------------------------
// class gl_enums
//-----------------------------------------------------------------------------------------------------------------------
struct gl_enum_def
{
    dynamic_string m_name;
    dynamic_string m_def;
    bool m_alias_flag;
    bool m_define_flag;
};

typedef vogl::vector<gl_enum_def> gl_enum_def_vec;

typedef std::map<dynamic_string, gl_enum_def_vec, gl_string_key_comparer> gl_enum_def_vec_map;

class gl_enums
{
public:
    gl_enums()
    {
    }

    bool parse_file(const char *pFilename)
    {
        cfile_stream enum_file(pFilename, cDataStreamReadable);
        if (!enum_file.is_opened())
        {
            vogl_error_printf("Unable to open file \"%s\"!\n", pFilename);
            return false;
        }

        vogl::vector<gl_enum_def_vec_map::iterator> cur_enums;
        bool is_define = false;
        bool seen_at_least_one_define = false;

        dynamic_string line_str;
        while (enum_file.get_remaining())
        {
            if (!enum_file.read_line(line_str))
                break;

            line_str.trim_end();

            int comment_ofs = line_str.find_left('#');
            if (comment_ofs >= 0)
            {
                line_str.truncate(comment_ofs);
                line_str.trim_end();
            }

            if (line_str.is_empty())
                continue;

            int colon_ofs = line_str.find_left(':');
            if (colon_ofs >= 0)
                line_str.left(colon_ofs + 1);

            dynamic_string_array tokens;
            line_str.tokenize(", \t", tokens);

            if ((tokens[0] == "passthru:") || (tokens[0] == "profile:") || (tokens[0].find_left("future_use:") >= 0))
                continue;

            if ((colon_ofs >= 0) && (tokens.size()) &&
                ((tokens.back() == "enum:") || (tokens.back() == "define:")))
            {
                if (seen_at_least_one_define)
                    cur_enums.resize(0);

                is_define = (tokens.back() == "define:");

                for (uint32_t i = 0; i < (tokens.size() - 1); i++)
                {
                    gl_enum_def_vec_map::iterator cur_enum(m_enums.find(tokens[i]));
                    if (cur_enum == m_enums.end())
                    {
                        cur_enum = m_enums.insert(std::make_pair(tokens[i], gl_enum_def_vec())).first;
                    }

                    cur_enums.push_back(cur_enum);
                }
            }
            else if (tokens.size())
            {
                if (line_str.find_left('=') >= 0)
                {
                    seen_at_least_one_define = true;

                    if ((tokens.size() != 3) || (tokens[1] != "="))
                    {
                        vogl_error_printf("Unrecognized line: %s\n", line_str.get_ptr());
                        return false;
                    }

                    if ((tokens[0].is_empty()) || (tokens[1].is_empty()))
                    {
                        vogl_error_printf("Unrecognized line: %s\n", line_str.get_ptr());
                        return false;
                    }

                    if (!cur_enums.size())
                    {
                        vogl_error_printf("Enum defined outside of enum block: %s\n", line_str.get_ptr());
                        return false;
                    }

                    gl_enum_def def;
                    def.m_name = tokens[0];
                    def.m_def = tokens[2];
                    def.m_alias_flag = false;
                    def.m_define_flag = is_define;

                    for (uint32_t i = 0; i < cur_enums.size(); i++)
                        cur_enums[i]->second.push_back(def);
                }
                else if (tokens[0] == "use")
                {
                    seen_at_least_one_define = true;

                    if (!cur_enums.size())
                    {
                        vogl_error_printf("Enum defined outside of enum block: %s\n", line_str.get_ptr());
                        return false;
                    }

                    gl_enum_def def;
                    def.m_name = tokens[2];
                    def.m_def = tokens[1];
                    def.m_alias_flag = true;
                    def.m_define_flag = is_define;

                    for (uint32_t i = 0; i < cur_enums.size(); i++)
                        cur_enums[i]->second.push_back(def);
                }
                else
                {
                    vogl_error_printf("Unrecognized line: %s\n", line_str.get_ptr());
                    return false;
                }
            }
            else
            {
                vogl_error_printf("Unrecognized line: %s\n", line_str.get_ptr());
                return false;
            }
        }

        for (gl_enum_def_vec_map::iterator enum_it = m_enums.begin(); enum_it != m_enums.end(); ++enum_it)
        {
            //printf("%s\n", enum_it->first.get_ptr());
            gl_enum_def_vec_map::const_iterator alias_it(m_enums.find(enum_it->first));
            VOGL_ASSERT(alias_it != m_enums.end());
        }

        uint32_t num_unresolved_aliases;
        uint32_t pass_num = 0;
        do
        {
            if (++pass_num == 100)
            {
                vogl_error_printf("Typemap alias depth is too deep, or there are circular aliases\n");
                return false;
            }

            num_unresolved_aliases = 0;

            for (gl_enum_def_vec_map::iterator enum_it = m_enums.begin(); enum_it != m_enums.end(); ++enum_it)
            {
                gl_enum_def_vec &def_vec = enum_it->second;
                for (uint32_t i = 0; i < def_vec.size(); i++)
                {
                    if (!def_vec[i].m_alias_flag)
                        continue;

                    gl_enum_def_vec_map::const_iterator alias_it(m_enums.find(def_vec[i].m_def));
                    if (alias_it == m_enums.end())
                    {
                        vogl_error_printf("Bad alias enum: %s %s\n", def_vec[i].m_def.get_ptr(), def_vec[i].m_name.get_ptr());
                        return false;
                    }

                    const gl_enum_def_vec &alias_def_vec = alias_it->second;

                    uint32_t j;
                    for (j = 0; j < alias_def_vec.size(); j++)
                    {
                        if (alias_def_vec[j].m_name == def_vec[i].m_name)
                            break;
                    }

                    if (j == alias_def_vec.size())
                    {
                        vogl_error_printf("Bad alias enum: %s %s\n", def_vec[i].m_def.get_ptr(), def_vec[i].m_name.get_ptr());
                        return false;
                    }
                    if (alias_def_vec[j].m_alias_flag)
                    {
                        num_unresolved_aliases++;
                        continue;
                    }

                    def_vec[i].m_def = alias_def_vec[j].m_def;
                    def_vec[i].m_alias_flag = false;
                }
            }
            //printf(".");
        } while (num_unresolved_aliases);
        //printf("\n");

        return true;
    }

    void dump_to_text_file(const char *pFilename) const
    {
        FILE *pFile = vogl_fopen(pFilename, "w");
        if (pFile)
        {
            vogl_printf("--- Dumping %u GL enums to file %s\n", static_cast<uint32_t>(m_enums.size()), pFilename);

            for (gl_enum_def_vec_map::const_iterator enum_it = m_enums.begin(); enum_it != m_enums.end(); ++enum_it)
            {
                vogl_fprintf(pFile, "enum \"%s\"\n", enum_it->first.get_ptr());
                for (uint32_t i = 0; i < enum_it->second.size(); i++)
                    vogl_fprintf(pFile, "  \"%s\" = \"%s\"\n", enum_it->second[i].m_name.get_ptr(), enum_it->second[i].m_def.get_ptr());
            }

            vogl_fclose(pFile);
        }
        else
        {
            vogl_error_printf("Failed writing GL enums to file %s\n", pFilename);
        }
    }

    bool dump_to_definition_macro_file(const dynamic_string& outDirName, const char *pFilename, const char *pPrefix) const
    {
        gl_string_map unique_enums;
        for (gl_enum_def_vec_map::const_iterator enum_it = m_enums.begin(); enum_it != m_enums.end(); ++enum_it)
        {
            for (uint32_t i = 0; i < enum_it->second.size(); i++)
                unique_enums.insert(std::make_pair(enum_it->second[i].m_name, enum_it->second[i].m_def));
        }

        FILE *pFile = fopen_and_log_generic(outDirName, pFilename, "w");
        if (!pFile)
            return false;

        if (pPrefix)
        {
            for (gl_string_map::const_iterator it = unique_enums.begin(); it != unique_enums.end(); ++it)
                vogl_fprintf(pFile, "#define %s_%s %s\n", pPrefix, it->first.get_ptr(), it->second.get_ptr());
        }
        else
        {
            for (gl_string_map::const_iterator it = unique_enums.begin(); it != unique_enums.end(); ++it)
                vogl_fprintf(pFile, "#define %s %s\n", it->first.get_ptr(), it->second.get_ptr());
        }

        vogl_fclose(pFile);
        return true;
    }

    bool dump_to_description_macro_file(const dynamic_string& outDirName, const char *pFilename, const char *pPrefix, const gl_types &gl_typemap, const gl_types &alt_gl_typemap) const
    {
        const gl_types* ptr_alt_gltypemap = &alt_gl_typemap;
        return dump_to_description_macro_file(outDirName, pFilename, pPrefix, gl_typemap, 1, &ptr_alt_gltypemap);
    }

    bool dump_to_description_macro_file(const dynamic_string& outDirName, const char *pFilename, const char *pPrefix, const gl_types &gl_typemap, int num_alts, const gl_types** alt_gl_typemaps) const
    {
        VOGL_ASSERT(num_alts == 0 || alt_gl_typemaps != NULL);
        for (int i = 0; i < num_alts; ++i) { VOGL_ASSERT(alt_gl_typemaps[i] != NULL); }

        FILE *pFile = fopen_and_log_generic(outDirName, pFilename, "w");
        if (!pFile)
            return false;

        for (gl_enum_def_vec_map::const_iterator enum_it = m_enums.begin(); enum_it != m_enums.end(); ++enum_it)
        {
            vogl_fprintf(pFile, "DEFINE_GL_ENUM_CATEGORY_BEGIN(%s)\n", enum_it->first.get_ptr());

            for (uint32_t i = 0; i < enum_it->second.size(); i++)
            {
                const dynamic_string *pGLType = NULL;
                pGLType = gl_typemap.find(enum_it->first.get_ptr());
                if (!pGLType)
                {
                    for (int t = 0; t < num_alts; ++t)
                    {
                        pGLType = alt_gl_typemaps[t]->find(enum_it->first.get_ptr());
                        if (pGLType)
                            break;
                    }
                }

                if (enum_it->second[i].m_define_flag)
                {
                    vogl_fprintf(pFile, "DEFINE_GL_DEFINE_MEMBER(%s, %s, %s_%s)\n",
                                pPrefix,
                                enum_it->first.get_ptr(),
                                pPrefix, enum_it->second[i].m_name.get_ptr());
                }
                else
                {
                    vogl_fprintf(pFile, "DEFINE_GL_ENUM_MEMBER(%s, %s, \"%s\", \"%s_%s\", %s)\n",
                                pPrefix,
                                enum_it->first.get_ptr(),
                                pGLType ? pGLType->get_ptr() : "",
                                pPrefix, enum_it->second[i].m_name.get_ptr(), enum_it->second[i].m_def.get_ptr());
                }
            }

            vogl_fprintf(pFile, "DEFINE_GL_ENUM_CATEGORY_END(%s)\n", enum_it->first.get_ptr());
        }

        vogl_fclose(pFile);

        return true;
    }

private:
    gl_enum_def_vec_map m_enums;
};

//-----------------------------------------------------------------------------------------------------------------------
// class vogl_gen
//-----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_PROTO_PARAM_STR "(a,b,c,d,e,f,g,h)"

class vogl_gen
{
    apitrace_func_specs m_apitrace_gl_func_specs;
    gl_function_specs m_gl_xml_functions;
    gl_enums m_gl_enumerations;
    gl_enums m_glx_enumerations;
    gl_enums m_glx_ext_enumerations;
    gl_enums m_cgl_enumerations;
    gl_enums m_cgl_ext_enumerations;
    gl_enums m_wgl_enumerations;
    gl_enums m_wgl_ext_enumerations;

    gl_function_specs m_gl_funcs;
    gl_function_specs m_glx_funcs;
    gl_function_specs m_glxext_funcs;
    gl_function_specs m_cgl_funcs;
    gl_function_specs m_cglext_funcs;
    gl_function_specs m_wgl_funcs;
    gl_function_specs m_wglext_funcs;

    gl_function_specs m_all_gl_funcs;

    dynamic_string_array m_unique_categories;

    gl_string_map m_unique_ctype_enums;

    gl_types m_gl_typemap;
    gl_types m_glx_typemap;
    gl_types m_cgl_typemap;
    gl_types m_wgl_typemap;

    dynamic_string_array m_gl_so_dylib_dll_function_exports;

    gl_string_set m_all_gl_ctypes;
    gl_string_set m_all_gl_categories;
    gl_string_set m_all_array_sizes;

    gl_string_map m_pointee_types;

    dynamic_string_array m_whitelisted_funcs;
    dynamic_string_array m_whitelisted_displaylist_funcs;
    dynamic_string_array m_nullable_funcs;

    dynamic_string_array m_simple_replay_funcs;

    dynamic_string_array funcs_with_custom_array_size_macros;
    dynamic_string_array custom_return_param_array_size_macros;
    dynamic_string_array funcs_with_return_param_array_size_macros;

    gl_string_set custom_array_size_macros;
    vogl::vector<uint32_t> custom_array_size_macro_indices;
    dynamic_string_array custom_array_size_macro_names;

    dynamic_string_array gl_ext_func_deleted;
    dynamic_string_array new_ctype_from_ptr;
    dynamic_string_array simple_gl_replay_func;

    struct gl_get
    {
        gl_get()
            : m_min_vers(0xFFFF), m_max_vers(0)
        {
        }

        dynamic_string m_name;
        uint32_t m_min_vers;
        uint32_t m_max_vers;

        inline bool operator<(const gl_get &rhs) const
        {
            if (m_min_vers < rhs.m_min_vers)
                return true;
            else if (m_min_vers == rhs.m_min_vers)
            {
                if (m_max_vers < rhs.m_max_vers)
                    return true;
                else if (m_max_vers == rhs.m_max_vers)
                    return m_name < rhs.m_name;
            }
            return false;
        }
    };

    typedef vogl::hash_map<dynamic_string, gl_get> gl_get_hash_map;
    gl_get_hash_map m_gl_gets;

public:
    vogl_gen()
    {
    }

    // I would much rather this not be a class method at all, but this code base makes it a lot easier to be one. :|
    void cleanup_redundant_functions(gl_function_specs* spec_to_clean, const gl_function_specs& override_funcs, const char* func_class) const
    {
        VOGL_ASSERT(spec_to_clean != NULL);

        for (uint32_t i = 0; i < override_funcs.size(); i++)
        {
            int func_index = (*spec_to_clean).find_index(override_funcs[i].m_name.get_ptr());
            if (func_index < 0)
                continue;

            vogl_warning_printf("Extension func %s overriding %s definition\n", override_funcs[i].m_name.get_ptr(), func_class);
            (*spec_to_clean).get_funcs_vec().erase_unordered(func_index);
        }
    }

    bool init()
    {
        // TODO: I'm going to move the "glspec" dir from under bin to raddebugger/glspec (or whatever)
        dynamic_string specdir;

        if (g_command_line_params().get_value_as_string(specdir, "specdir"))
        {
            file_utils::change_directory(specdir.c_str());
            vogl_warning_printf("Changing current directory to %s\n", specdir.c_str());
        }
        else if (!file_utils::does_file_exist("gl.spec"))
        {
            if (file_utils::does_file_exist("glspec/gl.spec"))
            {
                file_utils::change_directory("glspec");
                vogl_warning_printf("Changing current directory to glspec\n");
            }
            else if (file_utils::does_file_exist("../bin/glspec/gl.spec"))
            {
                file_utils::change_directory("../bin/glspec");
                vogl_warning_printf("Changing current directory to ../bin/glspec\n");
            }
        }

        if (!file_utils::does_file_exist("gl.spec"))
        {
            vogl_error_printf("Can't find find gl.spec, which must be in the current directory!\n");
            return false;
        }

        // -- Load apitrace's GL function defintions, which was auto generated then hand-edited by the apitrace team - we ONLY use this for parameter namespace info and cross referencing (verification)
        if (!m_apitrace_gl_func_specs.parse())
        {
            vogl_error_printf("Failed parsing apitrace param info file!\n");
            return false;
        }

        // -- Load gl.xml, auto-generated from various web pages using several Python scripts
        if (!m_gl_xml_functions.parse_gl_xml_file("gl.xml"))
            return false;

        // -- Load the various GL/GLX/GLXEXT enum spec files
        m_gl_enumerations.parse_file("enum.spec");
        m_glx_enumerations.parse_file("glxenum.spec");
        m_glx_ext_enumerations.parse_file("glxenumext.spec");
        m_cgl_enumerations.parse_file("cglenum.spec");
        m_cgl_ext_enumerations.parse_file("cglenumext.spec");
        m_wgl_enumerations.parse_file("wglenum.spec");
        m_wgl_ext_enumerations.parse_file("wglenumext.spec");

        // -- Load the various GL/GLX/GLXEXT func spec files
        bool success = m_gl_funcs.parse_spec_file("gl.spec", cGL);
        if (!success)
        {
            vogl_error_printf("Failed parsing gl.spec!\n");
            return false;
        }
        success = m_glx_funcs.parse_spec_file("glx.spec", cGLX);
        if (!success)
        {
            vogl_error_printf("Failed parsing glx.spec!\n");
            return false;
        }
        success = m_glxext_funcs.parse_spec_file("glxext.spec", cGLX);
        if (!success)
        {
            vogl_error_printf("Failed parsing glxext.spec!\n");
            return false;
        }
        success = m_cgl_funcs.parse_spec_file("cgl.spec", cCGL);
        if (!success)
        {
            vogl_error_printf("Failed parsing cgl.spec!\n");
            return false;
        }
        success = m_cglext_funcs.parse_spec_file("cglext.spec", cCGL);
        if (!success)
        {
            vogl_error_printf("Failed parsing cglext.spec!\n");
            return false;
        }
        success = m_wgl_funcs.parse_spec_file("wgl.spec", cWGL);
        if (!success)
        {
            vogl_error_printf("Failed parsing wgl.spec!\n");
            return false;
        }
        success = m_wglext_funcs.parse_spec_file("wglext.spec", cWGL);
        if (!success)
        {
            vogl_error_printf("Failed parsing wglext.spec!\n");
            return false;
        }


        // -- Clean up functions that appears both as window extension funcs and either 
        // plain GLX/CGL/WGL functions.
        cleanup_redundant_functions(&m_glx_funcs, m_glxext_funcs, "glx");
        cleanup_redundant_functions(&m_gl_funcs, m_glxext_funcs, "gl");
        cleanup_redundant_functions(&m_cgl_funcs, m_cglext_funcs, "CGL");
        cleanup_redundant_functions(&m_gl_funcs, m_cglext_funcs, "gl");
        cleanup_redundant_functions(&m_wgl_funcs, m_wglext_funcs, "wgl");
        cleanup_redundant_functions(&m_gl_funcs, m_wglext_funcs, "gl");


        // Cleanup the DepthRange function. It uses parameters called 'near' and 'far', and this messes up windows 
        // because they are defined to be empty. We fix it for all platforms.
        gl_function_def* glDepthRange_func = m_gl_funcs.find("DepthRange");
        if (glDepthRange_func != NULL) {
            VOGL_VERIFY(glDepthRange_func->m_param_names.size() == 2);
            if (glDepthRange_func->m_param_names[0] == "near") {
                glDepthRange_func->m_param_names[0] = "_near";
                glDepthRange_func->m_params[0].m_name = "_near";
            }
            
            if (glDepthRange_func->m_param_names[1] == "far") {
                glDepthRange_func->m_param_names[1] = "_far";
                glDepthRange_func->m_params[1].m_name = "_far";
            }
        }
        

        for (uint32_t j = 0; j < m_glxext_funcs.size(); j++)
        {
            if (m_glxext_funcs[j].m_name.ends_with("SGIX"))
            {
                gl_ext_func_deleted.push_back(m_glxext_funcs[j].m_name);

                m_glxext_funcs.get_funcs_vec().erase_unordered(j);
                j--;
            }
        }

        for (uint32_t j = 0; j < m_cglext_funcs.size(); j++)
        {
            if (m_cglext_funcs[j].m_name.ends_with("SGIX"))
            {
                gl_ext_func_deleted.push_back(m_cglext_funcs[j].m_name);

                m_cglext_funcs.get_funcs_vec().erase_unordered(j);
                j--;
            }
        }

        for (uint32_t j = 0; j < m_wglext_funcs.size(); j++)
        {
            if (m_wglext_funcs[j].m_name.ends_with("SGIX"))
            {
                gl_ext_func_deleted.push_back(m_wglext_funcs[j].m_name);

                m_wglext_funcs.get_funcs_vec().erase_unordered(j);
                j--;
            }
        }

        // -- Parse the various spec typemap files
        success = m_gl_typemap.parse_file("gl.tm");
        if (!success)
        {
            vogl_error_printf("Failed parsing gl.tm!\n");
            return false;
        }

        success = m_glx_typemap.parse_file("glx.tm");
        if (!success)
        {
            vogl_error_printf("Failed parsing glx.tm!\n");
            return false;
        }

        success = m_cgl_typemap.parse_file("cgl.tm");
        if (!success)
        {
            vogl_error_printf("Failed parsing cgl.tm!\n");
            return false;
        }

        success = m_wgl_typemap.parse_file("wgl.tm");
        if (!success)
        {
            vogl_error_printf("Failed parsing wgl.tm!\n");
            return false;
        }

        const gl_types* glx_cgl_wgl_typemaps[] = { &m_glx_typemap, &m_cgl_typemap, &m_wgl_typemap };

        // -- Determine the ctypes used by all the GL/GLX/CGL/WGL functions
        if (!determine_ctypes("gl", m_gl_funcs, m_gl_typemap, VOGL_ARRAY_SIZE(glx_cgl_wgl_typemaps), glx_cgl_wgl_typemaps))
        {
            vogl_error_printf("Failed determined gl c types!\n");
            return false;
        }

        if (!determine_ctypes("glX", m_glx_funcs, m_glx_typemap, m_gl_typemap))
        {
            vogl_error_printf("Failed determined glX c types!\n");
            return false;
        }

        if (!determine_ctypes("glX", m_glxext_funcs, m_glx_typemap, m_gl_typemap))
        {
            vogl_error_printf("Failed determined glX c types!\n");
            return false;
        }

        if (!determine_ctypes("CGL", m_cgl_funcs, m_cgl_typemap, m_gl_typemap))
        {
            vogl_error_printf("Failed determined CGL c types!\n");
            return false;
        }

        if (!determine_ctypes("CGL", m_cglext_funcs, m_cgl_typemap, m_gl_typemap))
        {
            vogl_error_printf("Failed determined CGL c types!\n");
            return false;
        }

        if (!determine_ctypes("wgl", m_wgl_funcs, m_wgl_typemap, m_gl_typemap))
        {
            vogl_error_printf("Failed determined wgl c types!\n");
            return false;
        }

        if (!determine_ctypes("wgl", m_wglext_funcs, m_wgl_typemap, m_gl_typemap))
        {
            vogl_error_printf("Failed determined wgl c types!\n");
            return false;
        }


        // -- Create master list of all GL/GLX/GLXEXT/CGL/CGLEXT/WGL/WGLEXT functions
        m_all_gl_funcs = m_gl_funcs;
        m_all_gl_funcs += m_glx_funcs;
        m_all_gl_funcs += m_glxext_funcs;
        m_all_gl_funcs += m_cgl_funcs;
        m_all_gl_funcs += m_cglext_funcs;
        m_all_gl_funcs += m_wgl_funcs;
        m_all_gl_funcs += m_wglext_funcs;


        // -- Determine the unique list of function API categories
        for (uint32_t i = 0; i < m_all_gl_funcs.size(); i++)
        {
            if (m_unique_categories.find(m_all_gl_funcs[i].m_category) < 0)
                m_unique_categories.push_back(m_all_gl_funcs[i].m_category);
        }

        m_unique_categories.sort();

        // -- Load the GL/GLX function export table, generated by dumping the headers of various drivers
        if (!load_gl_so_function_export_list("gl_glx_so_export_list.txt", m_gl_so_dylib_dll_function_exports))
        {
            vogl_error_printf("Failed parsing gl_glx_so_export_list.txt!\n");
            return false;
        }

        // Concatenate the set of functions we will export from the dylib to the same list. 
        if (!load_gl_so_function_export_list("gl_cgl_dylib_export_list.txt", m_gl_so_dylib_dll_function_exports))
        {
            vogl_error_printf("Failed parsing gl_cgl_dylib_export_list.txt!\n");
            return false;
        }

        // Concatenate the set of functions we will export from the DLL to the same list. 
        if (!load_gl_so_function_export_list("gl_wgl_dll_export_list.txt", m_gl_so_dylib_dll_function_exports))
        {
            vogl_error_printf("Failed parsing gl_wgl_dll_export_list.txt!\n");
            return false;
        }

        process_func_protos(m_gl_funcs, m_all_gl_ctypes, m_all_gl_categories, m_all_array_sizes);
        process_func_protos(m_glx_funcs, m_all_gl_ctypes, m_all_gl_categories, m_all_array_sizes);
        process_func_protos(m_glxext_funcs, m_all_gl_ctypes, m_all_gl_categories, m_all_array_sizes);
        process_func_protos(m_cgl_funcs, m_all_gl_ctypes, m_all_gl_categories, m_all_array_sizes);
        process_func_protos(m_cglext_funcs, m_all_gl_ctypes, m_all_gl_categories, m_all_array_sizes);
        process_func_protos(m_wgl_funcs, m_all_gl_ctypes, m_all_gl_categories, m_all_array_sizes);
        process_func_protos(m_wglext_funcs, m_all_gl_ctypes, m_all_gl_categories, m_all_array_sizes);

        // -- Create the m_unique_ctype_enums table
        for (uint32_t i = 0; i < m_all_gl_funcs.size(); i++)
        {
            const gl_function_def &func = m_all_gl_funcs[i];

            m_unique_ctype_enums.insert(std::make_pair(func.m_return_ctype_enum, func.m_return_ctype));
            for (uint32_t p = 0; p < func.m_params.size(); p++)
                m_unique_ctype_enums.insert(std::make_pair(func.m_params[p].m_ctype_enum, func.m_params[p].m_ctype));
        }

// -- Create the m_pointee_types table
#if 0
		printf("---\n");
		for (uint32_t i = 0; i < whitelisted_funcs.size(); i++)
		{
			int j = simple_replay_funcs.find(whitelisted_funcs[i]);
			if (j < 0)
				printf("%s\n", whitelisted_funcs[i].get_ptr());
		}
		printf("---\n");
#endif

        for (gl_string_map::const_iterator it = m_unique_ctype_enums.begin(); it != m_unique_ctype_enums.end(); ++it)
        {
            dynamic_string ctype(it->first.get_ptr());
            if (!ctype.ends_with("_PTR"))
                continue;

            ctype.shorten(4);

            if (ctype.ends_with("_CONST"))
                ctype.shorten(6);

            if (m_unique_ctype_enums.find(ctype) != m_unique_ctype_enums.end())
            {
                m_pointee_types.insert(std::make_pair(it->first.get_ptr(), ctype));
            }
            else
            {
                dynamic_string ctype_without_const(ctype);
                int ofs = ctype_without_const.find_left("_CONST_");
                if (ofs >= 0)
                {
                    ctype_without_const.remove(ofs, 6);
                }
                if (m_unique_ctype_enums.find(ctype_without_const) != m_unique_ctype_enums.end())
                {
                    m_pointee_types.insert(std::make_pair(it->first.get_ptr(), ctype_without_const));
                }
                else
                {
                    dynamic_string ctype_without_const_definition(it->second);

                    int ptr_ofs = ctype_without_const_definition.find_right('*');
                    if (ptr_ofs >= 0)
                        ctype_without_const_definition.remove(ptr_ofs, 1);
                    int const_ofs = ctype_without_const_definition.find_left("const");
                    if (const_ofs >= 0)
                        ctype_without_const_definition.remove(const_ofs, 5);
                    ctype_without_const_definition.trim();

                    new_ctype_from_ptr.push_back(ctype_without_const + ":" + ctype_without_const_definition);

                    m_unique_ctype_enums.insert(std::make_pair(ctype_without_const, ctype_without_const_definition));
                }
                m_pointee_types.insert(std::make_pair(it->first.get_ptr(), ctype_without_const));
            }
        }

        m_pointee_types.insert(std::make_pair("VOGL_LPCSTR", "VOGL_CHAR"));


        // -- Read the GL/GLX/CGL/WGL whitelisted and nullable funcs file
        if (!read_regex_function_array("gl_glx_whitelisted_funcs.txt", m_whitelisted_funcs))
            return false;

        if (!read_regex_function_array("gl_cgl_whitelisted_funcs.txt", m_whitelisted_funcs))
            return false;

        if (!read_regex_function_array("gl_wgl_whitelisted_funcs.txt", m_whitelisted_funcs))
            return false;

        // -- Read the display list whitelist
        if (!read_regex_function_array("gl_glx_displaylist_whitelist.txt", m_whitelisted_displaylist_funcs))
            return false;

        if (!read_regex_function_array("gl_cgl_displaylist_whitelist.txt", m_whitelisted_displaylist_funcs))
            return false;

        if (!read_regex_function_array("gl_wgl_displaylist_whitelist.txt", m_whitelisted_displaylist_funcs))
            return false;

        if (!read_regex_function_array("gl_glx_nullable_funcs.txt", m_nullable_funcs))
            return false;

        if (!read_regex_function_array("gl_cgl_nullable_funcs.txt", m_nullable_funcs))
            return false;

        if (!read_regex_function_array("gl_wgl_nullable_funcs.txt", m_nullable_funcs))
            return false;

        if (!read_regex_function_array("gl_glx_simple_replay_funcs.txt", m_simple_replay_funcs))
            return false;

        if (!read_regex_function_array("gl_cgl_simple_replay_funcs.txt", m_simple_replay_funcs))
            return false;

        if (!read_regex_function_array("gl_wgl_simple_replay_funcs.txt", m_simple_replay_funcs))
            return false;

        update_whitelisted_funcs(m_gl_funcs, m_unique_ctype_enums, m_pointee_types, m_whitelisted_funcs);

        m_whitelisted_funcs.sort();

        process_func_defs("gl", m_gl_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, funcs_with_custom_array_size_macros, custom_return_param_array_size_macros, funcs_with_return_param_array_size_macros);
        process_func_defs("glX", m_glx_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, funcs_with_custom_array_size_macros, custom_return_param_array_size_macros, funcs_with_return_param_array_size_macros);
        process_func_defs("glX", m_glxext_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, funcs_with_custom_array_size_macros, custom_return_param_array_size_macros, funcs_with_return_param_array_size_macros);
        process_func_defs("CGL", m_cgl_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, funcs_with_custom_array_size_macros, custom_return_param_array_size_macros, funcs_with_return_param_array_size_macros);
        process_func_defs("CGL", m_cglext_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, funcs_with_custom_array_size_macros, custom_return_param_array_size_macros, funcs_with_return_param_array_size_macros);
        process_func_defs("wgl", m_wgl_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, funcs_with_custom_array_size_macros, custom_return_param_array_size_macros, funcs_with_return_param_array_size_macros);
        process_func_defs("wgl", m_wglext_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, funcs_with_custom_array_size_macros, custom_return_param_array_size_macros, funcs_with_return_param_array_size_macros);


        uint32_t cur_func_id = 0;
        create_array_size_macros("gl", m_gl_funcs, m_gl_so_dylib_dll_function_exports, custom_array_size_macros, custom_array_size_macro_indices, custom_array_size_macro_names, cur_func_id);
        create_array_size_macros("glX", m_glx_funcs, m_gl_so_dylib_dll_function_exports, custom_array_size_macros, custom_array_size_macro_indices, custom_array_size_macro_names, cur_func_id);
        create_array_size_macros("glX", m_glxext_funcs, m_gl_so_dylib_dll_function_exports, custom_array_size_macros, custom_array_size_macro_indices, custom_array_size_macro_names, cur_func_id);
        create_array_size_macros("CGL", m_cgl_funcs, m_gl_so_dylib_dll_function_exports, custom_array_size_macros, custom_array_size_macro_indices, custom_array_size_macro_names, cur_func_id);
        create_array_size_macros("CGL", m_cglext_funcs, m_gl_so_dylib_dll_function_exports, custom_array_size_macros, custom_array_size_macro_indices, custom_array_size_macro_names, cur_func_id);
        create_array_size_macros("wgl", m_wgl_funcs, m_gl_so_dylib_dll_function_exports, custom_array_size_macros, custom_array_size_macro_indices, custom_array_size_macro_names, cur_func_id);
        create_array_size_macros("wgl", m_wglext_funcs, m_gl_so_dylib_dll_function_exports, custom_array_size_macros, custom_array_size_macro_indices, custom_array_size_macro_names, cur_func_id);

        if (g_command_line_params().get_value_as_bool("verbose"))
        {
            if (gl_ext_func_deleted.size())
            {
                vogl_debug_printf("\nDeleted glxext/cglext/wglext funcs: ");
                for (uint32_t i = 0; i < gl_ext_func_deleted.size(); i++)
                    vogl_debug_printf("%s ", gl_ext_func_deleted[i].get_ptr());
                vogl_debug_printf("\n");
            }
            
            // printf("Introducing new ctype from ptr to pointee: %s %s\n", ctype_without_const.get_ptr(), ctype_without_const_definition.get_ptr());
            if (new_ctype_from_ptr.size())
            {
                vogl_debug_printf("\nNew ctype from ptr to pointee: ");
                for (uint32_t i = 0; i < new_ctype_from_ptr.size(); i++)
                    vogl_debug_printf("%s ", new_ctype_from_ptr[i].get_ptr());
                vogl_debug_printf("\n");
            }
            
            // vogl_printf("Adding simple GL replay func %s to func whitelist\n", full_func_name.get_ptr());
            if (simple_gl_replay_func.size())
            {
                vogl_debug_printf("\nAdded simple GL replay func to func whitelist: ");
                for (uint32_t i = 0; i < simple_gl_replay_func.size(); i++)
                    vogl_debug_printf("%s ", simple_gl_replay_func[i].get_ptr());
                vogl_debug_printf("\n");
            }
        }

        gl_types os_specific_typemap = m_glx_typemap;
        os_specific_typemap.get_type_map().insert(m_cgl_typemap.get_type_map().begin(), m_cgl_typemap.get_type_map().end());
        os_specific_typemap.get_type_map().insert(m_wgl_typemap.get_type_map().begin(), m_wgl_typemap.get_type_map().end());
        
        // -- Cross reference the GL spec vs. XML files, for validation
        if (!validate_functions(m_gl_xml_functions, m_all_gl_funcs, m_apitrace_gl_func_specs, m_gl_typemap, os_specific_typemap))
        {
            vogl_error_printf("Failed validating functions!\n");
            return false;
        }

        if (!init_gl_gets())
            return false;

        return true;
    }

    bool func_regex(const char *pPattern) const
    {
        regexp r;
        if (!r.init(pPattern))
        {
            vogl_error_printf("Bad regular expression: \"%s\", reason: \"%s\"\n", pPattern, r.get_error().get_ptr());
            return false;
        }

        uint32_t t = 0;
        for (uint32_t func_index = 0; func_index < m_all_gl_funcs.size(); func_index++)
        {
            if (r.full_match(m_all_gl_funcs[func_index].m_full_name.get_ptr()))
            {
                const gl_function_def &func = m_all_gl_funcs[func_index];

                printf("%s ", func.m_return_ctype.get_ptr());
                printf("%s(", func.m_full_name.get_ptr());
                for (uint32_t j = 0; j < func.m_params.size(); j++)
                {
                    printf("%s %s", func.m_params[j].m_ctype.get_ptr(), func.m_params[j].m_name.get_ptr());
                    if (j != func.m_params.size() - 1)
                        printf(", ");
                }
                printf(");\n");
                t++;
            }
        }
        printf("Found %u results\n", t);

        return true;
    }

    bool param_regex(const char *pPattern) const
    {
        regexp r;
        if (!r.init(pPattern))
        {
            vogl_error_printf("Bad regular expression: \"%s\", reason: \"%s\"\n", pPattern, r.get_error().get_ptr());
            return false;
        }

        uint32_t t = 0;
        for (uint32_t func_index = 0; func_index < m_all_gl_funcs.size(); func_index++)
        {
            const gl_function_def &func_def = m_all_gl_funcs[func_index];

            for (uint32_t param_index = 0; param_index < func_def.m_params.size(); param_index++)
            {
                if (r.full_match(func_def.m_params[param_index].m_name.get_ptr()))
                {
                    printf("%s %u %s\n", func_def.m_full_name.get_ptr(), param_index, func_def.m_params[param_index].m_name.get_ptr());
                    t++;
                }
            }
        }
        printf("Found %u results\n", t);

        return true;
    }

    bool category_regex(const char *pPattern) const
    {
        regexp r;
        if (!r.init(pPattern))
        {
            vogl_error_printf("Bad regular expression: \"%s\", reason: \"%s\"\n", pPattern, r.get_error().get_ptr());
            return false;
        }

        uint32_t t = 0;
        for (uint32_t func_index = 0; func_index < m_all_gl_funcs.size(); func_index++)
        {
            const gl_function_def &func_def = m_all_gl_funcs[func_index];

            if (r.full_match(func_def.m_category.get_ptr()))
            {
                printf("%s %s\n", func_def.m_full_name.get_ptr(), func_def.m_category.get_ptr());
                t++;
            }
        }
        printf("Found %u results\n", t);

        return true;
    }

    bool ctype_regex(const char *pPattern) const
    {
        regexp r;
        if (!r.init(pPattern))
        {
            vogl_error_printf("Bad regular expression: \"%s\", reason: \"%s\"\n", pPattern, r.get_error().get_ptr());
            return false;
        }

        uint32_t t = 0;
        for (uint32_t func_index = 0; func_index < m_all_gl_funcs.size(); func_index++)
        {
            const gl_function_def &func_def = m_all_gl_funcs[func_index];

            if (r.full_match(func_def.m_return_ctype.get_ptr()))
            {
                printf("%s return %s\n", func_def.m_full_name.get_ptr(), func_def.m_return_ctype.get_ptr());
                t++;
            }

            for (uint32_t param_index = 0; param_index < func_def.m_params.size(); param_index++)
            {
                if (r.full_match(func_def.m_params[param_index].m_ctype.get_ptr()))
                {
                    printf("%s %u %s %s\n", func_def.m_full_name.get_ptr(), param_index, func_def.m_params[param_index].m_name.get_ptr(), func_def.m_params[param_index].m_ctype.get_ptr());
                    t++;
                }
            }
        }
        printf("Found %u results\n", t);

        return true;
    }

    bool namespace_regex(const char *pPattern) const
    {
        regexp r;
        if (!r.init(pPattern))
        {
            vogl_error_printf("Bad regular expression: \"%s\", reason: \"%s\"\n", pPattern, r.get_error().get_ptr());
            return false;
        }

        uint32_t t = 0;
        for (uint32_t func_index = 0; func_index < m_apitrace_gl_func_specs.size(); func_index++)
        {
            const apitrace_gl_func_def &func_def = m_apitrace_gl_func_specs[func_index];

            if (r.full_match(vogl_get_namespace_name(func_def.m_return_namespace)))
            {
                printf("%s return %s\n", func_def.m_name.get_ptr(), vogl_get_namespace_name(func_def.m_return_namespace));
                t++;
            }

            for (uint32_t param_index = 0; param_index < func_def.m_params.size(); param_index++)
            {
                const apitrace_gl_func_param_def &param = func_def.m_params[param_index];

                if (r.full_match(vogl_get_namespace_name(param.m_namespace)))
                {
                    printf("%s %u %s\n", func_def.m_name.get_ptr(), param_index, vogl_get_namespace_name(param.m_namespace));
                    t++;
                }
            }
        }
        printf("Found %u results\n", t);

        return true;
    }

    bool dump_debug_files() const
    {
        vogl_warning_printf("TODO: Where would be the best place to dump debug files? For now they still go to cwd.");
        dynamic_string out_debug_dir = ".";

        m_gl_enumerations.dump_to_text_file("dbg_enums.txt");
        m_glx_enumerations.dump_to_text_file("dbg_glx_enums.txt");
        m_glx_ext_enumerations.dump_to_text_file("dbg_glx_ext_enums.txt");
        m_cgl_enumerations.dump_to_text_file("dbg_cgl_enums.txt");
        m_cgl_ext_enumerations.dump_to_text_file("dbg_cgl_ext_enums.txt");
        m_wgl_enumerations.dump_to_text_file("dbg_wgl_enums.txt");
        m_wgl_ext_enumerations.dump_to_text_file("dbg_wgl_ext_enums.txt");


        // -- Dump the definitions for debugging
        m_gl_xml_functions.dump_to_file("dbg_gl_xml_funcs.txt");
        m_gl_funcs.dump_to_file("dbg_gl_funcs.txt");
        m_glx_funcs.dump_to_file("dbg_glx_funcs.txt");
        m_glxext_funcs.dump_to_file("dbg_glxext_funcs.txt");
        m_cgl_funcs.dump_to_file("dbg_cgl_funcs.txt");
        m_cglext_funcs.dump_to_file("dbg_cglext_funcs.txt");
        m_wgl_funcs.dump_to_file("dbg_wgl_funcs.txt");
        m_wglext_funcs.dump_to_file("dbg_wglext_funcs.txt");

        // -- Dump the spec typemap files, for debugging
        m_gl_typemap.dump_to_file("dbg_gl_typemap.txt");
        m_glx_typemap.dump_to_file("dbg_glx_typemap.txt");
        m_cgl_typemap.dump_to_file("dbg_cgl_typemap.txt");
        m_wgl_typemap.dump_to_file("dbg_wgl_typemap.txt");

        m_all_gl_funcs.dump_to_file("dbg_gl_all_funcs.txt");

        FILE *pFile;

        // Write a simple macro file line this:
        //GL_FUNC(OpenGL,true,GLenum,glGetError,(void),())
        //GL_FUNC_VOID(OpenGL,true,glActiveTexture,(GLenum a),(a))
        pFile = fopen_and_log_generic(out_debug_dir, "dbg_gl_glx_cgl_wgl_simple_func_macros.txt", "w");
        for (uint32_t i = 0; i < m_all_gl_funcs.size(); i++)
        {
            const gl_function_def &def = m_all_gl_funcs[i];
            if (def.m_return == "void")
                fprintf(pFile, "GL_FUNC_VOID(%s,%s,(%s),(%s))\n", def.get_lib_name(), def.m_full_name.get_ptr(), def.get_param_proto().get_ptr(), def.get_param_args().get_ptr());
            else
                fprintf(pFile, "GL_FUNC(%s,%s,%s,(%s),(%s))\n", def.get_lib_name(), def.m_return_ctype.get_ptr(), def.m_full_name.get_ptr(), def.get_param_proto().get_ptr(), def.get_param_args().get_ptr());
        }
        fclose(pFile);

        pFile = fopen_and_log_generic(out_debug_dir, "dbg_gl_glx_cgl_wgl_types.txt", "w");
        for (gl_string_set::const_iterator it = m_all_gl_ctypes.begin(); it != m_all_gl_ctypes.end(); ++it)
            vogl_fprintf(pFile, "%s\n", it->get_ptr());
        vogl_fclose(pFile);

        pFile = fopen_and_log_generic(out_debug_dir, "dbg_gl_glx_cgl_wgl_array_sizes.txt", "w");
        for (gl_string_set::const_iterator it = m_all_array_sizes.begin(); it != m_all_array_sizes.end(); ++it)
            vogl_fprintf(pFile, "%s\n", it->get_ptr());
        vogl_fclose(pFile);

        printf("\n");
        printf("--- Functions with custom array size macros:\n");
        for (uint32_t i = 0; i < funcs_with_custom_array_size_macros.size(); i++)
            printf("%s\n", funcs_with_custom_array_size_macros[i].get_ptr());
        printf("---\n");

        printf("--- Whitelisted functions with custom array size macros:\n");
        for (uint32_t i = 0; i < funcs_with_custom_array_size_macros.size(); i++)
        {
            if (scan_dynamic_string_array_for_string(m_whitelisted_funcs, funcs_with_custom_array_size_macros[i].get_ptr(), true) < 0)
                continue;
            printf("%s\n", funcs_with_custom_array_size_macros[i].get_ptr());
        }
        printf("---\n");

        printf("--- All categories:\n");
        for (uint32_t i = 0; i < m_unique_categories.size(); i++)
            printf("%s\n", m_unique_categories[i].get_ptr());
        printf("---\n");

        // -- Write the final whitelist to a file for debugging
        file_utils::write_text_file("dbg_final_gl_glx_cgl_wgl_whitelisted_funcs.txt", m_whitelisted_funcs, true);

        VOGL_ASSERT(m_whitelisted_funcs.is_sorted());

        for (uint32_t i = 0; i < m_unique_categories.size(); i++)
        {
            dynamic_string_array missing_funcs;

            bool are_any_whitelisted = false;
            for (uint32_t j = 0; j < m_all_gl_funcs.size(); j++)
            {
                const gl_function_def &func = m_all_gl_funcs[j];
                if (func.m_category != m_unique_categories[i])
                    continue;

                bool is_whitelisted = m_whitelisted_funcs.find_sorted(func.m_full_name) >= 0;

                if (!is_whitelisted)
                    missing_funcs.push_back(func.m_full_name);
                else
                    are_any_whitelisted = true;
            }

            if (missing_funcs.size())
            {
                printf("--- Non-whitelisted funcs for %s category %s:\n", are_any_whitelisted ? "partially supported" : "unsupported", m_unique_categories[i].get_ptr());
                for (uint32_t j = 0; j < missing_funcs.size(); j++)
                    printf("%s\n", missing_funcs[j].get_ptr());
                printf("---\n");
            }
            else
            {
                printf("--- Full support for category: %s\n", m_unique_categories[i].get_ptr());
            }
        }

        return true;
    }

    bool generate() const
    {
        dynamic_string out_inc_dir;
        g_command_line_params().get_value_as_string(out_inc_dir, "outinc", 0, ".");
        // Create and ignore failure to create, we'll fail if we can't write to the location.
        file_utils::create_directories(out_inc_dir, false);

        if (!m_gl_enumerations.dump_to_definition_macro_file(out_inc_dir, "gl_enums.inc", "GL"))
            return false;
        if (!m_glx_enumerations.dump_to_definition_macro_file(out_inc_dir, "glx_enums.inc", "GLX"))
            return false;
        if (!m_glx_ext_enumerations.dump_to_definition_macro_file(out_inc_dir, "glx_ext_enums.inc", "GLX"))
            return false;
        if (!m_cgl_enumerations.dump_to_definition_macro_file(out_inc_dir, "cgl_enums.inc", NULL))
            return false;
        if (!m_cgl_ext_enumerations.dump_to_definition_macro_file(out_inc_dir, "cgl_ext_enums.inc", NULL))
            return false;
        if (!m_wgl_enumerations.dump_to_definition_macro_file(out_inc_dir, "wgl_enums.inc", NULL))
            return false;
        if (!m_wgl_ext_enumerations.dump_to_definition_macro_file(out_inc_dir, "wgl_ext_enums.inc", NULL))
            return false;

        const gl_types* glx_cgl_wgl_typemaps[] = { &m_glx_typemap, &m_cgl_typemap, &m_wgl_typemap };
        if (!m_gl_enumerations.dump_to_description_macro_file(out_inc_dir, "gl_enum_desc.inc", "GL", m_gl_typemap, VOGL_ARRAY_SIZE(glx_cgl_wgl_typemaps), glx_cgl_wgl_typemaps))
            return false;
        if (!m_glx_enumerations.dump_to_description_macro_file(out_inc_dir, "glx_enum_desc.inc", "GLX", m_glx_typemap, m_gl_typemap))
            return false;
        if (!m_glx_ext_enumerations.dump_to_description_macro_file(out_inc_dir, "glx_ext_desc.inc", "GLX", m_glx_typemap, m_gl_typemap))
            return false;
        if (!m_cgl_enumerations.dump_to_description_macro_file(out_inc_dir, "cgl_enum_desc.inc", "CGL", m_cgl_typemap, m_gl_typemap))
            return false;
        if (!m_cgl_ext_enumerations.dump_to_description_macro_file(out_inc_dir, "cgl_ext_desc.inc", "CGL", m_cgl_typemap, m_gl_typemap))
            return false;
        if (!m_wgl_enumerations.dump_to_description_macro_file(out_inc_dir, "wgl_enum_desc.inc", "WGL", m_wgl_typemap, m_gl_typemap))
            return false;
        if (!m_wgl_ext_enumerations.dump_to_description_macro_file(out_inc_dir, "wgl_ext_desc.inc", "WGL", m_wgl_typemap, m_gl_typemap))
            return false;


        // -- Generate the gl_glx_cgl_wgl_protos.inc include file
        
        FILE *pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_protos.inc", "w");
        if (!pFile)
            return false;

        dump_func_proto_macros(pFile, "gl", m_gl_funcs, m_gl_so_dylib_dll_function_exports);
        dump_func_proto_macros(pFile, "glX", m_glx_funcs, m_gl_so_dylib_dll_function_exports);
        dump_func_proto_macros(pFile, "glX", m_glxext_funcs, m_gl_so_dylib_dll_function_exports);
        dump_func_proto_macros(pFile, "CGL", m_cgl_funcs, m_gl_so_dylib_dll_function_exports);
        dump_func_proto_macros(pFile, "CGL", m_cglext_funcs, m_gl_so_dylib_dll_function_exports);
        dump_func_proto_macros(pFile, "wgl", m_wgl_funcs, m_gl_so_dylib_dll_function_exports);
        dump_func_proto_macros(pFile, "wgl", m_wglext_funcs, m_gl_so_dylib_dll_function_exports);

        vogl_fprintf(pFile, "#undef DEF_PROTO_EXPORTED\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO_EXPORTED_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO_INTERNAL\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO_INTERNAL_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO_VOID\n");
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_ctypes.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_ctypes.inc", "w");
        if (!pFile)
            return false;

        vogl_fprintf(pFile, "DEF_TYPE(VOGL_INVALID_CTYPE, int)\n");
        for (gl_string_map::const_iterator it = m_unique_ctype_enums.begin(); it != m_unique_ctype_enums.end(); ++it)
            vogl_fprintf(pFile, "DEF_TYPE(%s, %s)\n", it->first.get_ptr(), it->second.get_ptr());
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_ctypes_ptr_to_pointee.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_ctypes_ptr_to_pointee.inc", "w");
        if (!pFile)
            return false;

        for (gl_string_map::const_iterator it = m_pointee_types.begin(); it != m_pointee_types.end(); ++it)
            vogl_fprintf(pFile, "DEF_PTR_TO_POINTEE_TYPE(%s, %s)\n", it->first.get_ptr(), it->second.get_ptr());
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_simple_replay_funcs.inc simple replay funcs file, and update the whitelist
        generate_simple_replay_funcs(out_inc_dir, m_all_gl_funcs, m_unique_ctype_enums, m_pointee_types, m_whitelisted_funcs);

        // -- Generate replayer helper macros
        generate_replay_func_load_macros(out_inc_dir, m_all_gl_funcs, m_unique_ctype_enums, m_pointee_types, m_whitelisted_funcs);

        // -- Generate the gl_glx_cgl_wgl_func_defs.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_func_defs.inc", "w");
        if (!pFile)
            return false;

        dump_inc_file_header(pFile);
        dump_func_def_macros(pFile, "gl", m_gl_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs);
        dump_func_def_macros(pFile, "glX", m_glx_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs);
        dump_func_def_macros(pFile, "glX", m_glxext_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs);
        dump_func_def_macros(pFile, "CGL", m_cgl_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs);
        dump_func_def_macros(pFile, "CGL", m_cglext_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs);
        dump_func_def_macros(pFile, "wgl", m_wgl_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs);
        dump_func_def_macros(pFile, "wgl", m_wglext_funcs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs);
        dump_function_def_undef_macros(pFile);
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_func_return_param_array_size_macros.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_func_return_param_array_size_macros.inc", "w");
        if (!pFile)
            return false;

        for (uint32_t i = 0; i < custom_return_param_array_size_macros.size(); i++)
        {
            dynamic_string macro_name(custom_return_param_array_size_macros[i]);
            int left_paren_pos = macro_name.find_left('(');
            int right_paren_pos = macro_name.find_left(')');
            if (left_paren_pos >= 0)
                macro_name.truncate(left_paren_pos);

            int num_params = custom_return_param_array_size_macros[i].count_char(',') + 1;
            if (right_paren_pos - left_paren_pos == 1) 
                num_params = 0;

            dynamic_string params("(");
            for (int j = 0; j < num_params; j++)
            {
                if (j)
                    params.append_char(',');
                params.append_char((char)('a' + j));
            }
            params += ")";

            vogl_fprintf(pFile, "#ifndef %s\n", macro_name.get_ptr());
            vogl_fprintf(pFile, "   #define %s%s -1\n", macro_name.get_ptr(), params.get_ptr());
            vogl_fprintf(pFile, "#endif\n");
        }
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_func_descs.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_func_descs.inc", "w");
        if (!pFile)
            return false;

        dump_inc_file_header(pFile);
        dump_func_desc_macros(pFile, "gl", m_gl_funcs, m_apitrace_gl_func_specs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, m_nullable_funcs, m_whitelisted_displaylist_funcs);
        dump_func_desc_macros(pFile, "glX", m_glx_funcs, m_apitrace_gl_func_specs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, m_nullable_funcs, m_whitelisted_displaylist_funcs);
        dump_func_desc_macros(pFile, "glX", m_glxext_funcs, m_apitrace_gl_func_specs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, m_nullable_funcs, m_whitelisted_displaylist_funcs);
        dump_func_desc_macros(pFile, "CGL", m_cgl_funcs, m_apitrace_gl_func_specs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, m_nullable_funcs, m_whitelisted_displaylist_funcs);
        dump_func_desc_macros(pFile, "CGL", m_cglext_funcs, m_apitrace_gl_func_specs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, m_nullable_funcs, m_whitelisted_displaylist_funcs);
        dump_func_desc_macros(pFile, "wgl", m_wgl_funcs, m_apitrace_gl_func_specs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, m_nullable_funcs, m_whitelisted_displaylist_funcs);
        dump_func_desc_macros(pFile, "wgl", m_wglext_funcs, m_apitrace_gl_func_specs, m_gl_so_dylib_dll_function_exports, m_whitelisted_funcs, m_nullable_funcs, m_whitelisted_displaylist_funcs);
        dump_function_def_undef_macros(pFile);
        vogl_fclose(pFile);

        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_categories.inc", "w");
        if (!pFile)
            return false;

        for (gl_string_set::const_iterator it = m_all_gl_categories.begin(); it != m_all_gl_categories.end(); ++it)
            vogl_fprintf(pFile, "DEF_CATEGORY(%s)\n", it->get_ptr());
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_array_size_macros.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_array_size_macros.inc", "w");
        if (!pFile)
            return false;

        for (gl_string_set::const_iterator it = custom_array_size_macros.begin(); it != custom_array_size_macros.end(); ++it)
        {
            dynamic_string macro_name(it->get_ptr());
            macro_name.truncate(macro_name.find_left('('));
            vogl_fprintf(pFile, "#ifndef %s\n", macro_name.get_ptr());
            vogl_fprintf(pFile, "   #define %s%s -1\n", macro_name.get_ptr(), DEF_FUNCTION_PROTO_PARAM_STR);
            vogl_fprintf(pFile, "#endif\n");
        }
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_array_size_macros_validator.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_array_size_macros_validator.inc", "w");
        if (!pFile)
            return false;

        for (uint32_t i = 0; i < custom_array_size_macro_indices.size(); i++)
        {
            uint32_t j = custom_array_size_macro_indices[i];
            uint32_t func_index = j >> 16;
            uint32_t param_index = j & 0xFFFF;
            VOGL_NOTE_UNUSED(param_index);

            const dynamic_string &macro_name = custom_array_size_macro_names[i];

            vogl_fprintf(pFile, "#ifndef DEF_FUNCTION_CUSTOM_HANDLER_%s%s\n", g_lib_api_prefixes[m_all_gl_funcs[func_index].m_lib], m_all_gl_funcs[func_index].m_name.get_ptr());
            vogl_fprintf(pFile, "   #ifdef %s\n", macro_name.get_ptr());
            vogl_fprintf(pFile, "      VALIDATE_ARRAY_SIZE_MACRO_DEFINED(%s, 0x%08X)\n", macro_name.get_ptr(), custom_array_size_macro_indices[i]);
            vogl_fprintf(pFile, "   #else\n");
            vogl_fprintf(pFile, "      VALIDATE_ARRAY_SIZE_MACRO_NOT_DEFINED(%s, 0x%08X)\n", macro_name.get_ptr(), custom_array_size_macro_indices[i]);
            vogl_fprintf(pFile, "   #endif\n");
            vogl_fprintf(pFile, "#endif\n");
        }
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_custom_return_param_array_size_macro_validator.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_custom_return_param_array_size_macro_validator.inc", "w");
        if (!pFile)
            return false;

        for (uint32_t i = 0; i < custom_return_param_array_size_macros.size(); i++)
        {
            dynamic_string macro_name(custom_return_param_array_size_macros[i]);
            int n = macro_name.find_left('(');
            if (n >= 0)
                macro_name.truncate(n);

            vogl_fprintf(pFile, "#ifdef %s\n", macro_name.get_ptr());
            vogl_fprintf(pFile, "   CUSTOM_FUNC_RETURN_PARAM_ARRAY_SIZE_HANDLER_DEFINED(%s, %s)\n", macro_name.get_ptr(), funcs_with_return_param_array_size_macros[i].get_ptr());
            vogl_fprintf(pFile, "#else\n");
            vogl_fprintf(pFile, "   CUSTOM_FUNC_RETURN_PARAM_ARRAY_SIZE_HANDLER_NOT_DEFINED(%s, %s)\n", macro_name.get_ptr(), funcs_with_return_param_array_size_macros[i].get_ptr());
            vogl_fprintf(pFile, "#endif\n");
        }
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_custom_func_handler_validator.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_custom_func_handler_validator.inc", "w");
        if (!pFile)
            return false;

        for (uint32_t i = 0; i < m_all_gl_funcs.size(); i++)
        {
            vogl_fprintf(pFile, "#ifdef DEF_FUNCTION_CUSTOM_HANDLER_%s%s\n", g_lib_api_prefixes[m_all_gl_funcs[i].m_lib], m_all_gl_funcs[i].m_name.get_ptr());
            vogl_fprintf(pFile, "   CUSTOM_FUNC_HANDLER_DEFINED(%u)\n", i);
            vogl_fprintf(pFile, "#else\n");
            vogl_fprintf(pFile, "   CUSTOM_FUNC_HANDLER_NOT_DEFINED(%u)\n", i);
            vogl_fprintf(pFile, "#endif\n");
        }
        vogl_fclose(pFile);

        // -- Generate the gl_glx_cgl_wgl_array_size_macro_func_param_indices.inc include file
        pFile = fopen_and_log_generic(out_inc_dir, "gl_glx_cgl_wgl_array_size_macro_func_param_indices.inc", "w");
        if (!pFile)
            return false;

        for (uint32_t i = 0; i < custom_array_size_macro_indices.size(); i++)
        {
            if (i)
                vogl_fprintf(pFile, ",");
            vogl_fprintf(pFile, "0x%08X", custom_array_size_macro_indices[i]);
            if ((i & 31) == 31)
                vogl_fprintf(pFile, "\n");
        }
        vogl_fprintf(pFile, "\n");
        vogl_fclose(pFile);

        if (!generate_gl_gets_inc_file(out_inc_dir))
            return false;

        if (!generate_libvogltrace_export_script())
            return false;

        return true;
    }

private:
    //-----------------------------------------------------------------------------------------------------------------------
    // init_gl_gets
    //-----------------------------------------------------------------------------------------------------------------------
    bool init_gl_gets()
    {
        static const struct gl_get_files
        {
            const char *m_pFilename;
            uint32_t m_vers;
        } s_gl_get_files[] =
        {
            { "gl10_gets.txt", 0x10 },
            { "gl15_gets.txt", 0x15 },
            { "gl21_gets.txt", 0x21 },
            { "gl33_gets.txt", 0x33 },
            { "gl40_gets.txt", 0x40 },
            { "gl41_gets.txt", 0x41 },
            { "gl42_gets.txt", 0x42 },
            { "gl43_gets.txt", 0x43 }
        };

        for (uint32_t file_iter = 0; file_iter < sizeof(s_gl_get_files) / sizeof(s_gl_get_files[0]); file_iter++)
        {
            const char *pFilename = s_gl_get_files[file_iter].m_pFilename;

            //         typedef vogl::hash_map<dynamic_string, gl_get> gl_get_hash_map;
            //         gl_get_hash_map m_gl_gets;

            dynamic_string_array get_names;

            if (!file_utils::read_text_file(pFilename, get_names,
                file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFIgnoreCommentedLines | file_utils::cRTFPrintErrorMessages))
            {
                return false;
            }

            for (uint32_t i = 0; i < get_names.size(); i++)
            {
                if ((get_names[i].toupper().compare(get_names[i], true) != 0) || (get_names[i].contains(' ')))
                {
                    vogl_error_printf("In file %s, enum name is invalid: \"%s\"\n", pFilename, get_names[i].get_ptr());
                    return false;
                }

                gl_get_hash_map::insert_result ins_res(m_gl_gets.insert(get_names[i], gl_get()));
                gl_get &gl_get_obj = ins_res.first->second;
                gl_get_obj.m_name = get_names[i];
                gl_get_obj.m_min_vers = vogl::math::minimum(gl_get_obj.m_min_vers, s_gl_get_files[file_iter].m_vers);
                gl_get_obj.m_max_vers = vogl::math::maximum(gl_get_obj.m_max_vers, s_gl_get_files[file_iter].m_vers);
            }
        }

        //for (gl_get_hash_map::const_iterator it = m_gl_gets.begin(); it != m_gl_gets.end(); ++it)
        //   printf("%s 0x%04X 0x%04X\n", it->first.get_ptr(), it->second.m_min_vers, it->second.m_max_vers);

        return true;
    }

    bool generate_libvogltrace_export_script() const
    {
#if 0
		// We're writing an anonymous version map on purpose, otherwise bad shit happens in some apps!
		{
global:
			OpenDebugManager;
			OpenDebugSession;
local:
			*;
		};
#endif
        dynamic_string_array nongenerated_exports;
        file_utils::read_text_file("gl_glx_cgl_wgl_nongenerated_so_export_list.txt", nongenerated_exports, file_utils::cRTFPrintWarningMessages);

        dynamic_string out_linker_dir;
        g_command_line_params().get_value_as_string(out_linker_dir, "outlinker", 0, ".");
        file_utils::create_directories(out_linker_dir, false);

        // Convert this to use naked pfiles like everything else--to support writing native line endings.
        // cfile_stream really only works with binary files, and we want to have the \n->\r\n translation happen for us.
        FILE* pFile = fopen_and_log_generic(out_linker_dir, "libvogltrace_linker_script.txt", "w");
        if (!pFile) 
            return false;
            
        fputs("{\n", pFile);
        fputs("  global:\n", pFile);

        for (uint32_t i = 0; i < m_gl_so_dylib_dll_function_exports.size(); i++)
            fprintf(pFile, "    %s;\n", m_gl_so_dylib_dll_function_exports[i].get_ptr());

        for (uint32_t i = 0; i < nongenerated_exports.size(); i++)
            fprintf(pFile, "    %s;\n", nongenerated_exports[i].get_ptr());

        fputs("  local:\n", pFile);
        fputs("    *;\n", pFile);
        fputs("};\n", pFile);
        fclose(pFile);

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // generate_gl_gets_inc_file
    //-----------------------------------------------------------------------------------------------------------------------
    bool generate_gl_gets_inc_file(const dynamic_string& out_inc_dir) const
    {
        // Note: I used this to generate our first gl_gets.inc file, which then had to be hand edited.
        FILE* pFile = fopen_and_log_generic(out_inc_dir, "gl_gets_approx.inc", "w");
        if (!pFile)
            return false;

        vogl::vector<gl_get> sorted_gets;
        for (gl_get_hash_map::const_iterator it = m_gl_gets.begin(); it != m_gl_gets.end(); ++it)
            sorted_gets.push_back(it->second);
        sorted_gets.sort();

        int prev_min_vers = -1;
        int prev_max_vers = -1;
        for (uint32_t i = 0; i < sorted_gets.size(); i++)
        {
            if ((prev_min_vers != static_cast<int>(sorted_gets[i].m_min_vers)) || (prev_max_vers != static_cast<int>(sorted_gets[i].m_max_vers)))
            {
                fprintf(pFile, "// Start of version %u.%u - %u.%u\n", sorted_gets[i].m_min_vers >> 4, sorted_gets[i].m_min_vers & 0xF, sorted_gets[i].m_max_vers >> 4, sorted_gets[i].m_max_vers & 0xF);
                prev_min_vers = sorted_gets[i].m_min_vers;
                prev_max_vers = sorted_gets[i].m_max_vers;
            }
            fprintf(pFile, "DEFINE_GL_GET(%s, 0x%04X, 0x%04X)\n", sorted_gets[i].m_name.get_ptr(), sorted_gets[i].m_min_vers, sorted_gets[i].m_max_vers);
        }
        vogl_fclose(pFile);

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // ensure_gl_exports_are_defined
    //-----------------------------------------------------------------------------------------------------------------------
    bool ensure_gl_exports_are_defined(gl_function_specs &gl_funcs, 
                                       const gl_function_specs &glx_funcs, const gl_function_specs &glxext_funcs, 
                                       const gl_function_specs &cgl_funcs, const gl_function_specs &cglext_funcs, 
                                       const gl_function_specs &wgl_funcs, const gl_function_specs &wglext_funcs)
    {
        cfile_stream list_file("gl_glx_cgl_wgl_so_export_list.txt", cDataStreamReadable);
        if (!list_file.is_opened())
            return false;

        dynamic_string line_str;
        while (list_file.get_remaining())
        {
            if (!list_file.read_line(line_str))
                break;

            line_str.trim();
            if (line_str.is_empty())
                continue;

            dynamic_string func;
            const gl_function_specs *pSpecs = NULL;
            if (line_str.find_left("glX") == 0)
            {
                func.set(line_str).right(3);
                pSpecs = &glx_funcs;
            }
            else if (line_str.find_left("gl") == 0)
            {
                func.set(line_str).right(2);
                pSpecs = &gl_funcs;
            }
            else if (line_str.find_left("CGL") == 0)
            {
                func.set(line_str).right(3);
                pSpecs = &cgl_funcs;
            }
            else if (line_str.find_left("wgl") == 0)
            {
                func.set(line_str).right(3);
                pSpecs = &wgl_funcs;
            }

            if (!pSpecs)
            {
                vogl_warning_printf("Unrecognized SO export: %s\n", line_str.get_ptr());
                continue;
            }

            const gl_function_def *pFunc = pSpecs->find(func.get_ptr());
            if ((!pFunc) && (pSpecs == &glx_funcs))
            {
                pSpecs = &glxext_funcs;
                pFunc = pSpecs->find(func.get_ptr());
            }

            if ((!pFunc) && (pSpecs == &cgl_funcs))
            {
                pSpecs = &cglext_funcs;
                pFunc = pSpecs->find(func.get_ptr());
            }

            if ((!pFunc) && (pSpecs == &wgl_funcs))
            {
                pSpecs = &wglext_funcs;
                pFunc = pSpecs->find(func.get_ptr());
            }

            if (!pFunc)
            {
                if (func.ends_with("OES", true) || func.ends_with("x", true) || func.ends_with("NV", true) || func.ends_with("NVX") ||
                    func.ends_with("SGI") || func.ends_with("EXT") || func.ends_with("X") || func.ends_with("xv") || func.ends_with("Autodesk") ||
                    func.ends_with("WIN"))
                {
                    //vogl_warning_printf("Skipping missing SO export: %s\n", line_str.get_ptr());
                    continue;
                }

                vogl_warning_printf("Can't find SO export in spec files: %s\n", line_str.get_ptr());
                continue;
            }

            //printf("Found %s\n", pFunc->m_name.get_ptr());
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // load_gl_so_function_export_list
    //-----------------------------------------------------------------------------------------------------------------------
    bool load_gl_so_function_export_list(const char *pFilename, dynamic_string_array &gl_so_function_exports)
    {
        cfile_stream list_file(pFilename, cDataStreamReadable);
        if (!list_file.is_opened())
            return false;

        dynamic_string line_str;
        while (list_file.get_remaining())
        {
            if (!list_file.read_line(line_str))
                break;

            line_str.trim();
            if (line_str.is_empty())
                continue;

            gl_so_function_exports.push_back(line_str);
        }

        // unique will sort and remove duplicates.
        gl_so_function_exports.unique();

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // translate_ctype_to_ctype_enum
    //-----------------------------------------------------------------------------------------------------------------------
    dynamic_string translate_ctype_to_ctype_enum(dynamic_string ctype)
    {
        ctype.trim().toupper().replace("_", "");

        uint32_t num_found;
        do
        {
            ctype.replace(" *", "*", true, &num_found);
        } while (num_found);

        do
        {
            ctype.replace("  ", " ", true, &num_found);
        } while (num_found);

        ctype.replace(" ", "_");
        ctype.replace("*", "_PTR");

        return "VOGL_" + ctype;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // determine_ctypes
    //-----------------------------------------------------------------------------------------------------------------------
    bool determine_ctypes(const char *pFunc_prefix, gl_function_specs &gl_funcs, const gl_types &typemap, const gl_types &alt_typemap)
    {
        const gl_types* alt_typemap_ptr = &alt_typemap;
        return determine_ctypes(pFunc_prefix, gl_funcs, typemap, 1, &alt_typemap_ptr);
    }

    bool determine_ctypes(const char *pFunc_prefix, gl_function_specs &gl_funcs, const gl_types &typemap, int num_alts, const gl_types **alt_typemaps)
    {
        const dynamic_string str_wglDescribeLayerPlane = "wglDescribeLayerPlane";
        const dynamic_string str_LAYERPLANEDESCRIPTOR = "LAYERPLANEDESCRIPTOR";

        VOGL_ASSERT(num_alts == 0 || alt_typemaps != NULL);
        for (int i = 0; i < num_alts; ++i) { VOGL_ASSERT(alt_typemaps[i] != NULL); }

        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            gl_function_def &func = gl_funcs[func_index];

            dynamic_string full_func_name(cVarArg, "%s%s", pFunc_prefix, func.m_name.get_ptr());
            
            VOGL_DEBUG_BREAK_ON_FUNCTION(func.m_name, gDebugFunctionName);

            if (func.m_return == "void")
                func.m_return_ctype = "void";
            else
            {
                const dynamic_string *pCType = typemap.find(func.m_return);
                if (!pCType)
                {
                    for (int i = 0; i < num_alts; ++i)
                    {
                        pCType = alt_typemaps[i]->find(func.m_return);
                        if (pCType)
                            break;
                    }
                }
                if (!pCType)
                {
                    vogl_warning_printf("Unable to map spec return type %s\n", func.m_return.get_ptr());
                    return false;
                }

                func.m_return_ctype = *pCType;
            }
            func.m_return_ctype_enum = translate_ctype_to_ctype_enum(func.m_return_ctype);

            for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                gl_function_param &param = func.m_params[param_index];

                const dynamic_string *pCType = typemap.find(param.m_type);
                if (!pCType)
                {
                    for (int i = 0; i < num_alts; ++i)
                    {
                        pCType = alt_typemaps[i]->find(param.m_type);
                        if (pCType)
                            break;
                    }
                }

                if (!pCType)
                {
                    vogl_warning_printf("Unable to map spec param %s type %s\n", param.m_name.get_ptr(), param.m_type.get_ptr());
                    return false;
                }

                dynamic_string type_prefix;

                dynamic_string type_suffix("");
                if ((param.m_semantic == "array") || (param.m_semantic == "reference"))
                {
                    type_suffix = " *";

                    // wglDescribePlayerPlane.params[4] doesn't agree with this otherwise always correct rule.
                    if (param.m_direction == "in" && (full_func_name != str_wglDescribeLayerPlane && param.m_type != str_LAYERPLANEDESCRIPTOR))
                    {
                        if (!pCType->ends_with("const"))
                            type_prefix = "const ";
                    }
                }

                dynamic_string full_ctype(cVarArg, "%s%s%s", type_prefix.get_ptr(), pCType->get_ptr(), type_suffix.get_ptr());

                param.m_ctype = full_ctype;
                param.m_ctype_enum = translate_ctype_to_ctype_enum(full_ctype);
            }
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // get_func_proto_macro
    //-----------------------------------------------------------------------------------------------------------------------
    dynamic_string get_func_proto_macro(const char *pFunc_prefix, const gl_function_def &func, const dynamic_string_array &sorted_gl_so_function_exports) const
    {
        dynamic_string proto;

		// Feral: TBD if this is the correct approach
    	if (strcmp(pFunc_prefix, func.get_lib_name()) != 0)
    	{
			vogl_warning_printf("Function's library prefix (%s) does not match supplied prefix (%s), overriding prefix to produce %s%s\n",
									func.get_lib_name(), pFunc_prefix, func.get_lib_name(), func.m_name.get_ptr());

			pFunc_prefix = func.get_lib_name();
    	}

        dynamic_string full_func_name(cVarArg, "%s%s", pFunc_prefix, func.m_name.get_ptr());

        dynamic_string exported_param((func.m_return == "void") ? "DEF_PROTO_INTERNAL_VOID" : "DEF_PROTO_INTERNAL");
        int export_index = sorted_gl_so_function_exports.find_sorted(full_func_name);
        if (export_index >= 0)
            exported_param = (func.m_return == "void") ? "DEF_PROTO_EXPORTED_VOID" : "DEF_PROTO_EXPORTED";

        dynamic_string category(func.m_category.get_len() ? func.m_category.get_ptr() : "UNDEFINED");

        proto.format_append("( %s, %s, %s, %s, %u, ", exported_param.get_ptr(), category.get_ptr(), func.m_return_ctype.get_ptr(), func.m_return_ctype_enum.get_ptr(), func.m_params.size());

        proto.format_append("%s, ", full_func_name.get_ptr());

        proto.format_append("(");
        for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
        {
            const gl_function_param &param = func.m_params[param_index];

            proto.format_append("%s%s%s", param.m_ctype.get_ptr(), param.m_ctype.ends_with("*") ? "" : " ", param.m_name.get_ptr());
            if (param_index != func.m_params.size() - 1)
                proto.format_append(", ");
        }

        proto.format_append("), ");

        proto.format_append("(");
        for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
        {
            proto.format_append("%s", func.m_params[param_index].m_name.get_ptr());
            if (param_index != func.m_params.size() - 1)
                proto.format_append(", ");
        }

        proto.format_append(") )");

        return proto;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // process_func_protos
    //-----------------------------------------------------------------------------------------------------------------------
    bool process_func_protos(const gl_function_specs &gl_funcs, gl_string_set &all_gl_ctypes, gl_string_set &all_gl_categories, gl_string_set &array_sizes)
    {

        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func = gl_funcs[func_index];

            all_gl_categories.insert(func.m_category);
            for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                const gl_function_param &param = func.m_params[param_index];

                all_gl_ctypes.insert(param.m_ctype);
                array_sizes.insert(param.m_array_size);
            }
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // dump_func_proto_macros
    //-----------------------------------------------------------------------------------------------------------------------
    bool dump_func_proto_macros(FILE *pFile, const char *pFunc_prefix, const gl_function_specs &gl_funcs, const dynamic_string_array &sorted_gl_so_function_exports) const
    {
        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func = gl_funcs[func_index];

            dynamic_string func_proto(get_func_proto_macro(pFunc_prefix, func, sorted_gl_so_function_exports));

            vogl_fprintf(pFile, "%s%s\n", (func.m_return_ctype == "void") ? "DEF_PROTO_VOID" : "DEF_PROTO", func_proto.get_ptr());
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // func_param_translate_array_size
    //-----------------------------------------------------------------------------------------------------------------------
    dynamic_string func_param_translate_array_size(const char *pFunc_prefix, const gl_function_def &func, const gl_function_param &param, const dynamic_string &func_proto, bool *pCustom_macro_flag = NULL) const
    {
        if (pCustom_macro_flag)
            *pCustom_macro_flag = false;

        if (param.m_array_size.begins_with("[") && param.m_array_size.ends_with("]"))
        {
            dynamic_string array_size_param(param.m_array_size);
            array_size_param.mid(1, array_size_param.get_len() - 2);

            for (uint32_t i = 0; i < func.m_params.size(); i++)
                if ((func.m_params[i].m_name == array_size_param) ||
                    ((func.m_params[i].m_name + "*1") == array_size_param) ||
                    ((func.m_params[i].m_name + "*2") == array_size_param) ||
                    ((func.m_params[i].m_name + "*3") == array_size_param) ||
                    ((func.m_params[i].m_name + "*4") == array_size_param) ||
                    ((func.m_params[i].m_name + "*5") == array_size_param) ||
                    ((func.m_params[i].m_name + "*6") == array_size_param) ||
                    ((func.m_params[i].m_name + "*7") == array_size_param) ||
                    ((func.m_params[i].m_name + "*8") == array_size_param) ||
                    ((func.m_params[i].m_name + "*9") == array_size_param) ||
                    ((func.m_params[i].m_name + "*10") == array_size_param) ||
                    ((func.m_params[i].m_name + "*11") == array_size_param) ||
                    ((func.m_params[i].m_name + "*12") == array_size_param) ||
                    ((func.m_params[i].m_name + "*13") == array_size_param) ||
                    ((func.m_params[i].m_name + "*14") == array_size_param) ||
                    ((func.m_params[i].m_name + "*15") == array_size_param) ||
                    ((func.m_params[i].m_name + "*16") == array_size_param))
                {
                    if (func.m_params[i].m_semantic == "value")
                        return array_size_param;
                    //else if (func.m_params[i].m_semantic == "reference")
                    //   return dynamic_string(cVarArg, "%s ? *%s : -2", array_size_param.get_ptr(), array_size_param.get_ptr());
                    //else if ((func.m_params[i].m_semantic == "array") && (func.m_params[i].m_array_size == "[1]"))
                    //   return dynamic_string(cVarArg, "%s ? *%s : -2", array_size_param.get_ptr(), array_size_param.get_ptr());
                }

            if (array_size_param == "0" || array_size_param == "1" || array_size_param == "2" ||
                array_size_param == "3" || array_size_param == "4" || array_size_param == "5" ||
                array_size_param == "6" || array_size_param == "7" || array_size_param == "8" ||
                array_size_param == "9" || array_size_param == "12" || array_size_param == "16")
            {
                return array_size_param;
            }

            if (array_size_param.begins_with("COMPSIZE(") && array_size_param.ends_with(")"))
            {
                dynamic_string comp_size_param(array_size_param);
                comp_size_param.mid(9, comp_size_param.get_len() - 10);

                // each of these are assumed to be a GL enum
                if ((comp_size_param == "pname") || (comp_size_param == "attribute") ||
                    (comp_size_param == "id") ||  //(comp_size_param == "buffer") ||
                    (comp_size_param == "map") || //(comp_size_param == "name") ||
                    (comp_size_param == "path") || (comp_size_param == "query") ||
                    (comp_size_param == "target") || (comp_size_param == "type") ||
                    (comp_size_param == "value"))
                    return dynamic_string(cVarArg, "DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_GL_ENUM(%s)", comp_size_param.get_ptr());

                // these might be wrong
                for (uint32_t i = 0; i < func.m_params.size(); i++)
                    if (((func.m_params[i].m_name + "*2") == comp_size_param) ||
                        ((func.m_params[i].m_name + "*3") == comp_size_param) ||
                        ((func.m_params[i].m_name + "*4") == comp_size_param))
                    {
                        return dynamic_string(cVarArg, "DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_GL_ENUM(%s)", comp_size_param.get_ptr());
                    }
            }
        }

        if (pCustom_macro_flag)
            *pCustom_macro_flag = true;

        dynamic_string custom_array_size(cVarArg, "DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_%s%s_%s%s", pFunc_prefix, func.m_name.get_ptr(), param.m_name.get_ptr(), func_proto.get_ptr());
        return custom_array_size;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // create_array_size_macros
    // TODO: use funcs_with_custom_array_size_macros[] vs. scanning for DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_
    //-----------------------------------------------------------------------------------------------------------------------
    void create_array_size_macros(const char *pFunc_prefix, const gl_function_specs &gl_funcs, const dynamic_string_array &sorted_gl_so_function_exports,
                                  gl_string_set &_custom_array_size_macros, vogl::vector<uint32_t> &_custom_array_size_macro_indices, dynamic_string_array &_custom_array_size_macro_names, uint32_t &cur_func_id) const
    {
        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func = gl_funcs[func_index];

            dynamic_string func_proto(get_func_proto_macro(pFunc_prefix, func, sorted_gl_so_function_exports));

            for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                const gl_function_param &param = func.m_params[param_index];

                if (param.m_semantic == "array")
                {
                    dynamic_string array_size_macro(func_param_translate_array_size(pFunc_prefix, func, param, func_proto));
                    if (array_size_macro.begins_with("DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_"))
                    {
                        _custom_array_size_macros.insert(array_size_macro);

                        // oh god this is ugly
                        dynamic_string macro_name(array_size_macro);
                        int n = macro_name.find_left('(');
                        if (n >= 0)
                            macro_name.truncate(n);

                        _custom_array_size_macro_indices.push_back((cur_func_id << 16) | param_index);
                        _custom_array_size_macro_names.push_back(macro_name);
                    }
                }
            }

            cur_func_id++;
        }
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // process_func_defs
    //-----------------------------------------------------------------------------------------------------------------------
    bool process_func_defs(const char *pFunc_prefix, const gl_function_specs &gl_funcs, const dynamic_string_array &sorted_gl_so_function_exports,
                           const dynamic_string_array &whitelisted_funcs,
                           dynamic_string_array &_funcs_with_custom_array_size_macros,
                           dynamic_string_array &_custom_return_param_array_size_macros,
                           dynamic_string_array &_funcs_with_return_param_array_size_macros)
    {
        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func = gl_funcs[func_index];

            dynamic_string full_func_name(func.m_full_name); //cVarArg, "%s%s", pFunc_prefix, func.m_name.get_ptr());

            bool is_in_whitelist = false;
            VOGL_NOTE_UNUSED(is_in_whitelist);
            for (uint32_t i = 0; i < whitelisted_funcs.size(); i++)
            {
                if (whitelisted_funcs[i] == full_func_name)
                {
                    is_in_whitelist = true;
                    break;
                }
            }

            dynamic_string func_proto(get_func_proto_macro(pFunc_prefix, func, sorted_gl_so_function_exports));

            // TODO: This (func_has_custom_array_size_macros) is dead code
            bool func_has_custom_array_size_macros = false;

            dynamic_string func_params;

            for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                const gl_function_param &param = func.m_params[param_index];

                func_params.append(param.m_name);
                if (param_index != (func.m_params.size() - 1))
                    func_params.append(", ");
            }

            if (func.m_return != "void")
            {
                dynamic_string return_param_compute_array_size_macro_name(cVarArg, "DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_%s(%s)", full_func_name.get_ptr(), func_params.get_ptr());

                _custom_return_param_array_size_macros.push_back(return_param_compute_array_size_macro_name);
                _funcs_with_return_param_array_size_macros.push_back(full_func_name);
            }

            if (func_has_custom_array_size_macros)
            {
                _funcs_with_custom_array_size_macros.push_back(full_func_name);
            }
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // dump_func_def_macros
    //-----------------------------------------------------------------------------------------------------------------------
    bool dump_func_def_macros(FILE *pFile, const char *pFunc_prefix, const gl_function_specs &gl_funcs, const dynamic_string_array &sorted_gl_so_function_exports,
                              const dynamic_string_array &whitelisted_funcs) const
    {
        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func = gl_funcs[func_index];

			// Feral: TBD if this is the correct approach
    		if (strcmp(pFunc_prefix, func.get_lib_name()) != 0)
    		{
				vogl_warning_printf("Function's library prefix (%s) does not match supplied prefix (%s), overriding prefix to produce %s%s\n",
										func.get_lib_name(), pFunc_prefix, func.get_lib_name(), func.m_name.get_ptr());

				pFunc_prefix = func.get_lib_name();
	    	}

            dynamic_string full_func_name(cVarArg, "%s%s", pFunc_prefix, func.m_name.get_ptr());

            bool is_in_whitelist = false;
            VOGL_NOTE_UNUSED(is_in_whitelist);
            for (uint32_t i = 0; i < whitelisted_funcs.size(); i++)
            {
                if (whitelisted_funcs[i] == full_func_name)
                {
                    is_in_whitelist = true;
                    break;
                }
            }

            dynamic_string func_proto(get_func_proto_macro(pFunc_prefix, func, sorted_gl_so_function_exports));

            vogl_fprintf(pFile, "// %s Category: %s Vers: \"%s\" Deprecated: \"%s\" Profile: \"%s\"\n", full_func_name.get_ptr(), func.m_category.get_ptr(), func.m_version.get_ptr(), func.m_deprecated.get_ptr(), func.m_profile.get_ptr());

            vogl_fprintf(pFile, "#ifdef DEF_FUNCTION_CUSTOM_HANDLER_%s\n", full_func_name.get_ptr());

            vogl_fprintf(pFile, "   DEF_FUNCTION_CUSTOM_HANDLER_%s%s\n", full_func_name.get_ptr(), func_proto.get_ptr());
            vogl_fprintf(pFile, "   #undef DEF_FUNCTION_CUSTOM_HANDLER_%s\n", full_func_name.get_ptr());

            vogl_fprintf(pFile, "#else\n");

            vogl_fprintf(pFile, "   DEF_FUNCTION_BEGIN%s%s\n", func.m_return == "void" ? "_VOID" : "", func_proto.get_ptr());

            vogl_fprintf(pFile, "   #ifdef DEF_FUNCTION_CUSTOM_FUNC_PROLOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "      DEF_FUNCTION_CUSTOM_FUNC_PROLOG_%s%s\n", full_func_name.get_ptr(), func_proto.get_ptr());
            vogl_fprintf(pFile, "      #undef DEF_FUNCTION_CUSTOM_FUNC_PROLOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "   #endif\n");

            vogl_fprintf(pFile, "   DEF_FUNCTION_INIT%s%s\n", func.m_return == "void" ? "_VOID" : "", func_proto.get_ptr());

            bool func_has_custom_array_size_macros = false;

            dynamic_string func_params;

            for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                const gl_function_param &param = func.m_params[param_index];

                func_params.append(param.m_name);
                if (param_index != (func.m_params.size() - 1))
                    func_params.append(", ");

                if (param.m_direction != "in")
                    continue;

                if (param.m_semantic == "value")
                {
                    vogl_fprintf(pFile, "   DEF_FUNCTION_INPUT_VALUE_PARAM( %u, %s, %s, %s, %s )\n", param_index, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr());
                }
                else if (param.m_semantic == "array")
                {
                    vogl_fprintf(pFile, "   // %s ArraySize=%s\n", param.m_name.get_ptr(), param.m_array_size.get_ptr());
                    vogl_fprintf(pFile, "   DEF_FUNCTION_INPUT_ARRAY_PARAM( %u, %s, %s, %s, %s, %s )\n", param_index, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr(), func_param_translate_array_size(pFunc_prefix, func, param, func_proto, &func_has_custom_array_size_macros).get_ptr());
                }
                else if (param.m_semantic == "reference")
                {
                    vogl_fprintf(pFile, "   DEF_FUNCTION_INPUT_REFERENCE_PARAM( %u, %s, %s, %s, %s )\n", param_index, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr());
                }
            }

            vogl_fprintf(pFile, "   #ifdef DEF_FUNCTION_CUSTOM_GL_CALLER_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "      DEF_FUNCTION_CUSTOM_GL_CALLER_%s%s\n", full_func_name.get_ptr(), func_proto.get_ptr());
            vogl_fprintf(pFile, "      #undef DEF_FUNCTION_CUSTOM_GL_CALLER_%s\n", full_func_name.get_ptr());

            vogl_fprintf(pFile, "   #else\n");

            vogl_fprintf(pFile, "      #ifdef DEF_FUNCTION_CUSTOM_GL_PROLOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "         DEF_FUNCTION_CUSTOM_GL_PROLOG_%s%s\n", full_func_name.get_ptr(), func_proto.get_ptr());
            vogl_fprintf(pFile, "         #undef DEF_FUNCTION_CUSTOM_GL_PROLOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "      #endif\n");

            vogl_fprintf(pFile, "      DEF_FUNCTION_CALL_GL%s%s\n", func.m_return == "void" ? "_VOID" : "", func_proto.get_ptr());

            vogl_fprintf(pFile, "      #ifdef DEF_FUNCTION_CUSTOM_GL_EPILOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "         DEF_FUNCTION_CUSTOM_GL_EPILOG_%s%s\n", full_func_name.get_ptr(), func_proto.get_ptr());
            vogl_fprintf(pFile, "         #undef DEF_FUNCTION_CUSTOM_GL_EPILOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "      #endif\n");

            vogl_fprintf(pFile, "   #endif\n");

            for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                const gl_function_param &param = func.m_params[param_index];
                if (param.m_direction != "out")
                    continue;

                if (param.m_semantic == "value")
                {
                    vogl_fprintf(pFile, "   DEF_FUNCTION_OUTPUT_VALUE_PARAM( %u, %s, %s, %s, %s )\n", param_index, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr());
                }
                else if (param.m_semantic == "array")
                {
                    vogl_fprintf(pFile, "   // %s ArraySize=%s\n", param.m_name.get_ptr(), param.m_array_size.get_ptr());
                    vogl_fprintf(pFile, "   DEF_FUNCTION_OUTPUT_ARRAY_PARAM( %u, %s, %s, %s, %s, %s )\n", param_index, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr(), func_param_translate_array_size(pFunc_prefix, func, param, func_proto, &func_has_custom_array_size_macros).get_ptr());
                }
                else if (param.m_semantic == "reference")
                {
                    vogl_fprintf(pFile, "   DEF_FUNCTION_OUTPUT_REFERENCE_PARAM( %u, %s, %s, %s, %s )\n", param_index, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr());
                }
            }

            vogl_fprintf(pFile, "   #ifdef DEF_FUNCTION_CUSTOM_FUNC_EPILOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "      DEF_FUNCTION_CUSTOM_FUNC_EPILOG_%s%s\n", full_func_name.get_ptr(), func_proto.get_ptr());
            vogl_fprintf(pFile, "      #undef DEF_FUNCTION_CUSTOM_FUNC_EPILOG_%s\n", full_func_name.get_ptr());
            vogl_fprintf(pFile, "   #endif\n");

            if (func.m_return != "void")
            {
                //vogl_fprintf(pFile, "   DEF_FUNCTION_RETURN_PARAM( %s, %s, %s, )\n", func.m_return.get_ptr(), func.m_return_ctype.get_ptr(), func.m_return_ctype_enum.get_ptr());
                dynamic_string return_param_compute_array_size_macro_name(cVarArg, "DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_%s(%s)", full_func_name.get_ptr(), func_params.get_ptr());

                vogl_fprintf(pFile, "   DEF_FUNCTION_RETURN_PARAM( %s, %s, %s, %s )\n", func.m_return.get_ptr(), func.m_return_ctype.get_ptr(), func.m_return_ctype_enum.get_ptr(), return_param_compute_array_size_macro_name.get_ptr());
            }

            vogl_fprintf(pFile, "   DEF_FUNCTION_END%s%s\n", func.m_return == "void" ? "_VOID" : "", func_proto.get_ptr());

            vogl_fprintf(pFile, "#endif\n");
            vogl_fprintf(pFile, "\n");
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // dump_func_desc_macros
    //-----------------------------------------------------------------------------------------------------------------------
    bool dump_func_desc_macros(
        FILE *pFile, const char *pFunc_prefix,
        const gl_function_specs &gl_funcs, const apitrace_func_specs &apitrace_gl_func_specs, 
        const dynamic_string_array &sorted_gl_so_function_exports, const dynamic_string_array &whitelisted_funcs, const dynamic_string_array &nullable_funcs, const dynamic_string_array &whitelisted_displaylists_funcs) const
    {
        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func = gl_funcs[func_index];

            dynamic_string full_func_name(cVarArg, "%s%s", pFunc_prefix, func.m_name.get_ptr());
            VOGL_ASSERT(full_func_name.compare(func.m_full_name, true) == 0);

            const apitrace_gl_func_def *pApitrace_func_def = apitrace_gl_func_specs.find(full_func_name.get_ptr());

            bool is_in_whitelist = whitelisted_funcs.find(full_func_name) != cInvalidIndex;
            bool is_nullable = nullable_funcs.find(full_func_name) != cInvalidIndex;

            bool is_whitelisted_for_displaylists = false;
            for (uint32_t i = 0; i < whitelisted_displaylists_funcs.size(); i++)
            {
                if (regexp_full_match(func.m_full_name.get_ptr(), whitelisted_displaylists_funcs[i].get_ptr()))
                {
                    is_whitelisted_for_displaylists = true;
                    break;
                }
            }

            if (is_whitelisted_for_displaylists)
            {
                if (func.m_notlistable)
                {
                    vogl_error_printf("Func's %s m_notlistable and is_whitelisted_for_displaylists flags are not compatible!\n", full_func_name.get_ptr());
                }
            }

            dynamic_string func_proto(get_func_proto_macro(pFunc_prefix, func, sorted_gl_so_function_exports));

            vogl_fprintf(pFile, "// %s Category: %s Vers: \"%s\" Deprecated: \"%s\" Profile: \"%s\"\n", full_func_name.get_ptr(), func.m_category.get_ptr(), func.m_version.get_ptr(), func.m_deprecated.get_ptr(), func.m_profile.get_ptr());

            vogl_fprintf(pFile, "DEF_FUNCTION_BEGIN%s\n", func_proto.get_ptr());

            vogl_namespace_t return_namespace = VOGL_NAMESPACE_UNKNOWN;
            if (pApitrace_func_def)
                return_namespace = pApitrace_func_def->m_return_namespace;

            vogl_fprintf(pFile, "   DEF_FUNCTION_INFO( %i, %s, \"%s\", \"%s\", \"%s\", \"%s\", %s, %s, %s, %s )\n", (int)return_namespace, func.m_return.get_ptr(), func.m_category.get_ptr(), func.m_version.get_ptr(),
                        func.m_profile.get_ptr(), func.m_deprecated.get_ptr(), is_in_whitelist ? "true" : "false", is_nullable ? "true" : "false", is_whitelisted_for_displaylists ? "true" : "false", !func.m_notlistable ? "true" : "false");

            if (func.m_params.size())
            {
                vogl_fprintf(pFile, "   DEF_FUNCTION_BEGIN_PARAMS\n");
                for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
                {
                    const gl_function_param &param = func.m_params[param_index];

                    vogl_namespace_t param_namespace = VOGL_NAMESPACE_UNKNOWN;
                    if (pApitrace_func_def)
                    {
                        VOGL_ASSERT(param_index < pApitrace_func_def->m_params.size());
                        if (param_index < pApitrace_func_def->m_params.size())
                            param_namespace = pApitrace_func_def->m_params[param_index].m_namespace;
                    }

                    dynamic_string dir(param.m_direction);
                    dir.toupper();

                    if (param.m_semantic == "value")
                    {
                        vogl_fprintf(pFile, "   DEF_FUNCTION_%s_VALUE_PARAM( %i, %s, %s, %s, %s )\n", dir.get_ptr(), (int)param_namespace, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr());
                    }
                    else if (param.m_semantic == "array")
                    {
                        vogl_fprintf(pFile, "   // %s ArraySize=%s\n", param.m_name.get_ptr(), param.m_array_size.get_ptr());
                        vogl_fprintf(pFile, "   DEF_FUNCTION_%s_ARRAY_PARAM( %i, %s, %s, %s, %s, %s )\n", dir.get_ptr(), (int)param_namespace, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr(), func_param_translate_array_size(pFunc_prefix, func, param, func_proto).get_ptr());
                    }
                    else if (param.m_semantic == "reference")
                    {
                        vogl_fprintf(pFile, "   DEF_FUNCTION_%s_REFERENCE_PARAM( %i, %s, %s, %s, %s )\n", dir.get_ptr(), (int)param_namespace, param.m_type.get_ptr(), param.m_ctype.get_ptr(), param.m_ctype_enum.get_ptr(), param.m_name.get_ptr());
                    }
                }
                vogl_fprintf(pFile, "   DEF_FUNCTION_END_PARAMS\n");
            }

            vogl_fprintf(pFile, "   DEF_FUNCTION_RETURN( %s, %s, %s )\n", func.m_return.get_ptr(), func.m_return_ctype.get_ptr(), func.m_return_ctype_enum.get_ptr());

            vogl_fprintf(pFile, "DEF_FUNCTION_END%s\n", func_proto.get_ptr());

            vogl_fprintf(pFile, "\n");
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // dump_inc_file_header
    //-----------------------------------------------------------------------------------------------------------------------
    void dump_inc_file_header(FILE *pFile) const
    {
        vogl_fprintf(pFile, "// Auto-generated by voglgen. Don't hand modify!\n\n");
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // dump_function_def_undef_macros
    //-----------------------------------------------------------------------------------------------------------------------
    void dump_function_def_undef_macros(FILE *pFile) const
    {
        vogl_fprintf(pFile, "#undef DEF_PROTO_EXPORTED\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO_EXPORTED_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO_INTERNAL\n");
        vogl_fprintf(pFile, "#undef DEF_PROTO_INTERNAL_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_BEGIN_PARAMS\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_REFERENCE_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_IN_VALUE_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_IN_ARRAY_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_IN_REFERENCE_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_OUT_REFERENCE_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_OUT_ARRAY_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_END_PARAMS\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_BEGIN\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_BEGIN_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_INIT\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_INIT_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_INPUT_VALUE_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_INPUT_REFERENCE_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_INPUT_ARRAY_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_OUTPUT_REFERENCE_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_OUTPUT_ARRAY_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_CALL_GL\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_CALL_GL_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_END\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_END_VOID\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_RETURN_PARAM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_GL_ENUM\n");
        vogl_fprintf(pFile, "#undef DEF_FUNCTION_INFO\n");
    }

    struct gl_type_alias_t
    {
        const char *m_pName;
        const char *m_pAlias;
    };

    //-----------------------------------------------------------------------------------------------------------------------
    // purify_gl_type
    //-----------------------------------------------------------------------------------------------------------------------
    static dynamic_string_array purify_gl_type(dynamic_string str)
    {
        static const gl_type_alias_t g_gl_type_comparison_exception_table[] =
            {
                { "__GLXextFuncPtr", "void *" },
                { "GLfunction", "void *" },
                { "GLuint*", "unsigned int *" },
                { "GLuint *", "unsigned int *" },
                { "GLuint", "unsigned int" },
                { "Bool", "bool" },
                { "GLboolean", "bool" },
                { "GLvoid", "void" },
                { "int64_t", "int64_t" },
                { "GLint64", "int64_t" },
                { "GLvoid*", "void*" },
                { "handleARB", "GLhandleARB" },
                { "GLint", "int" },
                { "const GLubyte *", "const char *" },
                { "GLubyte *", "char *" },
                { "GLubyte*", "char *" },
                { "GLchar*", "char *" },
                { "GLchar* const", "char *" },
                { "const char *", "char *" },
                { "GLchar", "char" },
                { "void function()", "void *" },
                { "int32_t", "int" },
                { "GLclampf", "float" },
                { "GLfloat", "float" },
                { "GLclampd", "double" },
                { "GLdouble", "double" },
                { "GLsizei", "int" },
                { "int32_t *", "int *" },
                { "int64_t *", "int64_t *" },
                { "GLint64*", "int64_t *" },
                { "GLint32*", "int *" },
                { "GLint32 *", "int *" },
                { "const GLuint*", "const unsigned int *" },
                { "const GLuint *", "const unsigned int *" },
                { "GLuint32*", "unsigned int *" },
                { "GLuint32 *", "unsigned int *" },
                { "GLulong*", "unsigned long *" },
                { "GLint*", "int *" },
                { "GLint *", "int *" },
                { "const GLint *", "const int *" },
                { "const GLvoid *", "const void *" },
                { "const GLvoid*", "const void *" },
                { "GLvoid *", "void *" },
                { "GLvoid*", "void *" },
                { "GLchar *", "char *" },
                { "GLchar*", "char *" },
                { "const GLchar *", "const char *" },
                { "const GLchar*", "const char *" },
                { "GLcharARB *", "char *" },
                { "GLcharARB*", "char *" },
                { "const GLcharARB *", "const char *" },
                { "const GLcharARB*", "const char *" },
                { "uint32_t *", "unsigned int *" },
                { "ulong", "unsigned long" },
                { "ulong *", "unsigned long *" },
                { "GLuint64", "unsigned int64_t" },
                { "GLuint64*", "unsigned int64_t *" },
                { "const GLuint64*", "const unsigned int64_t *" },
                { "GLuint64EXT", "unsigned int64_t" },
                { "const GLuint64EXT *", "const unsigned int64_t *" },
                { "GLuint64EXT *", "unsigned int64_t *" },
                { "GLint64", "int64_t" },
                { "GLint64*", "int64_t *" },
                { "const GLint64*", "const int64_t *" },
                { "GLint64EXT", "int64_t" },
                { "const GLint64EXT *", "const int64_t *" },
                { "GLint64EXT *", "int64_t *" },
                { "GLintptr", "int *" },
                { "GLintptrARB", "int *" },
                { "GLenum", "int" }, // Mapping GLenum->int is pushing it, but there are some extensions that don't agree on whether the param is GLenum or int.
                { "GLhalfNV", "GLhalf" },
                { "const GLhalfNV *", "const GLhalf *" },
                { "GLchar* const *", "char * const *" },
                { "GLchar **", "char **" },
                { "const GLfloat *", "const float *" },
                { "const GLdouble *", "const double *" },
                { "GLfloat *", "float *" },
                { "const GLfloat *", "const float *" },
                { "GLfloat*", "float *" },
                { "const GLfloat *", "const float *" },
                { "const GLfloat*", "const float *" },
                { "GLclampf *", "float *" },
                { "const GLclampf *", "const float *" },
                { "GLclampf*", "float *" },
                { "GLdouble *", "double *" },
                { "GLdouble*", "double *" },
                { "GLclampd *", "double *" },
                { "GLclampd*", "double *" },
                { "GLsizeiptrARB", "GLsizeiptr" },
                { "handleARB*", "GLhandleARB *" },
                { "const GLcharARB* *", "const char **" },
                { "GLchar * *", "const char **" },
                { "GLchar**", "const char **" },
                { "const GLchar* *", "const char **" },
                { "GLoint", "unsigned int" }, // Bad in scraped XML
                { "const GLchar * *", "const char **" },
                { "cl_context", "struct _cl_context *" }, // Correct in spec, bad in scraped XML.
                { "cl_event", "struct _cl_event *" },     // Correct in spec, bad in scraped XML.

                /* Ugly types from windows. */
                { "UINT* ", "unsigned int *" },
                { "UINT *", "unsigned int *" },
                { "UINT", "unsigned int" },
                { "INT32*", "int *" },
                { "INT32 *", "int *" },
                { "INT32", "int" },
                { "INT64*", "int64_t *" },
                { "INT64 *", "int64_t *" },
                { "INT64", "int64_t" },

            };

        // See if the ctype can be aliased to a simpler, hopefully equivalent ctype.
        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(g_gl_type_comparison_exception_table); i++)
        {
            if (str == g_gl_type_comparison_exception_table[i].m_pName)
            {
                str = g_gl_type_comparison_exception_table[i].m_pAlias;
                break;
            }
        }

        // Tokenize type string
        dynamic_string_array tokens;
        str.tokenize(" \t", tokens, true);

        // Kill all const crap, nobody seems to agree on it so screw it.
        for (int i = 0; i < static_cast<int>(tokens.size()); i++)
        {
            if (tokens[i] == "const")
            {
                tokens.erase(i);
                i--;
            }
        }

        // Seperate out trailing "*" chars into multiple tokens.
        dynamic_string ptr_str("*");

        for (int i = 0; i < static_cast<int>(tokens.size()); i++)
        {
            for (;;)
            {
                if (tokens[i].get_len() == 1)
                    break;

                int pointer_ofs = tokens[i].find_right('*');
                if (pointer_ofs < 0)
                    break;

                tokens[i].truncate(pointer_ofs);
                tokens.insert(i + 1, ptr_str);
            }
        }

        return tokens;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // validate_functions
    //-----------------------------------------------------------------------------------------------------------------------
    bool validate_functions(const gl_function_specs &gl_xml_funcs, const gl_function_specs &gl_funcs, const apitrace_func_specs &apitrace_gl_funcs,
                            const gl_types &primary_typemap, const gl_types &alt_typemap) const
    {
        dynamic_string_array missing_funcs;

        uint32_t total_skipped = 0;
        uint32_t total_validated = 0;
        uint32_t total_failures = 0;
        uint32_t total_not_found = 0;

        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func = gl_funcs[func_index];

            // Let's skip these guys, they are either bad in the XML and not worth fixing up the individual params in code, or they are totally uninteresting (SUN, OES),
            // or they have been manually validated to be correct in the .spec files.
            if ((func.m_name == "Uniform2ui") || (func.m_name == "Uniform3ui") || (func.m_name == "Uniform4ui") ||
                (func.m_name == "ProgramUniform2ui") || (func.m_name == "ProgramUniform3ui") || (func.m_name == "ProgramUniform4ui") ||
                (func.m_name == "ReplacementCodePointerSUN") || (func.m_name == "ClearDepthfOES") || (func.m_name == "QueryMatrixxOES") ||
                (func.m_name == "QueryMatrixxOES") || (func.m_name == "SendPbufferToVideoNV") || (func.m_name == "ReleaseVideoImageNV") ||
                (func.m_name == "BindVideoImageNV") || (func.m_name == "GetTransparentIndexSUN") ||
                (func.m_name == "GetIntegerui64i_vNV"))
            {
                total_skipped++;
                continue;
            }

            const gl_types *pPrimary_typemap = &primary_typemap;
            const gl_types *pAlt_typemap = &alt_typemap;
            if ((func.m_lib == cGLX) || (func.m_lib == cCGL) || (func.m_lib == cWGL))
                std::swap(pPrimary_typemap, pAlt_typemap);

            dynamic_string return_type;
            if (func.m_return == "void")
            {
                return_type = "void";
            }
            else
            {
                const dynamic_string *pCType = pPrimary_typemap->find(func.m_return);
                if (!pCType)
                    pCType = pAlt_typemap->find(func.m_return);
                if (!pCType)
                {
                    vogl_warning_printf("Unable to map spec type %s\n", func.m_return.get_ptr());
                    return false;
                }

                return_type = *pCType;
            }

            dynamic_string_array func_param_ctypes;

            for (uint32_t param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                const gl_function_param &param = func.m_params[param_index];

                const dynamic_string *pCType = pPrimary_typemap->find(param.m_type);
                if (!pCType)
                    pCType = pAlt_typemap->find(param.m_type);
                if (!pCType)
                {
                    vogl_warning_printf("Unable to map spec type %s\n", param.m_type.get_ptr());
                    return false;
                }

                dynamic_string type_prefix;

                dynamic_string type_suffix("");
                if ((param.m_semantic == "array") || (param.m_semantic == "reference"))
                {
                    type_suffix = " *";

                    if (param.m_direction == "in")
                    {
                        if (!pCType->ends_with("const"))
                            type_prefix = "const ";
                    }
                }

                dynamic_string full_ctype(cVarArg, "%s%s%s", type_prefix.get_ptr(), pCType->get_ptr(), type_suffix.get_ptr());

                func_param_ctypes.push_back(full_ctype);
            }

            const apitrace_gl_func_def *pApitrace_func_def = apitrace_gl_funcs.find(func.m_full_name.get_ptr());
            if (pApitrace_func_def)
            {
                if (pApitrace_func_def->m_return_gl_type != return_type)
                {
                    vogl_warning_printf("Function %s return type mismatch vs. apitrace specs: spec: %s, apitrace: %s\n", func.m_full_name.get_ptr(), return_type.get_ptr(), pApitrace_func_def->m_return_gl_type.get_ptr());
                    total_failures++;
                }

                if (pApitrace_func_def->m_params.size() != func_param_ctypes.size())
                {
                    vogl_warning_printf("Function %s has a different number of params vs. apitrace specs: spec: %u, apitrace: %u\n", func.m_full_name.get_ptr(), func_param_ctypes.size(), pApitrace_func_def->m_params.size());
                    total_failures++;
                }
                else
                {
                    for (uint32_t i = 0; i < func_param_ctypes.size(); i++)
                    {
                        const apitrace_gl_func_param_def &param_def = pApitrace_func_def->m_params[i];

                        dynamic_string apitrace_param_type(param_def.m_gl_type);

                        dynamic_string_array purified_spec_ctype(purify_gl_type(func_param_ctypes[i]));
                        dynamic_string_array purified_apitrace_ctype(purify_gl_type(apitrace_param_type));

                        bool type_mismatch = purified_spec_ctype.size() != purified_apitrace_ctype.size();
                        if (!type_mismatch)
                        {
                            for (uint32_t j = 0; j < purified_spec_ctype.size(); j++)
                            {
                                if (purified_spec_ctype[j] != purified_apitrace_ctype[j])
                                {
                                    type_mismatch = true;
                                    break;
                                }
                            }
                        }

                        if (type_mismatch)
                        {
                            vogl_warning_printf("Function %s param %u, spec vs. apitrace type difference: spec: %s apitrace: %s\n", func.m_full_name.get_ptr(), i, func_param_ctypes[i].get_ptr(), apitrace_param_type.get_ptr());
                            total_failures++;

                            //printf("   { \"%s\", \"%s\" },\n", func_param_ctypes[i].get_ptr(), apitrace_param_type.get_ptr());
                        }
                    }
                }
            }
            else
            {
                //printf("Func not found in apitrace specs: %s\n", func.m_full_name.get_ptr());
            }

            uint32_t gl_xml_func_index;
            for (gl_xml_func_index = 0; gl_xml_func_index < gl_xml_funcs.size(); gl_xml_func_index++)
            {
                if (gl_xml_funcs[gl_xml_func_index].m_lib != func.m_lib)
                    continue;

                dynamic_string gl_xml_func_name(gl_xml_funcs[gl_xml_func_index].m_name);

                if (gl_xml_func_name == func.m_name)
                    break;
            }

            if (gl_xml_func_index == gl_xml_funcs.size())
            {
                dynamic_string suffix;
                if (func.m_name.ends_with("ARB", true) || func.m_name.ends_with("EXT", true))
                    suffix.set(func.m_name).right(func.m_name.get_len() - 3);
                else if (func.m_name.ends_with("NV", true))
                    suffix.set(func.m_name).right(func.m_name.get_len() - 2);

                if (suffix.get_len())
                {
                    for (gl_xml_func_index = 0; gl_xml_func_index < gl_xml_funcs.size(); gl_xml_func_index++)
                    {
                        if (gl_xml_funcs[gl_xml_func_index].m_lib != func.m_lib)
                            continue;

                        dynamic_string gl_xml_func_name(gl_xml_funcs[gl_xml_func_index].m_name);
                        gl_xml_func_name += suffix;

                        if (gl_xml_func_name == func.m_name)
                            break;
                    }
                }

                if (gl_xml_func_index == gl_xml_funcs.size())
                {
                    missing_funcs.push_back(func.m_name);
                    total_not_found++;
                    continue;
                }
            }

            const gl_function_def &xml_func = gl_xml_funcs[gl_xml_func_index];
            if (xml_func.m_params.size() != func.m_params.size())
            {
                vogl_error_printf("Function %s param number mismatch\n", func.m_name.get_ptr());
                total_failures++;
                continue;
            }

            dynamic_string_array purified_func_return(purify_gl_type(return_type));

            dynamic_string xml_func_return(xml_func.m_return);

            // XML exceptions (they where incorrect on the web, GLsync here makes no sense)
            if ((func.m_name == "FramebufferRenderbuffer") || (func.m_name == "FlushMappedBufferRange"))
            {
                xml_func_return = "void";
            }

            dynamic_string_array purified_xml_func_return(purify_gl_type(xml_func_return));

            if (purified_func_return.size() != purified_xml_func_return.size())
            {
                vogl_warning_printf("Function %s return type mismatch: \"%s\" != \"%s\"\n", func.m_name.get_ptr(), return_type.get_ptr(), xml_func_return.get_ptr());
                total_failures++;
            }
            else
            {
                for (uint32_t i = 0; i < purified_func_return.size(); i++)
                {
                    if (purified_func_return[i] != purified_xml_func_return[i])
                    {
                        vogl_warning_printf("Function %s return type mismatch: \"%s\" != \"%s\", param \"%s\" != \"%s\"\n", func.m_name.get_ptr(), return_type.get_ptr(), xml_func_return.get_ptr(), purified_func_return[i].get_ptr(), purified_xml_func_return[i].get_ptr());
                        total_failures++;
                    }
                }
            }

            uint32_t param_index;
            for (param_index = 0; param_index < func.m_params.size(); param_index++)
            {
                dynamic_string param_type(func_param_ctypes[param_index]);
                dynamic_string xml_param_type(xml_func.m_params[param_index].m_type);

                // manual XML exceptions (incorrect on the web)
                if ((param_index == 4) && (func.m_name == "GetTransformFeedbackVarying"))
                {
                    xml_param_type = "GLsizei *";
                }
                if ((param_index == 3) && (func.m_name == "GetActiveUniformBlockiv"))
                {
                    xml_param_type = "GLint *";
                }
                if ((param_index == 1) && (func.m_full_name == "glXCreateContextAttribsARB"))
                {
                    xml_param_type = "GLXFBConfig";
                }
                if ((param_index == 3) && (func.m_name == "ProgramNamedParameter4fvNV"))
                {
                    xml_param_type = "GLfloat *";
                }
                if ((param_index == 3) && (func.m_name == "ProgramNamedParameter4dvNV"))
                {
                    xml_param_type = "GLdouble *";
                }

                dynamic_string_array purified_param_type(purify_gl_type(param_type));
                dynamic_string_array purified_xml_param_type(purify_gl_type(xml_param_type));

                if (purified_param_type.size() != purified_xml_param_type.size())
                {
                    vogl_warning_printf("Function %s param type mismatch: \"%s\" != \"%s\"\n", func.m_name.get_ptr(), param_type.get_ptr(), xml_param_type.get_ptr());
                    total_failures++;
                }
                else
                {
                    for (uint32_t j = 0; j < purified_param_type.size(); j++)
                    {
                        if (purified_param_type[j] != purified_xml_param_type[j])
                        {
                            vogl_warning_printf("Function %s param type mismatch: \"%s\" != \"%s\", param \"%s\" != \"%s\"\n", func.m_name.get_ptr(),
                                             param_type.get_ptr(),
                                             xml_param_type.get_ptr(),
                                             purified_param_type[j].get_ptr(),
                                             purified_xml_param_type[j].get_ptr());
                            total_failures++;
                        }
                    }
                }
            }

            total_validated++;
        }

        printf("\n");
        printf("Results of spec vs. XML function validation:\n");
        printf("Total passed: %u\n", total_validated);
        printf("Total failures: %u\n", total_failures);
        printf("Total skipped: %u\n", total_skipped);
        printf("Total functions present in spec but missing in XML: %u\n", total_not_found);

        if (g_command_line_params().get_value_as_bool("verbose"))
        {
            vogl_debug_printf("\nMissing function list:\n");
            for (uint32_t i = 0; i < missing_funcs.size(); i++)
                vogl_debug_printf("%s\n", missing_funcs[i].get_ptr());

            vogl_debug_printf("\nMissing function list excluding OES and vendor suffixes:\n");
            for (uint32_t i = 0; i < missing_funcs.size(); i++)
            {
                const dynamic_string &func = missing_funcs[i];

                uint32_t j;
                for (j = 0; j < VOGL_ARRAY_SIZE(g_gl_vendor_suffixes); j++)
                    if (func.ends_with(g_gl_vendor_suffixes[j], true))
                        break;

                if (j < VOGL_ARRAY_SIZE(g_gl_vendor_suffixes))
                    continue;

                vogl_debug_printf("%s\n", func.get_ptr());
            }
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // update_whitelisted_funcs
    //-----------------------------------------------------------------------------------------------------------------------
    bool update_whitelisted_funcs(const gl_function_specs &gl_funcs, const gl_string_map &unique_ctype_enums, const gl_string_map &pointee_types, dynamic_string_array &whitelisted_funcs)
    {
        VOGL_NOTE_UNUSED(unique_ctype_enums);
        VOGL_NOTE_UNUSED(pointee_types);

        dynamic_string_array new_whitelisted_funcs(whitelisted_funcs);

        vogl::vector<bool> used_flags(m_simple_replay_funcs.size());

        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func_def = gl_funcs[func_index];
            dynamic_string full_func_name(func_def.m_full_name); //cVarArg, "%s%s", g_lib_api_prefixes[gl_funcs[func_index].m_lib], gl_funcs[func_index].m_name.get_ptr());

            uint32_t i;
            for (i = 0; i < m_simple_replay_funcs.size(); i++)
            {
                if (m_simple_replay_funcs[i].ends_with("*"))
                {
                    dynamic_string prefix(m_simple_replay_funcs[i]);
                    prefix.left(prefix.get_len() - 1);
                    if (full_func_name.begins_with(prefix.get_ptr()))
                        break;
                }
                else
                {
                    if (full_func_name == m_simple_replay_funcs[i])
                        break;
                }
            }

            if (i == m_simple_replay_funcs.size())
                continue;

            VOGL_ASSERT(!used_flags[i]);
            used_flags[i] = true;

            if (whitelisted_funcs.find(full_func_name) < 0)
            {
                simple_gl_replay_func.push_back(full_func_name);
                whitelisted_funcs.push_back(full_func_name);
            }
            else
            {
                vogl_warning_printf("Simple GL replay func %s was already in the func whitelist\n", full_func_name.get_ptr());
            }
        }

        for (uint32_t i = 0; i < m_simple_replay_funcs.size(); i++)
        {
            if (!used_flags[i])
                vogl_warning_printf("Simple replay func %s was not found/used\n", m_simple_replay_funcs[i].get_ptr());
        }

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // generate_simple_replay_funcs
    //-----------------------------------------------------------------------------------------------------------------------
    bool generate_simple_replay_funcs(const dynamic_string& out_dir, const gl_function_specs &gl_funcs, const gl_string_map &unique_ctype_enums, const gl_string_map &pointee_types, const dynamic_string_array &whitelisted_funcs) const
    {
        dynamic_string_array new_whitelisted_funcs(whitelisted_funcs);

        FILE *pFile = fopen_and_log_generic(out_dir, "gl_glx_cgl_wgl_simple_replay_funcs.inc", "w");
        if (!pFile)
            return false;

        dump_inc_file_header(pFile);

        vogl::vector<bool> used_flags(m_simple_replay_funcs.size());

        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func_def = gl_funcs[func_index];
            dynamic_string full_func_name(func_def.m_full_name); //cVarArg, "%s%s", g_lib_api_prefixes[gl_funcs[func_index].m_lib], gl_funcs[func_index].m_name.get_ptr());

            uint32_t i;
            for (i = 0; i < m_simple_replay_funcs.size(); i++)
            {
                if (m_simple_replay_funcs[i].ends_with("*"))
                {
                    dynamic_string prefix(m_simple_replay_funcs[i]);
                    prefix.left(prefix.get_len() - 1);
                    if (full_func_name.begins_with(prefix.get_ptr()))
                        break;
                }
                else
                {
                    if (full_func_name == m_simple_replay_funcs[i])
                        break;
                }
            }

            if (i == m_simple_replay_funcs.size())
                continue;

            VOGL_ASSERT(!used_flags[i]);
            used_flags[i] = true;

            vogl_fprintf(pFile, "VOGL_SIMPLE_REPLAY_FUNC_BEGIN(%s, func_def.m_params.size())\n", full_func_name.get_ptr());

            for (uint32_t param_index = 0; param_index < func_def.m_params.size(); param_index++)
            {
                const bool last_param = (param_index == (func_def.m_params.size() - 1));

                if (func_def.m_params[param_index].m_ctype_enum.ends_with("_PTR"))
                {
                    gl_string_map::const_iterator it = pointee_types.find(func_def.m_params[param_index].m_ctype_enum);
                    if (it == pointee_types.end())
                    {
                        vogl_error_printf("Failed finding pointee ctype for ctype %s\n", func_def.m_params[param_index].m_ctype.get_ptr());
                        vogl_fclose(pFile);
                        return false;
                    }

                    it = unique_ctype_enums.find(it->second);
                    if (it == unique_ctype_enums.end())
                    {
                        vogl_error_printf("Failed finding pointee ctype for ctype %s\n", func_def.m_params[param_index].m_ctype.get_ptr());
                        vogl_fclose(pFile);
                        return false;
                    }

                    vogl_fprintf(pFile, "   VOGL_SIMPLE_REPLAY_FUNC_PARAM_CLIENT_MEMORY(%s, %u)\n", it->second.get_ptr(), param_index);
                }
                else
                {
                    // TODO: Add expected size of buffer check
                    vogl_fprintf(pFile, "   VOGL_SIMPLE_REPLAY_FUNC_PARAM_VALUE(%s, %u)\n", func_def.m_params[param_index].m_ctype.get_ptr(), param_index);
                }

                if (!last_param)
                    vogl_fprintf(pFile, "   VOGL_SIMPLE_REPLAY_FUNC_PARAM_SEPERATOR\n");
            }

            vogl_fprintf(pFile, "VOGL_SIMPLE_REPLAY_FUNC_END(%s)\n\n", full_func_name.get_ptr());
        }

        vogl_fclose(pFile);

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // generate_replay_func_load_macros
    //-----------------------------------------------------------------------------------------------------------------------
    bool generate_replay_func_load_macros(const dynamic_string& out_dir, const gl_function_specs &gl_funcs, const gl_string_map &unique_ctype_enums, const gl_string_map &pointee_types, const dynamic_string_array &whitelisted_funcs) const
    {
        VOGL_NOTE_UNUSED(unique_ctype_enums);
        VOGL_NOTE_UNUSED(pointee_types);

        dynamic_string_array new_whitelisted_funcs(whitelisted_funcs);

        FILE *pFile = fopen_and_log_generic(out_dir, "gl_glx_cgl_wgl_replay_helper_macros.inc", "w");
        if (!pFile)
            return false;

        dump_inc_file_header(pFile);

        for (uint32_t func_index = 0; func_index < gl_funcs.size(); func_index++)
        {
            const gl_function_def &func_def = gl_funcs[func_index];
            dynamic_string full_func_name(func_def.m_full_name); //cVarArg, "%s%s", g_lib_api_prefixes[gl_funcs[func_index].m_lib], gl_funcs[func_index].m_name.get_ptr());

            vogl_fprintf(pFile, "#define VOGL_REPLAY_LOAD_PARAMS_HELPER_%s %c\n", full_func_name.get_ptr(), func_def.m_params.size() ? '\\' : ' ');

            for (uint32_t param_index = 0; param_index < func_def.m_params.size(); param_index++)
            {
                const bool last_param = (param_index == (func_def.m_params.size() - 1));

                if (func_def.m_params[param_index].m_ctype_enum.ends_with("_PTR"))
                {
#if 0
					gl_string_map::const_iterator it = pointee_types.find(func_def.m_params[param_index].m_ctype_enum);
					if (it == pointee_types.end())
					{
						vogl_error_printf("Failed finding pointee ctype for ctype %s\n", func_def.m_params[param_index].m_ctype.get_ptr());
						vogl_fclose(pFile);
						return false;
					}

					it = unique_ctype_enums.find(it->second);
					if (it == unique_ctype_enums.end())
					{
						vogl_error_printf("Failed finding pointee ctype for ctype %s\n", func_def.m_params[param_index].m_ctype.get_ptr());
						vogl_fclose(pFile);
						return false;
					}
#endif

                    dynamic_string const_str;
                    if (!func_def.m_params[param_index].m_ctype.begins_with("const "))
                        const_str = "const ";

                    vogl_fprintf(pFile, "   %s%s pTrace_%s = reinterpret_cast<%s%s>(trace_packet.get_param_client_memory_ptr(%u));", const_str.get_ptr(), func_def.m_params[param_index].m_ctype.get_ptr(), func_def.m_params[param_index].m_name.get_ptr(), const_str.get_ptr(), func_def.m_params[param_index].m_ctype.get_ptr(), param_index);
                }
                else
                {
                    vogl_fprintf(pFile, "   %s %s = trace_packet.get_param_value<%s>(%u);", func_def.m_params[param_index].m_ctype.get_ptr(), func_def.m_params[param_index].m_name.get_ptr(), func_def.m_params[param_index].m_ctype.get_ptr(), param_index);
                }

                if (!last_param)
                    vogl_fprintf(pFile, " \\\n");
                else
                    vogl_fprintf(pFile, "\n");
            }

            vogl_fprintf(pFile, "\n");

            vogl_fprintf(pFile, "#define VOGL_REPLAY_CALL_GL_HELPER_%s GL_ENTRYPOINT(%s)(", full_func_name.get_ptr(), full_func_name.get_ptr());

            for (uint32_t param_index = 0; param_index < func_def.m_params.size(); param_index++)
            {
                const bool last_param = (param_index == (func_def.m_params.size() - 1));

                if (func_def.m_params[param_index].m_ctype_enum.ends_with("_PTR"))
                {
                    if (func_def.m_params[param_index].m_ctype_enum.begins_with("VOGL_CONST_"))
                        vogl_fprintf(pFile, "pTrace_%s", func_def.m_params[param_index].m_name.get_ptr());
                    else
                        vogl_fprintf(pFile, "pReplay_%s", func_def.m_params[param_index].m_name.get_ptr());
                }
                else
                {
                    vogl_fprintf(pFile, "%s", func_def.m_params[param_index].m_name.get_ptr());
                }

                if (!last_param)
                    vogl_fprintf(pFile, ", ");
            }

            vogl_fprintf(pFile, ")\n\n");
        }

        vogl_fclose(pFile);

        return true;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    // read_regex_function_array
    //-----------------------------------------------------------------------------------------------------------------------
    bool read_regex_function_array(const char *pFilename, dynamic_string_array &funcs)
    {
        dynamic_string_array regex_func_patterns;

        if (!file_utils::read_text_file(pFilename, regex_func_patterns, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFIgnoreCommentedLines | file_utils::cRTFPrintErrorMessages))
            return false;

        regex_func_patterns.unique(dynamic_string_equal_to_case_sensitive());

        vogl::vector<bool> used_flags(regex_func_patterns.size());

        for (uint32_t i = 0; i < m_all_gl_funcs.size(); i++)
        {
            uint32_t j;
            for (j = 0; j < regex_func_patterns.size(); j++)
                if (regexp_full_match(m_all_gl_funcs[i].m_full_name.get_ptr(), regex_func_patterns[j].get_ptr()))
                    break;

            if (j < regex_func_patterns.size())
            {
                funcs.push_back(m_all_gl_funcs[i].m_full_name);
                used_flags[j] = true;
            }
        }

        for (uint32_t i = 0; i < used_flags.size(); i++)
            if (!used_flags[i])
                vogl_warning_printf("Regex pattern \"%s\" from file \"%s\" did not match any function names!\n", regex_func_patterns[i].get_ptr(), pFilename);

        return true;
    }
};

//----------------------------------------------------------------------------------------------------------------------
// init_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static bool init_command_line_params(int argc, char *argv[])
{
    command_line_params::parse_config parse_cfg;
    parse_cfg.m_single_minus_params = true;
    parse_cfg.m_double_minus_params = true;

    if (!g_command_line_params().parse(get_command_line_params(argc, argv), VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, parse_cfg))
    {
        vogl_error_printf("Failed parsing command line parameters!\n");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------------------------------------------------
// check_for_option
//-----------------------------------------------------------------------------------------------------------------------
static bool check_for_option(int argc, char *argv[], const char *pOption)
{
    for (int i = 1; i < argc; i++)
    {
        if ((argv[i][0] == '/') || (argv[i][0] == '-'))
        {
            if (vogl_stricmp(&argv[i][1], pOption) == 0)
                return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------------------------------------------------
// print_title
//-----------------------------------------------------------------------------------------------------------------------
static void print_title()
{
    vogl_printf("voglgen");
    vogl_printf(" %s Built %s, %s", vogl_is_x64() ? "x64" : "x86", __DATE__, __TIME__);
#ifdef VOGL_BUILD_DEBUG
    vogl_printf(" DEBUG build");
#endif
    vogl_printf("\n");
    vogl_printf("Debugger present: %u\n", vogl_is_debugger_present());
}

#if 0
void parse_enums(const tinyxml2::XMLElement *pRegistry, const tinyxml2::XMLElement *pEnums)
{
    while (pEnums)
    {
        // namespace, group, type, vendor, comment
        const char *nmspace = pEnums->Attribute("namespace");
        const char *group = pEnums->Attribute("group");
        const char *type = pEnums->Attribute("type");
        const char *vendor = pEnums->Attribute("vendor");
        const char *comment = pEnums->Attribute("comment");

        printf("pEnums name: %s value: %s namespace: '%s' '%s' '%s' '%s' '%s'\n", pEnums->Name(), pEnums->Value(), nmspace,
               group, type, vendor, comment);
        const tinyxml2::XMLElement *elem = pEnums->FirstChildElement("enum");
        while(elem)
        {
            // value, name, comment
            const char *type = elem->Attribute("name");
            const char *value = elem->Attribute("value");

            printf("  type: %s %s\n", type, value);
            elem = elem->NextSiblingElement("enum");
        }

        pEnums = pEnums->NextSiblingElement("enums");
    }
}

bool parse_gl_xml_file2(const char *pFilename)
{
    tinyxml2::XMLDocument gl_xml;

    tinyxml2::XMLError ret = gl_xml.LoadFile(pFilename);
    if (ret != tinyxml2::XML_SUCCESS)
    {
        vogl_error_printf("Failed loading %s (%d).\n", pFilename, ret);
        return false;
    }

    // from https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/readme.pdf 
    //
    // <registry>
    //    <types>     Defines API types.
    //    <groups>    Named groups of tokens for parameter validation.
    //    <enums>     Defines API enumerants (tokens). Multiple.
    //    <commands>  Defines API functions.
    //    <feature>   Defines API features interfaces (API versions). One tag per feature set.
    //    <extension> Defines API extension interfaces.
    //
    const tinyxml2::XMLElement *pRegistry = gl_xml.RootElement();
    const tinyxml2::XMLElement *pTypes = pRegistry->FirstChildElement("types");
    const tinyxml2::XMLElement *pGroups = pRegistry->FirstChildElement("groups");
    const tinyxml2::XMLElement *pEnums = pRegistry->FirstChildElement("enums");
    const tinyxml2::XMLElement *pCommands = pRegistry->FirstChildElement("commands");
    const tinyxml2::XMLElement *pFeature = pRegistry->FirstChildElement("feature");
    const tinyxml2::XMLElement *pExtensions = pRegistry->FirstChildElement("extensions");

//        tinyxml2::XMLPrinter printer(stdout);
//        pRegistry->Print(&printer);

    printf("\n");
    printf("%p %s\n", pRegistry, pRegistry->Name());
    printf("%p %s\n", pTypes, pTypes ? pTypes->Name() : "??");
    printf("%p %s\n", pGroups, pGroups ? pGroups->Name() : "??");
    printf("%p %s\n", pEnums, pEnums ? pEnums->Name() : "??");
    printf("%p %s\n", pCommands, pCommands ? pCommands->Name() : "??");
    printf("%p %s\n", pFeature, pFeature ? pFeature->Name() : "??");
    printf("%p %s\n", pExtensions, pExtensions ? pExtensions->Name() : "??");
    printf("\n");
    
    parse_enums(pRegistry, pEnums);
    return true;
}
#endif

//-------------------------------------------------------------------------------------------------------------------------------
// main_internal
//-------------------------------------------------------------------------------------------------------------------------------
static int main_internal(int argc, char *argv[])
{
    VOGL_NOTE_UNUSED(argc);
    VOGL_NOTE_UNUSED(argv);
    char cwd[1024];

    cwd[0] = 0;
    colorized_console::init();
    colorized_console::set_exception_callback();

#if 0
    printf("%s\n", get_current_dir_name());
    parse_gl_xml_file2("/home/mikesart/dev/voglproj/vogl_chroot.bitbucket/vogl/glspec/khronos/gl.xml");
#endif

    if (check_for_option(argc, argv, "quiet"))
        console::set_output_level(cMsgError);
    else if (check_for_option(argc, argv, "debug"))
        console::set_output_level(cMsgDebug);
    else if (check_for_option(argc, argv, "verbose"))
        console::set_output_level(cMsgVerbose);

    print_title();

    bool status = false;
    if (init_command_line_params(argc, argv))
    {
        dynamic_string srcdir;

        if (g_command_line_params().get_value_as_string(srcdir, "srcdir"))
        {
            printf("srcdir: '%s'\n", srcdir.get_ptr());
            plat_chdir(srcdir.get_ptr());
        }
        // If we can't find this text file, try the vogl/glspec dir.
        if (!plat_fexist("apitrace_gl_param_info.txt"))
        {
            if (plat_fexist("../../vogl/glspec/apitrace_gl_param_info.txt"))
                plat_chdir("../../vogl/glspec");
        }
        plat_getcwd(cwd, sizeof(cwd));
        printf("Current working directory: %s\n\n", cwd);

        vogl_gen generator;

        vogl_printf("---------------- Begin Generator Init\n");
        status = generator.init();
        vogl_printf("---------------- End Generator Init\n");

        if (status)
        {
            dynamic_string regex_pattern;
            if (g_command_line_params().get_value_as_string(regex_pattern, "func_regex"))
                generator.func_regex(regex_pattern.get_ptr());
            else if (g_command_line_params().get_value_as_string(regex_pattern, "param_regex"))
                generator.param_regex(regex_pattern.get_ptr());
            else if (g_command_line_params().get_value_as_string(regex_pattern, "category_regex"))
                generator.category_regex(regex_pattern.get_ptr());
            else if (g_command_line_params().get_value_as_string(regex_pattern, "namespace_regex"))
                generator.namespace_regex(regex_pattern.get_ptr());
            else if (g_command_line_params().get_value_as_string(regex_pattern, "ctype_regex"))
                generator.ctype_regex(regex_pattern.get_ptr());
            else
            {
                vogl_printf("---------------- Begin Generate\n");
                status = generator.generate();
                vogl_printf("---------------- End Generate\n");

                if (status && g_command_line_params().get_value_as_bool("debug"))
                {
                    vogl_printf("---------------- Begin Debug Dump\n");
                    generator.dump_debug_files();
                    vogl_printf("---------------- End Debug Dump\n");
                }
            }
        }

        // If we generated any errors, report a non-zero exit code.
        status = status && console::get_total_messages(cMsgError) == 0;
    }

    if (!status)
        vogl_error_printf("voglgen FAILED!\n");
    else
        vogl_printf("voglgen succeeded\n  (to %s)\n", cwd);

    int exit_status = status ? EXIT_SUCCESS : EXIT_FAILURE;

    vogl_printf("%u warning(s), %u error(s)\n", console::get_total_messages(cMsgWarning), console::get_total_messages(cMsgError));

    vogl_printf("Exit status: %i\n", exit_status);

    colorized_console::deinit();

    vogl_print_heap_stats();

    return exit_status;
}

//-----------------------------------------------------------------------------------------------------------------------
// pause_and_wait
//-----------------------------------------------------------------------------------------------------------------------
static void pause_and_wait(void)
{
    vogl_printf("\nPress a key to continue.\n");

    for (;;)
    {
        if (vogl_getch() != -1)
            break;
    }
}

//-----------------------------------------------------------------------------------------------------------------------
// fopen_and_log_generic
//-----------------------------------------------------------------------------------------------------------------------
FILE *fopen_and_log_generic(const dynamic_string& outDirectory, const char* filename, const char* mode)
{
    dynamic_string final_path;
    file_utils::combine_path(final_path, outDirectory.c_str(), filename);

    FILE* retfile = vogl_fopen(final_path.c_str(), mode);
    if (retfile)
        vogl_printf("--- Generating \"%s\":\n", final_path.c_str());
    else
        vogl_error_printf("Failed opening file \"%s\" for mode \"%s\"!", final_path.c_str(), mode);

    return retfile;
}

//-----------------------------------------------------------------------------------------------------------------------
// main
//-----------------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int status = EXIT_FAILURE;

    // Initialize vogl_core.
    vogl_core_init();

    if (vogl_is_debugger_present())
    {
        status = main_internal(argc, argv);
    }
    else
    {
#ifdef _MSC_VER
        __try
        {
            status = main_internal(argc, argv);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            vogl_error_printf("Uncaught exception! voglgen command line tool failed!\n");
        }
#else
        status = main_internal(argc, argv);
#endif
    }

    if (check_for_option(argc, argv, "pause"))
    {
        if ((status == EXIT_FAILURE) || (console::get_total_messages(cMsgError)))
            pause_and_wait();
    }

    return status;
}
