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

// File: vogl_fs_preprocessor.cpp
#include "vogl_fs_preprocessor.h"
#include "vogl_file_utils.h"

vogl_fs_preprocessor* vogl_fs_preprocessor::m_instance = 0;

vogl_fs_preprocessor::vogl_fs_preprocessor()
    : m_pp_cmd(""),
      m_pp_opts(""),
      m_pp_prefix(""),
      m_count(0),
      m_length(0),
      m_in_length(0),
      m_shader_id(0),
      m_null_shader_id(0),
      m_in_shader(""),
      m_output_shader(""),
      m_null(false),
      m_tm(0),
      m_time_to_run_pp(0),
      m_enabled(false)
{
    VOGL_FUNC_TRACER
    m_null_color[0] = 0.6275;
    m_null_color[1] = 0.1255;
    m_null_color[2] = 0.9412;
    m_null_color[3] = 1;
    m_tm.start();
    atexit(&cleanup);
}

void vogl_fs_preprocessor::cleanup()
{
    delete m_instance;
    m_instance = 0;
}

vogl_fs_preprocessor::~vogl_fs_preprocessor()
{
    VOGL_FUNC_TRACER
}

void vogl_fs_preprocessor::reset()
{
    m_pp_cmd = "";
    m_pp_opts = "";
    m_pp_prefix = "";
    m_count = 0;
    m_length = 0;
    m_in_length = 0;
    m_in_shader = "";
    m_shader_id = 0,
    m_null_shader_id = 0,
    m_output_shader = "";
    m_null = false;
    m_null_color[0] = 0.6275;
    m_null_color[1] = 0.1255;
    m_null_color[2] = 0.9412;
    m_null_color[3] = 1;
    m_tm.stop();
    m_tm.start();
    m_time_to_run_pp = 0;
    m_enabled = false;
}

// Internal class function wrapped by public run() call so that we can easily time how long pp takes
bool vogl_fs_preprocessor::_run()
{
    // For NULL case, null FS output prior to PP run
    //   Also verify any Shader ID
    if (m_null)
    {
        if ((0 != m_null_shader_id) && (m_shader_id != m_null_shader_id))
        {
            m_output_shader = m_in_shader;
            m_count = 1;
            m_length = m_output_shader.get_len();
            return true;
        }
        m_in_shader = _set_null_output_color(m_in_shader);
    }
    // Write input shader to a file to feed into pp
    //dynamic_string in_fs_filename("tmp_shader_in.frag");
    dynamic_string in_fs_filename("tmp_shader_in.frag");
    if (m_pp_prefix.size() > 0)
        in_fs_filename = dynamic_string(cVarArg, "%sshader_in_%i.frag", m_pp_prefix.get_ptr(), m_shader_id);

    dynamic_string_array shader_strings;
    shader_strings.push_back(m_in_shader);
    bool success = file_utils::write_text_file(in_fs_filename.get_ptr(), shader_strings, true);
    if (success)
    {
        dynamic_string out_shader_filename("tmp_shader_out.frag");
        if (m_pp_prefix.size() > 0)
            out_shader_filename = dynamic_string(cVarArg, "%sshader_out_%i.frag", m_pp_prefix.get_ptr(), m_shader_id);

        //const dynamic_string out_shader_filename("tmp_shader_out.frag");
        dynamic_string pp_cmd(cVarArg, "%s %s %s 1>%s", m_pp_cmd.get_ptr(), m_pp_opts.get_ptr(), in_fs_filename.get_ptr(), out_shader_filename.get_ptr());
        // TODO : Is "system" best option here?  Add improved error handling.
        system(pp_cmd.get_ptr());
        dynamic_string_array updated_fs_lines;
        if (file_utils::read_text_file(out_shader_filename.get_ptr(), updated_fs_lines, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFPrintErrorMessages))
        {
            // TODO : Error handling here to parse for error cases in the pre-processor output
            // We successfully read updated shader so now use updated data
            m_count = updated_fs_lines.size();
            // When trying to use multi-line shader string as read into updated_fs_lines there were
            //   various compile errors so for now we just smash all shader lines into a single string
            m_output_shader.clear();
            for (GLsizei i = 0; i < m_count; i++)
            {
                m_output_shader.append(updated_fs_lines[i].get_ptr());
                m_output_shader.append("\n");
            }
            m_count = 1;
            m_length = m_output_shader.get_len();
        }
        else
        {
            vogl_error_printf("Could not read in new FS that was written out by FS pre-processor.\n");
            return false;
        }
    }
    else
    {
        vogl_error_printf("Could not write out FS to a file to input into FS pre-processor.\n");
        return false;
    }
    return true;
}

bool vogl_fs_preprocessor::run()
{
    double pp_start = m_tm.get_elapsed_secs();
    bool status = _run();
    m_time_to_run_pp += m_tm.get_elapsed_secs() - pp_start;
    return status;
}

void vogl_fs_preprocessor::set_shader(GLuint id, vogl::vector<const GLcharARB *> in_shader_vec, vogl::vector<GLint> lengths)
{
    m_shader_id = id;
    m_in_shader.clear();
    for (unsigned int i = 0; i < in_shader_vec.size(); i++)
    {
        m_in_shader.append(dynamic_string(in_shader_vec[i], lengths[i]));
        m_in_shader.append("\n");
    }
}

