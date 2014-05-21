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

// File: vogl_command_line_params.h
#pragma once

#include "vogl_core.h"

// TODO: Get rid of std::multimap! vogl::map can replace this.
#include <map>

namespace vogl
{
    // Returns an array of command line parameters - uses platform specific methods to retrieve them.
    dynamic_string_array get_command_line_params();

    // Returns an array of command line parameters given main()'s argc/argv.
    dynamic_string_array get_command_line_params(int argc, char *argv[]);

    // Returns the command line passed to the app as a single string.
    // On systems where this isn't trivial, this function combines together the separate arguments, quoting and adding spaces as needed.
    dynamic_string get_command_line();

    // Returns the command line passed to the app as a single string.
    // On systems where this isn't trivial, this function combines together the separate arguments, quoting and adding spaces as needed.
    dynamic_string get_command_line(int argc, char *argv[]);

    // Returns true if the specified command line parameter was specified to the app.
    bool check_for_command_line_param(const char *pParam);

    // Splits a string containing the app's command line parameters into an array of seperate parameters.
    bool split_command_line_params(const char *p, dynamic_string_array &params);

    // TODO: Add help text, and a function to print a tool's valid arguments.
    struct command_line_param_desc
    {
        const char *m_pName;
        uint32_t m_num_values;
        bool m_support_listing_file;
        const char *m_pDesc;
    };

    void dump_command_line_info(uint32_t n, const command_line_param_desc *pDesc, const char *prefix = NULL, bool hide_null_descs = false);

    // I'm pretty torn about this class's design. I flip flop between hating it to tolerating it on every update.
    class command_line_params
    {
    public:
        struct param_value
        {
            inline param_value()
                : m_split_param_index(0), m_modifier(0)
            {
            }

            dynamic_string_array m_values;
            uint32_t m_split_param_index;
            int8_t m_modifier;
        };

        // TODO: Get rid of std::multimap!
        typedef std::multimap<dynamic_string, param_value> param_map;
        typedef param_map::const_iterator param_map_const_iterator;
        typedef param_map::iterator param_map_iterator;

        command_line_params();

        void clear();

        struct parse_config
        {
            parse_config()
                : m_skip_first_param(true),
                  m_single_minus_params(true),
                  m_double_minus_params(false),
                  m_ignore_non_params(false),
                  m_ignore_unrecognized_params(false),
                  m_fail_on_non_params(false),
                  m_pParam_ignore_prefix(NULL),
                  m_pParam_accept_prefix(NULL)
            {
            }

            bool m_skip_first_param;
            bool m_single_minus_params;
            bool m_double_minus_params;
            bool m_ignore_non_params;
            bool m_ignore_unrecognized_params;
            bool m_fail_on_non_params; // if true, parsing fails if an non-option argument is specified, excluding the first argument which is the program's path
            const char *m_pParam_ignore_prefix;
            const char *m_pParam_accept_prefix;
        };

        bool parse(const dynamic_string_array &params, uint32_t total_param_descs, const command_line_param_desc *pParam_desc, const parse_config &config);
        bool parse(const char *pCmd_line, uint32_t total_param_descs, const command_line_param_desc *pParam_desc, const parse_config &config);

        const dynamic_string_array &get_split_param_array() const
        {
            return m_params;
        }
        bool is_split_param_an_option(uint32_t split_param_array_index) const;

        const param_map &get_param_map() const
        {
            return m_param_map;
        }

        uint32_t get_num_params() const
        {
            return static_cast<uint32_t>(m_param_map.size());
        }

        param_map_const_iterator begin() const
        {
            return m_param_map.begin();
        }
        param_map_const_iterator end() const
        {
            return m_param_map.end();
        }

        bool find(const char *pKey, param_map_const_iterator &begin, param_map_const_iterator &end) const;

        // Returns the # of command line params matching key (use "" key string for regular/non-option params)
        uint32_t get_count(const char *pKey) const;

        // Returns end() if param cannot be found, or index is out of range.
        param_map_const_iterator get_param(const char *pKey, uint32_t key_index) const;

        bool has_key(const char *pKey) const
        {
            return get_param(pKey, 0) != end();
        }

        bool has_value(const char *pKey, uint32_t key_index) const;
        uint32_t get_num_values(const char *pKey, uint32_t key_index) const;

        bool get_value_as_bool(const char *pKey, uint32_t key_index = 0, bool def = false) const;

        // *pSuccess will be set to false if the value can't parse, or if the key isn't found, otherwise true (even if the value had to be clamped to the specified range)
        int get_value_as_int(const char *pKey, uint32_t key_index = 0, int def = 0, int l = cINT32_MIN, int h = cINT32_MAX, uint32_t value_index = 0, bool *pSuccess = NULL) const;
        int64_t get_value_as_int64(const char *pKey, uint32_t key_index = 0, int64_t def = 0, int64_t l = cINT64_MIN, int64_t h = cINT64_MAX, uint32_t value_index = 0, bool *pSuccess = NULL) const;
        uint32_t get_value_as_uint(const char *pKey, uint32_t key_index = 0, uint32_t def = 0, uint32_t l = 0, uint32_t h = cUINT32_MAX, uint32_t value_index = 0, bool *pSuccess = NULL) const;
        uint64_t get_value_as_uint64(const char *pKey, uint32_t key_index = 0, uint64_t def = 0, uint64_t l = 0, uint64_t h = cUINT64_MAX, uint32_t value_index = 0, bool *pSuccess = NULL) const;
        float get_value_as_float(const char *pKey, uint32_t key_index = 0, float def = 0.0f, float l = -math::cNearlyInfinite, float h = math::cNearlyInfinite, uint32_t value_index = 0, bool *pSuccess = NULL) const;

        bool get_value_as_string(dynamic_string &value, const char *pKey, uint32_t key_index = 0, const char *pDef = "", uint32_t value_index = 0) const;
        dynamic_string get_value_as_string(const char *pKey, uint32_t key_index = 0, const char *pDef = "", uint32_t value_index = 0) const;
        const dynamic_string &get_value_as_string_or_empty(const char *pKey, uint32_t key_index = 0, uint32_t value_index = 0) const;

    private:
        dynamic_string_array m_params;

        param_map m_param_map;

        static bool load_string_file(const char *pFilename, dynamic_string_array &strings);
    };

    inline command_line_params &g_command_line_params()
    {
        static command_line_params s_command_line_params;
        return s_command_line_params;
    }

} // namespace vogl
