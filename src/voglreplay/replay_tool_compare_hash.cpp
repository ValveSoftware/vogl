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

// File: replay_tool_compare_hash.cpp
#include "vogl_common.h"
#include "vogl_gl_replayer.h"
#include "vogl_file_utils.h"

//----------------------------------------------------------------------------------------------------------------------
// tool_compare_hash_files
//----------------------------------------------------------------------------------------------------------------------
bool tool_compare_hash_files_mode()
{
    dynamic_string src1_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    dynamic_string src2_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if ((src1_filename.is_empty()) || (src2_filename.is_empty()))
    {
        vogl_error_printf("Must specify two source filenames!\n");
        return false;
    }

    dynamic_string_array src1_lines;
    if (!file_utils::read_text_file(src1_filename.get_ptr(), src1_lines, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFIgnoreCommentedLines | file_utils::cRTFPrintErrorMessages))
    {
        console::error("%s: Failed reading source file %s!\n", VOGL_FUNCTION_INFO_CSTR, src1_filename.get_ptr());
        return false;
    }

    vogl_printf("Read 1st source file \"%s\", %u lines\n", src1_filename.get_ptr(), src1_lines.size());

    dynamic_string_array src2_lines;
    if (!file_utils::read_text_file(src2_filename.get_ptr(), src2_lines, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFIgnoreCommentedLines | file_utils::cRTFPrintErrorMessages))
    {
        console::error("%s: Failed reading source file %s!\n", VOGL_FUNCTION_INFO_CSTR, src2_filename.get_ptr());
        return false;
    }

    vogl_printf("Read 2nd source file \"%s\", %u lines\n", src2_filename.get_ptr(), src2_lines.size());

    const uint64_t sum_comp_thresh = g_command_line_params().get_value_as_uint64("sum_compare_threshold");

    const uint32_t compare_first_frame = g_command_line_params().get_value_as_uint("compare_first_frame");
    if (compare_first_frame > src2_lines.size())
    {
        vogl_error_printf("%s: -compare_first_frame is %u, but the second file only has %u frames!\n", VOGL_FUNCTION_INFO_CSTR, compare_first_frame, src2_lines.size());
        return false;
    }

    const uint32_t lines_to_comp = math::minimum(src1_lines.size(), src2_lines.size() - compare_first_frame);

    if (src1_lines.size() != src2_lines.size())
    {
        // FIXME: When we replay q2, we get 2 more frames vs. tracing. Not sure why, this needs to be investigated.
        if ( (!g_command_line_params().get_value_as_bool("ignore_line_count_differences")) && (labs(src1_lines.size() - src2_lines.size()) > 3) )
        {
            vogl_error_printf("%s: Input files have a different number of lines! (%u vs %u)\n", VOGL_FUNCTION_INFO_CSTR, src1_lines.size(), src2_lines.size());
            return false;
        }
        else
        {
            vogl_warning_printf("%s: Input files have a different number of lines! (%u vs %u)\n", VOGL_FUNCTION_INFO_CSTR, src1_lines.size(), src2_lines.size());
        }
    }

    const uint32_t compare_ignore_frames = g_command_line_params().get_value_as_uint("compare_ignore_frames");
    if (compare_ignore_frames > lines_to_comp)
    {
        vogl_error_printf("%s: -compare_ignore_frames is too large!\n", VOGL_FUNCTION_INFO_CSTR);
        return false;
    }

    const bool sum_hashing = g_command_line_params().get_value_as_bool("sum_hashing");

    if (g_command_line_params().has_key("compare_expected_frames"))
    {
        const uint32_t compare_expected_frames = g_command_line_params().get_value_as_uint("compare_expected_frames");
        if ((src1_lines.size() != compare_expected_frames) || (src2_lines.size() != compare_expected_frames))
        {
            vogl_warning_printf("%s: Expected %u frames! First file has %u frames, second file has %u frames.\n", VOGL_FUNCTION_INFO_CSTR, compare_expected_frames, src1_lines.size(), src2_lines.size());
            return false;
        }
    }

    uint32_t total_mismatches = 0;
    uint64_t max_sum_delta = 0;

    for (uint32_t i = compare_ignore_frames; i < lines_to_comp; i++)
    {
        const char *pStr1 = src1_lines[i].get_ptr();
        const char *pStr2 = src2_lines[i + compare_first_frame].get_ptr();

        uint64_t val1 = 0, val2 = 0;
        if (!string_ptr_to_uint64(pStr1, val1))
        {
            vogl_error_printf("%s: Failed parsing line at index %u of first source file!\n", VOGL_FUNCTION_INFO_CSTR, i);
            return false;
        }

        if (!string_ptr_to_uint64(pStr2, val2))
        {
            vogl_error_printf("%s: Failed parsing line at index %u of second source file!\n", VOGL_FUNCTION_INFO_CSTR, i);
            return false;
        }

        bool mismatch = false;

        if (sum_hashing)
        {
            uint64_t delta;
            if (val1 > val2)
                delta = val1 - val2;
            else
                delta = val2 - val1;
            max_sum_delta = math::maximum(max_sum_delta, delta);

            if (delta > sum_comp_thresh)
                mismatch = true;
        }
        else
        {
            mismatch = val1 != val2;
        }

        if (mismatch)
        {
            if (sum_hashing)
                vogl_error_printf("Mismatch at frame %u: %" PRIu64 ", %" PRIu64 "\n", i, val1, val2);
            else
                vogl_error_printf("Mismatch at frame %u: 0x%" PRIX64 ", 0x%" PRIX64 "\n", i, val1, val2);
            total_mismatches++;
        }
    }

    if (sum_hashing)
        vogl_printf("Max sum delta: %" PRIu64 "\n", max_sum_delta);

    if (!total_mismatches)
        vogl_printf("No mismatches\n");
    else
    {
        vogl_error_printf("%u total mismatches!\n", total_mismatches);
        return false;
    }

    return true;
}
