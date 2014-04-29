/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
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

// File: vogl_command_line_params.cpp
#include "vogl_core.h"
#include "vogl_command_line_params.h"
#include "vogl_console.h"
#include "vogl_cfile_stream.h"
#include "vogl_strutils.h"

#ifdef PLATFORM_WINDOWS
#define VOGL_CMD_LINE_ALLOW_SLASH_PARAMS 1
#endif

#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

#ifdef VOGL_USE_LINUX_API
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace vogl
{
#if defined(VOGL_USE_LINUX_API)
    static const char *get_proc_cmdline()
    {
        static bool s_inited = false;
        static vogl::vector<char> s_buf;

        if (!s_inited)
        {
            s_inited = true;

            FILE *pFile = vogl_fopen("/proc/self/cmdline", "rb");
            if (pFile)
            {
                for (;;)
                {
                    int c = vogl_fgetc(pFile);
                    if (c < 0)
                        break;
                    s_buf.push_back(static_cast<char>(c));
                }
                vogl_fclose(pFile);
            }

            s_buf.push_back(0);
            s_buf.push_back(0);
        }

        return s_buf.get_const_ptr();
    }
#endif 

    dynamic_string_array get_command_line_params(int argc, char *argv[])
    {
        dynamic_string_array params;
        for (int i = 0; i < argc; i++)
            params.push_back(dynamic_string(argv[i]));
        return params;
    }

    dynamic_string_array get_command_line_params()
    {
        dynamic_string_array params;

#ifdef VOGL_USE_WIN32_API
        dynamic_string cmd_line;
        split_command_line_params(get_command_line().c_str(), params);
#else
        const char *pSrc = get_proc_cmdline();
        while (*pSrc)
        {
            params.push_back(pSrc);
            pSrc += strlen(pSrc) + 1;
        }
#endif

        return params;
    }

    dynamic_string get_command_line()
    {
        dynamic_string cmd_line;

#ifdef VOGL_USE_WIN32_API
        cmd_line.set(GetCommandLineA());
#else
        dynamic_string_array params(get_command_line_params());

        for (uint i = 0; i < params.size(); i++)
        {
            dynamic_string tmp(params[i]);

            // If the param is not already quoted, and it has any whitespace, then quote it. (The goal here is to ensure the split_command_line_params() method,
            // which was unfortunately written for Windows where it's trivial to get the full unparsed cmd line as a string, doesn't split up this parameter.)
            if ((tmp.front() != '\"') && (tmp.contains(' ') || tmp.contains('\t')))
                tmp = "\"" + tmp + "\"";

            if (cmd_line.get_len())
                cmd_line += " ";

            cmd_line += tmp;
        }
#endif
        return cmd_line;
    }

    dynamic_string get_command_line(int argc, char *argv[])
    {
        dynamic_string cmd_line;

#ifdef VOGL_USE_WIN32_API
        (void)argc, (void)argv;

        cmd_line.set(GetCommandLineA());
#else
        cmd_line.clear();
        for (int i = 0; i < argc; i++)
        {
            dynamic_string tmp(argv[i]);

            // If the param is not already quoted, and it has any whitespace, then quote it.
            if ((tmp.front() != '\"') && (tmp.contains(' ') || tmp.contains('\t')))
                tmp = "\"" + tmp + "\"";

            if (cmd_line.get_len())
                cmd_line += " ";
            cmd_line += tmp;
        }
#endif
        return cmd_line;
    }

    bool check_for_command_line_param(const char *pParam)
    {
#ifdef VOGL_USE_WIN32_API
        return (strstr(GetCommandLineA(), pParam) != NULL);
#else
        const char *pSrc = get_proc_cmdline();
        while (*pSrc)
        {
            if (!strcmp(pParam, pSrc))
                return true;
            pSrc += strlen(pSrc) + 1;
        }
#endif

        return false;
    }

    bool split_command_line_params(const char *p, dynamic_string_array &params)
    {
        bool within_param = false;
        bool within_quote = false;

        uint ofs = 0;
        dynamic_string str;

        while (p[ofs])
        {
            const char c = p[ofs];

            if (within_param)
            {
                if (within_quote)
                {
                    if (c == '"')
                        within_quote = false;

                    str.append_char(c);
                }
                else if ((c == ' ') || (c == '\t'))
                {
                    if (!str.is_empty())
                    {
                        params.push_back(str);
                        str.clear();
                    }
                    within_param = false;
                }
                else
                {
                    if (c == '"')
                        within_quote = true;

                    str.append_char(c);
                }
            }
            else if ((c != ' ') && (c != '\t'))
            {
                within_param = true;

                if (c == '"')
                    within_quote = true;

                str.append_char(c);
            }

            ofs++;
        }

        if (within_quote)
        {
            console::error("Unmatched quote in command line \"%s\"\n", p);
            return false;
        }

        if (!str.is_empty())
            params.push_back(str);
        return true;
    }

    void dump_command_line_info(uint n, const command_line_param_desc *pDesc, const char *prefix)
    {
        if (!prefix)
            prefix = "";
        for (uint i = 0; i < n; i++)
        {
            console::message("%s%s", prefix, pDesc[i].m_pName);
            for (uint j = 0; j < pDesc[i].m_num_values; j++)
                console::message(" [value]");

            if (pDesc[i].m_pDesc)
                console::printf(": %s", pDesc[i].m_pDesc);

            console::printf("\n");
        }
    }

    command_line_params::command_line_params()
    {
    }

    void command_line_params::clear()
    {
        m_params.clear();

        m_param_map.clear();
    }

    bool command_line_params::load_string_file(const char *pFilename, dynamic_string_array &strings)
    {
        cfile_stream in_stream;
        if (!in_stream.open(pFilename, cDataStreamReadable | cDataStreamSeekable))
        {
            console::error("Unable to open file \"%s\" for reading!\n", pFilename);
            return false;
        }

        dynamic_string ansi_str;

        for (;;)
        {
            if (!in_stream.read_line(ansi_str))
                break;

            ansi_str.trim();
            if (ansi_str.is_empty())
                continue;

            strings.push_back(dynamic_string(ansi_str.get_ptr()));
        }

        return true;
    }

    bool command_line_params::parse(const dynamic_string_array &params, uint total_param_descs, const command_line_param_desc *pParam_desc, const command_line_params::parse_config &config)
    {
        m_params = params;

        command_line_param_desc desc;
        desc.m_num_values = 0;
        desc.m_support_listing_file = false;
        desc.m_pName = "";
        desc.m_pDesc = NULL;

        uint arg_index = 0;
        while (arg_index < params.size())
        {
            const uint cur_arg_index = arg_index;
            const dynamic_string &src_param = params[arg_index++];

            if (src_param.is_empty())
                continue;

            bool is_param = false;
            uint param_prefix_chars = 1;

#if VOGL_CMD_LINE_ALLOW_SLASH_PARAMS
            is_param = (src_param[0] == '/');
#endif
            if ((src_param[0] == '-') && ((config.m_single_minus_params) || (config.m_double_minus_params)))
            {
                is_param = true;

                bool double_minus = (src_param[1] == '-');
                if (double_minus)
                {
                    if (config.m_double_minus_params)
                    {
                        param_prefix_chars = 2;
                    }
                    else
                    {
                        if (config.m_ignore_unrecognized_params)
                            continue;

                        console::error("Unrecognized command line parameter: \"%s\"\n", src_param.get_ptr());
                        return false;
                    }
                }
            }

            if (is_param)
            {
                if (src_param.get_len() < (param_prefix_chars + 1))
                {
                    console::warning("Skipping invalid command line parameter: \"%s\"\n", src_param.get_ptr());
                    continue;
                }

                dynamic_string key_str(src_param);

                key_str.right(param_prefix_chars);

                if (config.m_pParam_ignore_prefix)
                {
                    if (key_str.begins_with(config.m_pParam_ignore_prefix, true))
                        continue;
                }

                if (config.m_pParam_accept_prefix)
                {
                    if (!key_str.begins_with(config.m_pParam_accept_prefix, true))
                        continue;
                }

                int modifier = 0;
                char c = key_str[key_str.get_len() - 1];
                if (c == '+')
                    modifier = 1;
                else if (c == '-')
                    modifier = -1;

                if (modifier)
                    key_str.left(key_str.get_len() - 1);

                if (total_param_descs)
                {
                    uint param_index;
                    for (param_index = 0; param_index < total_param_descs; param_index++)
                        if (key_str == pParam_desc[param_index].m_pName)
                            break;

                    if (param_index == total_param_descs)
                    {
                        if (config.m_ignore_unrecognized_params)
                            continue;

                        console::error("Unrecognized command line parameter: \"%s\"\n", src_param.get_ptr());
                        return false;
                    }

                    desc = pParam_desc[param_index];
                }

                const uint cMaxValues = 16;
                dynamic_string val_str[cMaxValues];
                uint num_val_strs = 0;
                if (desc.m_num_values)
                {
                    VOGL_ASSERT(desc.m_num_values <= cMaxValues);

                    if ((arg_index + desc.m_num_values) > params.size())
                    {
                        console::error("Expected %u value(s) after command line parameter: \"%s\"\n", desc.m_num_values, src_param.get_ptr());
                        return false;
                    }

                    for (uint v = 0; v < desc.m_num_values; v++)
                    {
                        val_str[num_val_strs++] = params[arg_index++];
                    }
                }

                dynamic_string_array strings;

                if ((desc.m_support_listing_file) && (val_str[0].get_len() >= 2) && (val_str[0][0] == '@'))
                {
                    dynamic_string filename(val_str[0]);
                    filename.right(1);
                    filename.unquote();

                    if (!load_string_file(filename.get_ptr(), strings))
                    {
                        console::error("Failed loading listing file \"%s\"!\n", filename.get_ptr());
                        return false;
                    }
                }
                else
                {
                    for (uint v = 0; v < num_val_strs; v++)
                    {
                        val_str[v].unquote();
                        strings.push_back(val_str[v]);
                    }
                }

                param_value pv;
                pv.m_values.swap(strings);
                pv.m_split_param_index = cur_arg_index;
                pv.m_modifier = (int8)modifier;
                m_param_map.insert(std::make_pair(key_str, pv));
            }
            else if (!config.m_ignore_non_params)
            {
                if ((config.m_fail_on_non_params) && (cur_arg_index))
                {
                    console::error("Unrecognized command line argument: \"%s\"!\n", src_param.get_ptr());
                    return false;
                }

                param_value pv;
                pv.m_values.push_back(src_param);
                pv.m_values.back().unquote();
                pv.m_split_param_index = cur_arg_index;
                m_param_map.insert(std::make_pair(get_empty_dynamic_string(), pv));
            }
        }

        return true;
    }

    bool command_line_params::parse(const char *pCmd_line, uint total_param_descs, const command_line_param_desc *pParam_desc, const command_line_params::parse_config &config)
    {
        dynamic_string_array p;
        if (!split_command_line_params(pCmd_line, p))
            return 0;

        if (p.is_empty())
            return 0;

        if (config.m_skip_first_param)
            p.erase(0U);

        return parse(p, total_param_descs, pParam_desc, config);
    }

    bool command_line_params::is_split_param_an_option(uint split_param_array_index) const
    {
        VOGL_ASSERT(split_param_array_index < m_params.size());
        if (split_param_array_index >= m_params.size())
            return false;

        const dynamic_string &w = m_params[split_param_array_index];
        if (w.is_empty())
            return false;

#if VOGL_CMD_LINE_ALLOW_SLASH_PARAMS
        return (w.get_len() >= 2) && ((w[0] == '-') || (w[0] == '/'));
#else
        return (w.get_len() >= 2) && (w[0] == '-');
#endif
    }

    bool command_line_params::find(const char *pKey, param_map_const_iterator &begin, param_map_const_iterator &end) const
    {
        dynamic_string key(pKey);
        begin = m_param_map.lower_bound(key);
        end = m_param_map.upper_bound(key);
        return begin != end;
    }

    uint command_line_params::get_count(const char *pKey) const
    {
        param_map_const_iterator begin, end;
        find(pKey, begin, end);

        uint n = 0;

        while (begin != end)
        {
            n++;
            begin++;
        }

        return n;
    }

    command_line_params::param_map_const_iterator command_line_params::get_param(const char *pKey, uint key_index) const
    {
        param_map_const_iterator begin, end;
        find(pKey, begin, end);

        if (begin == end)
            return m_param_map.end();

        uint n = 0;

        while ((begin != end) && (n != key_index))
        {
            n++;
            begin++;
        }

        if (begin == end)
            return m_param_map.end();

        return begin;
    }

    bool command_line_params::has_value(const char *pKey, uint key_index) const
    {
        return get_num_values(pKey, key_index) != 0;
    }

    uint command_line_params::get_num_values(const char *pKey, uint key_index) const
    {
        param_map_const_iterator it = get_param(pKey, key_index);

        if (it == end())
            return 0;

        return it->second.m_values.size();
    }

    bool command_line_params::get_value_as_bool(const char *pKey, uint key_index, bool def) const
    {
        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return def;

        if (it->second.m_modifier)
            return it->second.m_modifier > 0;
        else
            return true;
    }

    int command_line_params::get_value_as_int(const char *pKey, uint key_index, int def, int l, int h, uint value_index, bool *pSuccess) const
    {
        if (pSuccess)
            *pSuccess = false;

        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return def;
        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            return def;
        }

        int val;
        const char *p = it->second.m_values[value_index].get_ptr();
        if (!string_ptr_to_int(p, val))
        {
            if (!pKey[0])
                vogl::console::warning("Non-integer value specified for parameter at index %u, using default value of %i\n", key_index, def);
            else
                vogl::console::warning("Non-integer value specified for parameter \"%s\" at index %u, using default value of %i\n", pKey, key_index, def);
            return def;
        }

        if (val < l)
        {
            vogl::console::warning("Value %i for parameter \"%s\" at index %u is out of range, clamping to %i\n", val, pKey, key_index, l);
            val = l;
        }
        else if (val > h)
        {
            vogl::console::warning("Value %i for parameter \"%s\" at index %u is out of range, clamping to %i\n", val, pKey, key_index, h);
            val = h;
        }

        if (pSuccess)
            *pSuccess = true;

        return val;
    }

    int64_t command_line_params::get_value_as_int64(const char *pKey, uint key_index, int64_t def, int64_t l, int64_t h, uint value_index, bool *pSuccess) const
    {
        if (pSuccess)
            *pSuccess = false;

        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return def;
        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            return def;
        }

        int64_t val;
        const char *p = it->second.m_values[value_index].get_ptr();
        if (!string_ptr_to_int64(p, val))
        {
            if (!pKey[0])
                vogl::console::warning("Non-integer value specified for parameter at index %u, using default value of %" PRIi64 "\n", key_index, def);
            else
                vogl::console::warning("Non-integer value specified for parameter \"%s\" at index %u, using default value of %" PRIi64 "\n", pKey, key_index, def);
            return def;
        }

        if (val < l)
        {
            vogl::console::warning("Value %" PRIi64 " for parameter \"%s\" at index %u is out of range, clamping to %" PRIi64 "\n", val, pKey, key_index, l);
            val = l;
        }
        else if (val > h)
        {
            vogl::console::warning("Value %" PRIi64 " for parameter \"%s\" at index %u is out of range, clamping to %" PRIi64 "\n", val, pKey, key_index, h);
            val = h;
        }

        if (pSuccess)
            *pSuccess = true;

        return val;
    }

    uint command_line_params::get_value_as_uint(const char *pKey, uint key_index, uint def, uint l, uint h, uint value_index, bool *pSuccess) const
    {
        if (pSuccess)
            *pSuccess = false;

        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return def;
        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            return def;
        }

        uint val;
        const char *p = it->second.m_values[value_index].get_ptr();
        if (!string_ptr_to_uint(p, val))
        {
            if (!pKey[0])
                vogl::console::warning("Non-integer value specified for parameter at index %u, using default value of %u\n", key_index, def);
            else
                vogl::console::warning("Non-integer value specified for parameter \"%s\" at index %u, using default value of %u\n", pKey, key_index, def);
            return def;
        }

        if (val < l)
        {
            vogl::console::warning("Value %u for parameter \"%s\" at index %u is out of range, clamping to %u\n", val, pKey, key_index, l);
            val = l;
        }
        else if (val > h)
        {
            vogl::console::warning("Value %u for parameter \"%s\" at index %u is out of range, clamping to %u\n", val, pKey, key_index, h);
            val = h;
        }

        if (pSuccess)
            *pSuccess = true;

        return val;
    }

    uint64_t command_line_params::get_value_as_uint64(const char *pKey, uint key_index, uint64_t def, uint64_t l, uint64_t h, uint value_index, bool *pSuccess) const
    {
        if (pSuccess)
            *pSuccess = false;

        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return def;
        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            return def;
        }

        uint64_t val;
        const char *p = it->second.m_values[value_index].get_ptr();
        if (!string_ptr_to_uint64(p, val))
        {
            if (!pKey[0])
                vogl::console::warning("Non-integer value specified for parameter at index %u, using default value of %" PRIu64 "\n", key_index, def);
            else
                vogl::console::warning("Non-integer value specified for parameter \"%s\" at index %u, using default value of %" PRIu64 "\n", pKey, key_index, def);
            return def;
        }

        if (val < l)
        {
            vogl::console::warning("Value %" PRIu64 " for parameter \"%s\" at index %u is out of range, clamping to %" PRIu64 "\n", val, pKey, key_index, l);
            val = l;
        }
        else if (val > h)
        {
            vogl::console::warning("Value %" PRIu64 " for parameter \"%s\" at index %u is out of range, clamping to %" PRIu64 "\n", val, pKey, key_index, h);
            val = h;
        }

        if (pSuccess)
            *pSuccess = true;

        return val;
    }

    float command_line_params::get_value_as_float(const char *pKey, uint key_index, float def, float l, float h, uint value_index, bool *pSuccess) const
    {
        if (pSuccess)
            *pSuccess = false;

        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return def;
        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            return def;
        }

        float val;
        const char *p = it->second.m_values[value_index].get_ptr();
        if (!string_ptr_to_float(p, val))
        {
            vogl::console::warning("Invalid value specified for float parameter \"%s\", using default value of %f\n", pKey, def);
            return def;
        }

        // Let's assume +-cNearlyInfinite implies no clamping.
        if ((l != -math::cNearlyInfinite) && (val < l))
        {
            vogl::console::warning("Value %f for parameter \"%s\" is out of range, clamping to %f\n", val, pKey, l);
            val = l;
        }
        else if ((h != math::cNearlyInfinite) && (val > h))
        {
            vogl::console::warning("Value %f for parameter \"%s\" is out of range, clamping to %f\n", val, pKey, h);
            val = h;
        }

        if (pSuccess)
            *pSuccess = true;

        return val;
    }

    bool command_line_params::get_value_as_string(dynamic_string &value, const char *pKey, uint key_index, const char *pDef, uint value_index) const
    {
        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return false;
        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            value.set(pDef);
            return false;
        }

        value = it->second.m_values[value_index];
        return true;
    }

    dynamic_string command_line_params::get_value_as_string(const char *pKey, uint key_index, const char *pDef, uint value_index) const
    {
        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return dynamic_string(pDef);
        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            return dynamic_string(pDef);
        }

        return it->second.m_values[value_index];
    }

    const dynamic_string &command_line_params::get_value_as_string_or_empty(const char *pKey, uint key_index, uint value_index) const
    {
        param_map_const_iterator it = get_param(pKey, key_index);
        if (it == end())
            return get_empty_dynamic_string();

        if (value_index >= it->second.m_values.size())
        {
            vogl::console::debug("%s: Trying to retrieve value %u of command line parameter %s, but this parameter only has %u values\n", VOGL_METHOD_NAME, value_index, pKey, it->second.m_values.size());
            return get_empty_dynamic_string();
        }

        return it->second.m_values[value_index];
    }

} // namespace vogl
