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
      m_count(0),
      m_length(0),
      m_in_length(0),
      m_in_shader(""),
      m_output_shader(""),
      m_tm(0),
      m_time_to_run_pp(0),
      m_enabled(false)
{
    VOGL_FUNC_TRACER
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
    m_count = 0;
    m_length = 0;
    m_in_length = 0;
    m_in_shader = "";
    m_output_shader = "";
    m_tm.stop();
    m_tm.start();
    m_time_to_run_pp = 0;
    m_enabled = false;
}

// Internal class function wrapped by public run() call so that we can easily time how long pp takes
bool vogl_fs_preprocessor::_run()
{
    // Write input shader to a file to feed into pp
    dynamic_string in_fs_filename("tmp_shader_in.frag");
    dynamic_string_array shader_strings;
    shader_strings.push_back(m_in_shader);
    bool success = file_utils::write_text_file(in_fs_filename.get_ptr(), shader_strings, true);
    if (success)
    {
        //const dynamic_string out_shader_filename(cVarArg, "tmp_shader_out_%i.frag", replay_object);
        const dynamic_string out_shader_filename("tmp_shader_out.frag");
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

void vogl_fs_preprocessor::set_shader(vogl::vector<const GLcharARB *> in_shader_vec, vogl::vector<GLint> lengths)
{
    m_in_shader.clear();
    for (unsigned int i = 0; i < in_shader_vec.size(); i++)
    {
        m_in_shader.append(dynamic_string(in_shader_vec[i], lengths[i]));
        m_in_shader.append("\n");
    }
}
