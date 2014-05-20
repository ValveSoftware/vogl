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

// File: vogl_tool_json.cpp
#include "vogl_common.h"
#include "vogl_gl_replayer.h"
#include "vogl_json.h"

//----------------------------------------------------------------------------------------------------------------------
// tool_unpack_json_mode
//----------------------------------------------------------------------------------------------------------------------
bool tool_unpack_json_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    if (desc)
    {
        // desc->append(g_command_line_param_descs_unpack_json, VOGL_ARRAY_SIZE(g_command_line_param_descs_unpack_json));
        return true;
    }

    dynamic_string input_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input UBJ file!\n");
        return false;
    }

    dynamic_string output_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of output text file!\n");
        return false;
    }

    json_document doc;
    vogl_message_printf("Reading UBJ file \"%s\"\n", input_filename.get_ptr());

    if (!doc.binary_deserialize_file(input_filename.get_ptr()))
    {
        vogl_error_printf("Unable to deserialize input file \"%s\"!\n", input_filename.get_ptr());
        if (doc.get_error_msg().has_content())
            vogl_error_printf("%s\n", doc.get_error_msg().get_ptr());
        return false;
    }

    if (!doc.serialize_to_file(output_filename.get_ptr()))
    {
        vogl_error_printf("Failed serializing to output file \"%s\"!\n", output_filename.get_ptr());
        return false;
    }

    vogl_message_printf("Wrote textual JSON file to \"%s\"\n", output_filename.get_ptr());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_pack_json_mode
//----------------------------------------------------------------------------------------------------------------------
bool tool_pack_json_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_FUNC_TRACER

    if (desc)
    {
        // desc->append(g_command_line_param_descs_pack_json, VOGL_ARRAY_SIZE(g_command_line_param_descs_pack_json));
        return true;
    }

    dynamic_string input_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input text file!\n");
        return false;
    }

    dynamic_string output_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of output UBJ file!\n");
        return false;
    }

    json_document doc;
    vogl_message_printf("Reading JSON text file \"%s\"\n", input_filename.get_ptr());

    if (!doc.deserialize_file(input_filename.get_ptr()))
    {
        vogl_error_printf("Unable to deserialize input file \"%s\"!\n", input_filename.get_ptr());
        if (doc.get_error_msg().has_content())
            vogl_error_printf("%s (Line: %u)\n", doc.get_error_msg().get_ptr(), doc.get_error_line());
        return false;
    }

    if (!doc.binary_serialize_to_file(output_filename.get_ptr()))
    {
        vogl_error_printf("Failed serializing to output file \"%s\"!\n", output_filename.get_ptr());
        return false;
    }

    vogl_message_printf("Wrote binary UBJ file to \"%s\"\n", output_filename.get_ptr());

    return true;
}