// This function performs null shader substitution by setting FS output color to constant color
//   The shader will then be cleaned up when run through LunarGOO as the preprocessor
// TODO : This code is pretty fragile but works on TF2 and Dota2 tests for now
//   Ideally would like to offload the NULL (and any other capabilities) into the FSPP
dynamic_string vogl_fs_preprocessor::_set_null_output_color(dynamic_string in_shader)
{
    // For the input shader, find "gl_FragColor *;" section and replace with null color
    // Parse in_shader and find gl_FragColor
    vogl::vector<const char *> replace_strings(5); // in case we have to replace multiple color outputs
    replace_strings[0] = "gl_FragColor";
    replace_strings[1] = "gl_FragData[0]";
    replace_strings[2] = "gl_FragData[1]";
    replace_strings[3] = "gl_FragData[2]";
    replace_strings[4] = "gl_FragData[3]";
    dynamic_string new_out_color = dynamic_string(cVarArg, "REPLACE_ME = vec4(%1.4f, %1.4f, %1.4f, %1.4f);", m_null_color[0], m_null_color[1], m_null_color[2], m_null_color[3]);
    dynamic_string replace_me = ""; // the string that we'll replace
    uint32_t num_found = 0;
    int32_t out_color_index = in_shader.find_right("gl_FragColor.a =", true);
    // Special case when glFragColor.a/.rgb are set independently for now
    if (out_color_index >= 0)
    {
        while (';' != in_shader[out_color_index])
            replace_me.append_char(in_shader[out_color_index++]);
        replace_me.append_char(';');
        // Verify that we can do rgb substitution before we replace alpha
        out_color_index = in_shader.find_right("gl_FragColor.rgb", true);
        if (out_color_index < 0)
        {
            vogl_error_printf("Unable to do NULL substitution for FS %i\n", m_shader_id);
            VOGL_ASSERT_ALWAYS;
        }
        // Just remove gl_FragColor.a line and merge NULL into gl_FragColor.rgb line
        in_shader.replace(replace_me.get_ptr(), "", true, &num_found, 1);
        new_out_color.replace("REPLACE_ME", replace_strings[0]);
        while (';' != in_shader[out_color_index])
            replace_me.append_char(in_shader[out_color_index++]);
        replace_me.append_char(';');
        in_shader.replace(replace_me.get_ptr(), new_out_color.get_ptr(), true, &num_found, 1);
    }
    else
    {
        for (uint32_t count = 0; count < replace_strings.size(); count++)
        {
            out_color_index = in_shader.find_right(replace_strings[count], true);
            if (out_color_index >= 0)
            {
                // need to make this replacement
                new_out_color.replace("REPLACE_ME", replace_strings[count]);
                // identify exact string we're replacing
                while (';' != in_shader[out_color_index])
                    replace_me.append_char(in_shader[out_color_index++]);
                replace_me.append_char(';');
                in_shader.replace(replace_me.get_ptr(), new_out_color.get_ptr(), true, &num_found, 1);
                if (num_found != 1)
                {
                    vogl_warning_printf("Failed to find shader output to replace null shader output\n");
                }
                // reset replace strings for next pass
                replace_me.clear();
                new_out_color.replace(replace_strings[count], "REPLACE_ME", true, &num_found, 1);
            }
        }
    }
    return in_shader;
}

// This is a bit of a work-around that allows us to pull certain options off
//   of the PP cmd line options and handle them prior to passing further options
//   onto the PP.  Right now this handles NULL_FS processing.
//   Default NULL color is bright purple.
// If "NULL" option found, will be of format NULL_FS[FS_ID]:<#>,<#>,<#>,<#>
// Here are some examples of ways to use NULL options:
//      "NULL_FS" will cause all FSs to be nulled out
//      "NULL_FS[464]" will only null out only FS with trace handle "464"
//      "NULL_FS:1.0,0.0,0.0,1.0" will NULL all shaders out to red
//      "NULL_FS[789]:0.0,0.0,1.0,1.0" will NULL out shader with handle "789" to blue
dynamic_string vogl_fs_preprocessor::_check_opts(const dynamic_string &in_opts)
{
    dynamic_string working_str = in_opts;
    // parse the options and pull off any "internal" options that we deal with ourselves
    if (working_str.contains("NULL_FS", true))
    {
        dynamic_string null_str = "";
        dynamic_string color_str = "";
        dynamic_string shader_id_str = "";
        int ns_start_index = working_str.find_left("NULL_FS");
        int ns_index = ns_start_index;
        unsigned int null_color_index = 0;
        bool store_color = false;
        bool store_id = false;
        // Pull NULL str out of opts
        while (working_str[ns_index] && working_str[ns_index] != ' ')
        {
            null_str.append_char(working_str[ns_index]);
            // if we're in a color section, store color at appropriate index
            if (store_color)
            {
                if (working_str[ns_index] == ',')
                {
                    m_null_color[null_color_index++] = strtof(color_str.get_ptr(), NULL);
                    color_str.clear();
                }
                else
                    color_str.append_char(working_str[ns_index]);
            }
            if (store_id)
            {
                if (working_str[ns_index] == ']')
                {
                    m_null_shader_id = atoi(shader_id_str.get_ptr());
                    store_id = false;
                }
                else
                    shader_id_str.append_char(working_str[ns_index]);
            }
            if (':' == working_str[ns_index])
                store_color = true;
            if ('[' == working_str[ns_index])
                store_id = true;
            ns_index++;
        }
        if (store_color)
            m_null_color[null_color_index] = strtof(color_str.get_ptr(), NULL);
        working_str.remove(ns_start_index, null_str.size());
        //   enable null FS
        m_null = true;
    }
    return working_str;
}
