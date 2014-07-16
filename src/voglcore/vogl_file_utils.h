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

// File: vogl_file_utils.h
#pragma once

#include "vogl_core.h"
#include "vogl_growable_array.h"

namespace vogl
{
    class dynamic_string;
    typedef vogl::vector<dynamic_string> dynamic_string_array;

    struct file_utils
    {
        // Returns true if pSrcFilename is older than pDstFilename
        static bool is_read_only(const char *pFilename);
        static bool disable_read_only(const char *pFilename);
        static bool is_older_than(const char *pSrcFilename, const char *pDstFilename);
        static bool does_file_exist(const char *pFilename);
        static bool does_dir_exist(const char *pDir);
        static bool get_file_size(const char *pFilename, uint64_t &file_size);
        static bool get_file_size(const char *pFilename, uint32_t &file_size);
        static void delete_file(const char *pFilename);

        static bool is_path_separator(char c);
        static bool is_path_or_drive_separator(char c);
        static bool is_drive_separator(char c);

        static bool split_path(const char *p, dynamic_string *pDrive, dynamic_string *pDir, dynamic_string *pFilename, dynamic_string *pExt);
        static bool split_path(const char *p, dynamic_string &path, dynamic_string &filename);

        static bool get_pathname(const char *p, dynamic_string &path);
        static bool get_filename(const char *p, dynamic_string &filename);
        static dynamic_string get_pathname(const char *p)
        {
            dynamic_string result;
            get_pathname(p, result);
            return result;
        }
        static dynamic_string get_filename(const char *p)
        {
            dynamic_string result;
            get_filename(p, result);
            return result;
        }

        static void combine_path(dynamic_string &dst, const char *pA, const char *pB);
        static void combine_path(dynamic_string &dst, const char *pA, const char *pB, const char *pC);
        static void combine_path_and_extension(dynamic_string &dst, const dynamic_string *pA, const dynamic_string *pB, const dynamic_string *pC, const dynamic_string *pExt);
        static void combine_path_and_extension(dynamic_string &dst, const char *pA, const char *pB, const char *pC, const char *pExt);

        static bool full_path(dynamic_string &path);

        // Returns extension, *without* a leading '.'
        static bool get_extension(dynamic_string &filename);
        static bool remove_extension(dynamic_string &filename);
        static bool create_directory(const char *pPath);
        static bool create_directories(const dynamic_string &path, bool remove_filename);
        static bool create_directories(const char *pPath, bool remove_filename);
        static bool create_directories_from_full_path(const dynamic_string &path);
        static void trim_trailing_seperator(dynamic_string &path);
        static bool add_default_extension(dynamic_string &path, const char *pExt); // pExt should begin with a '.'

        static int wildcmp(const char *pWild, const char *pString);

        // use vogl_free() on the returned buffer
        static void *read_file_to_heap(const char *pPath, size_t &data_size);
        static bool read_file_to_vec(const char *pPath, uint8_vec &data);

        // Does not use ftell when reading file (ftell doesn't work on virtual /proc files)
        #if VOGL_HAS_PROC_FILESYSTEM
            static bool read_proc_file(const char *pPath, vogl::growable_array<char, 2048> &data);
        #endif

        static bool write_buf_to_file(const char *pPath, const void *pData, size_t data_size);
        static bool write_vec_to_file(const char *pPath, const uint8_vec &data);

        enum read_text_file_flags_t
        {
            cRTFTrim = 1,                 // trims whitespace at beginning and end of each line
            cRTFTrimEnd = 2,              // trims whitepsace at end of each line
            cRTFIgnoreEmptyLines = 4,     // filters out empty lines (after optional trimming)
            cRTFIgnoreCommentedLines = 8, // filters out "//" commented lines
            cRTFPrintErrorMessages = 16,  // print error messages to console
            cRTFPrintWarningMessages = 32 // print error messages to console
        };

        // TODO: This only works on files with valid file sizes (i.e. it won't work on /proc/self/status).
        static bool read_text_file(const char *pPath, dynamic_string_array &lines, uint32_t flags = 0);

        static bool read_text_file_crt(const char *pPath, dynamic_string_array &lines);

        static bool write_text_file(const char *pPath, const dynamic_string_array &lines, bool unix_line_endings = false);

        // write_string_to_file() doesn't write any line ending characters - you're on your own.
        static bool write_string_to_file(const char *pPath, const dynamic_string &str);

        // Return temporary filename prefixed with prefix string.
        static dynamic_string generate_temp_filename(const char *prefix);

        static bool change_directory(const char *pPath);

        static char *get_exec_filename(char *pPath, size_t dest_len);
    }; // struct file_utils

} // namespace vogl
