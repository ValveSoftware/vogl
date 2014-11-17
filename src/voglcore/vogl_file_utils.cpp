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

// File: vogl_file_utils.cpp
#include <fcntl.h>
#include "vogl_core.h"
#include "vogl_file_utils.h"
#include "vogl_strutils.h"
#include "vogl_cfile_stream.h"
#include "vogl_console.h"
#include "vogl_port.h"

#ifdef VOGL_USE_WIN32_API
    #include "vogl_winhdr.h"
#endif

#if defined(PLATFORM_WINDOWS)
    #include <direct.h>
    #include <io.h>
#endif

#if defined(COMPILER_GCCLIKE)
    #include <sys/stat.h>
    #include <libgen.h>
#endif

#if defined(VOGL_USE_OSX_API)
	#include <crt_externs.h>
#endif

namespace vogl
{
#ifdef VOGL_USE_WIN32_API
    bool file_utils::is_read_only(const char *pFilename)
    {
        uint32_t dst_file_attribs = GetFileAttributesA(pFilename);
        if (dst_file_attribs == INVALID_FILE_ATTRIBUTES)
            return false;
        if (dst_file_attribs & FILE_ATTRIBUTE_READONLY)
            return true;
        return false;
    }

    bool file_utils::disable_read_only(const char *pFilename)
    {
        uint32_t dst_file_attribs = GetFileAttributesA(pFilename);
        if (dst_file_attribs == INVALID_FILE_ATTRIBUTES)
            return false;
        if (dst_file_attribs & FILE_ATTRIBUTE_READONLY)
        {
            dst_file_attribs &= ~FILE_ATTRIBUTE_READONLY;
            if (SetFileAttributesA(pFilename, dst_file_attribs))
                return true;
        }
        return false;
    }

    bool file_utils::is_older_than(const char *pSrcFilename, const char *pDstFilename)
    {
        WIN32_FILE_ATTRIBUTE_DATA src_file_attribs;
        const BOOL src_file_exists = GetFileAttributesExA(pSrcFilename, GetFileExInfoStandard, &src_file_attribs);

        WIN32_FILE_ATTRIBUTE_DATA dst_file_attribs;
        const BOOL dest_file_exists = GetFileAttributesExA(pDstFilename, GetFileExInfoStandard, &dst_file_attribs);

        if ((dest_file_exists) && (src_file_exists))
        {
            LONG timeComp = CompareFileTime(&src_file_attribs.ftLastWriteTime, &dst_file_attribs.ftLastWriteTime);
            if (timeComp < 0)
                return true;
        }
        return false;
    }

    bool file_utils::does_file_exist(const char *pFilename)
    {
        const DWORD fullAttributes = GetFileAttributesA(pFilename);

        if (fullAttributes == INVALID_FILE_ATTRIBUTES)
            return false;

        if (fullAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return false;

        return true;
    }

    bool file_utils::does_dir_exist(const char *pDir)
    {
        //-- Get the file attributes.
        DWORD fullAttributes = GetFileAttributesA(pDir);

        if (fullAttributes == INVALID_FILE_ATTRIBUTES)
            return false;

        if (fullAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return true;

        return false;
    }

    bool file_utils::get_file_size(const char *pFilename, uint64_t &file_size)
    {
        file_size = 0;

        WIN32_FILE_ATTRIBUTE_DATA attr;

        if (0 == GetFileAttributesExA(pFilename, GetFileExInfoStandard, &attr))
            return false;

        if (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return false;

        file_size = static_cast<uint64_t>(attr.nFileSizeLow) | (static_cast<uint64_t>(attr.nFileSizeHigh) << 32U);

        return true;
    }
#elif defined(COMPILER_GCCLIKE)
    bool file_utils::is_read_only(const char *pFilename)
    {
        VOGL_NOTE_UNUSED(pFilename);
        vogl_error_printf("Unimplemented\n");
        return false;
    }

    bool file_utils::disable_read_only(const char *pFilename)
    {
        VOGL_NOTE_UNUSED(pFilename);
        vogl_error_printf("Unimplemented\n");
        return false;
    }

    bool file_utils::is_older_than(const char *pSrcFilename, const char *pDstFilename)
    {
        VOGL_NOTE_UNUSED(pSrcFilename);
        VOGL_NOTE_UNUSED(pDstFilename);
        vogl_error_printf("Unimplemented\n");
        return false;
    }

    bool file_utils::does_file_exist(const char *pFilename)
    {
        struct stat64 stat_buf;
        int result = stat64(pFilename, &stat_buf);
        if (result)
        {
            return false;
        }
        if (S_ISREG(stat_buf.st_mode))
            return true;
        return false;
    }

    bool file_utils::does_dir_exist(const char *pDir)
    {
        struct stat64 stat_buf;
        int result = stat64(pDir, &stat_buf);
        if (result)
            return false;
        if (S_ISDIR(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode))
            return true;
        return false;
    }

    bool file_utils::get_file_size(const char *pFilename, uint64_t &file_size)
    {
        file_size = 0;
        struct stat64 stat_buf;
        int result = stat64(pFilename, &stat_buf);
        if (result)
            return false;
        if (!S_ISREG(stat_buf.st_mode))
            return false;
        file_size = stat_buf.st_size;
        return true;
    }
#else
    bool file_utils::is_read_only(const char *pFilename)
    {
        return false;
    }

    bool file_utils::disable_read_only(const char *pFilename)
    {
        pFilename;
        vogl_error_printf("Unimplemented\n");
        return false;
    }

    bool file_utils::is_older_than(const char *pSrcFilename, const char *pDstFilename)
    {
        vogl_error_printf("Unimplemented\n");
        return false;
    }

    bool file_utils::does_file_exist(const char *pFilename)
    {
        FILE *pFile = vogl_fopen(pFilename, "rb");
        if (!pFile)
            return false;
        vogl_fclose(pFile);
        return true;
    }

    bool file_utils::does_dir_exist(const char *pDir)
    {
        vogl_error_printf("Unimplemented\n");
        return false;
    }

    bool file_utils::get_file_size(const char *pFilename, uint64_t &file_size)
    {
        FILE *pFile = vogl_fopen(pFilename, "rb");
        if (!pFile)
            return false;
        vogl_fseek(pFile, 0, SEEK_END);
        file_size = vogl_ftell(pFile);
        vogl_fclose(pFile);
        return true;
    }
#endif

    bool file_utils::get_file_size(const char *pFilename, uint32_t &file_size)
    {
        uint64_t file_size64;
        if (!get_file_size(pFilename, file_size64))
        {
            file_size = 0;
            return false;
        }

        if (file_size64 > cUINT32_MAX)
            file_size64 = cUINT32_MAX;

        file_size = static_cast<uint32_t>(file_size64);
        return true;
    }

    void file_utils::delete_file(const char *pFilename)
    {
        remove(pFilename);
    }

    bool file_utils::is_path_separator(char c)
    {
        #if defined(PLATFORM_WINDOWS)
            return (c == '/') || (c == '\\');
        #else
            return (c == '/');
        #endif
    }

    bool file_utils::is_path_or_drive_separator(char c)
    {
        #if defined(PLATFORM_WINDOWS)
            return (c == '/') || (c == '\\') || (c == ':');
        #else
            return (c == '/');
        #endif
    }

    bool file_utils::is_drive_separator(char c)
    {
        #if defined(PLATFORM_WINDOWS)
            return (c == ':');
        #else
            VOGL_NOTE_UNUSED(c);
            return false;
        #endif
    }

    bool file_utils::split_path(const char *p, dynamic_string *pDrive, dynamic_string *pDir, dynamic_string *pFilename, dynamic_string *pExt)
    {
        VOGL_ASSERT(p);

        #if defined(PLATFORM_WINDOWS)
            char drive_buf[_MAX_DRIVE];
            char dir_buf[_MAX_DIR];
            char fname_buf[_MAX_FNAME];
            char ext_buf[_MAX_EXT];

            #if defined(COMPILER_MSVC)
                // Compiling with MSVC
                errno_t error = _splitpath_s(p,
                                             pDrive ? drive_buf : NULL, pDrive ? _MAX_DRIVE : 0,
                                             pDir ? dir_buf : NULL, pDir ? _MAX_DIR : 0,
                                             pFilename ? fname_buf : NULL, pFilename ? _MAX_FNAME : 0,
                                             pExt ? ext_buf : NULL, pExt ? _MAX_EXT : 0);
                if (error != 0)
                    return false;
            #elif defined(COMPILER_MINGW)
                // Compiling with MinGW
                _splitpath(p,
                           pDrive ? drive_buf : NULL,
                           pDir ? dir_buf : NULL,
                           pFilename ? fname_buf : NULL,
                           pExt ? ext_buf : NULL);
            #else
                #error "Need to provide splitpath functionality for this compiler / platform combo."
            #endif

            if (pDrive)
                *pDrive = drive_buf;
            if (pDir)
                *pDir = dir_buf;
            if (pFilename)
                *pFilename = fname_buf;
            if (pExt)
                *pExt = ext_buf;
        #else // !PLATFORM_WINDOWS
            char dirtmp[1024];
            char nametmp[1024];
            strcpy_safe(dirtmp, sizeof(dirtmp), p);
            strcpy_safe(nametmp, sizeof(nametmp), p);

            if (pDrive)
                pDrive->clear();

            const char* pDirName = dirtmp;

            int len = strlen(p);
            bool endsWithSlash = (strncmp(&p[len-1], "/", 1) == 0);
            if (!endsWithSlash)
            {
                // if path does not end in a '/', then the directory needs to be split from the file
                pDirName = dirname(dirtmp);
                if (!pDirName)
                    return false;
            }

            if (pDir)
            {
                pDir->set(pDirName);
                if ((!pDir->is_empty()) && (pDir->back() != '/'))
                    pDir->append_char('/');
            }

            const char *pBaseName = "";
            if (!endsWithSlash)
            {
                pBaseName = basename(nametmp);
                if (!pBaseName)
                    return false;
            }

            if (pFilename)
            {
                pFilename->set(pBaseName);
                remove_extension(*pFilename);
            }

            if (pExt)
            {
                pExt->set(pBaseName);
                get_extension(*pExt);
                if (pExt->get_len())
                    *pExt = "." + *pExt;
            }
        #endif // #if defined(PLATFORM_WINDOWS)

        return true;
    }

    bool file_utils::split_path(const char *p, dynamic_string &path, dynamic_string &filename)
    {
        dynamic_string temp_drive, temp_path, temp_ext;
        if (!split_path(p, &temp_drive, &temp_path, &filename, &temp_ext))
            return false;

        filename += temp_ext;

        combine_path(path, temp_drive.get_ptr(), temp_path.get_ptr());
        return true;
    }

    bool file_utils::get_pathname(const char *p, dynamic_string &path)
    {
        dynamic_string temp_drive, temp_path;
        if (!split_path(p, &temp_drive, &temp_path, NULL, NULL))
            return false;

        combine_path(path, temp_drive.get_ptr(), temp_path.get_ptr());
        return true;
    }

    bool file_utils::get_filename(const char *p, dynamic_string &filename)
    {
        dynamic_string temp_ext;
        if (!split_path(p, NULL, NULL, &filename, &temp_ext))
            return false;

        filename += temp_ext;
        return true;
    }

    void file_utils::combine_path(dynamic_string &dst, const char *pA, const char *pB)
    {
        dynamic_string temp(pA);
        if ((!temp.is_empty()) && (!is_path_separator(pB[0])))
        {
            char c = temp[temp.get_len() - 1];
            if (!is_path_separator(c))
                temp.append_char(VOGL_PATH_SEPERATOR_CHAR);
        }
        temp += pB;
        dst.swap(temp);
    }

    void file_utils::combine_path(dynamic_string &dst, const char *pA, const char *pB, const char *pC)
    {
        combine_path(dst, pA, pB);
        combine_path(dst, dst.get_ptr(), pC);
    }

    void file_utils::combine_path_and_extension(dynamic_string &dst, const char *pA, const char *pB, const char *pC, const char *pExt)
    {
        combine_path(dst, pA, pB, pC);

        if ((!dst.ends_with(".")) && (pExt[0]) && (pExt[0] != '.'))
            dst.append_char('.');

        dst.append(pExt);
    }

    void file_utils::combine_path_and_extension(dynamic_string &dst, const dynamic_string *pA, const dynamic_string *pB, const dynamic_string *pC, const dynamic_string *pExt)
    {
        combine_path_and_extension(dst, pA ? pA->get_ptr() : "", pB ? pB->get_ptr() : "", pC ? pC->get_ptr() : "", pExt ? pExt->get_ptr() : "");
    }

    bool file_utils::full_path(dynamic_string &path)
    {
        #if defined(PLATFORM_WINDOWS)
            char buf[1024];
            char *p = _fullpath(buf, path.get_ptr(), sizeof(buf));
            if (!p)
                return false;
        #else
            char buf[PATH_MAX];
            char *p;
            dynamic_string pn, fn;
            split_path(path.get_ptr(), pn, fn);
            if ((fn == ".") || (fn == ".."))
            {
                p = realpath(path.get_ptr(), buf);
                if (!p)
                    return false;
                path.set(buf);
            }
            else
            {
                if (pn.is_empty())
                    pn = "./";
                p = realpath(pn.get_ptr(), buf);
                if (!p)
                    return false;
                combine_path(path, buf, fn.get_ptr());
            }
        #endif

        return true;
    }

    bool file_utils::get_extension(dynamic_string &filename)
    {
        int sep = -1;
        #if defined(PLATFORM_WINDOWS)
            int rightmost_backslash = filename.find_right('\\');
            int rightmost_forwardslash = filename.find_right('/');
            sep = VOGL_MAX(rightmost_backslash, 
                           rightmost_forwardslash);
        #endif
        if (sep < 0)
            sep = filename.find_right('/');

        int dot = filename.find_right('.');
        if (dot <= sep)
        {
            filename.set_len(0);
            return false;
        }

        filename.right(dot + 1);

        return true;
    }

    bool file_utils::remove_extension(dynamic_string &filename)
    {
        int sep = -1;
        #if defined(PLATFORM_WINDOWS)
            int rightmost_backslash = filename.find_right('\\');
            int rightmost_forwardslash = filename.find_right('/');
            sep = VOGL_MAX(rightmost_backslash,
                           rightmost_forwardslash);
        #endif
        if (sep < 0)
            sep = filename.find_right('/');

        int dot = filename.find_right('.');
        if (dot < sep)
            return false;

        filename.left(dot);

        return true;
    }

    bool file_utils::create_directory(const char *pPath)
    {
        #if defined(PLATFORM_WINDOWS)
            int status = _mkdir(pPath);
        #else
            int status = mkdir(pPath, S_IRWXU | S_IRWXG | S_IRWXO);
        #endif
        if (status == 0)
            vogl_debug_printf("Created directory %s\n", pPath);
        return !status;
    }

    bool file_utils::create_directories(const dynamic_string &path, bool remove_filename)
    {
        dynamic_string path_to_create(path);

        full_path(path_to_create);

        if (remove_filename)
        {
            dynamic_string pn, fn;
            split_path(path_to_create.get_ptr(), pn, fn);
            path_to_create = pn;
        }

        return create_directories_from_full_path(path_to_create);
    }

    bool file_utils::create_directories(const char* pPath, bool remove_filename)
    {
        dynamic_string path_to_create(pPath);

        return create_directories(path_to_create, remove_filename);
    }

    bool file_utils::create_directories_from_full_path(const dynamic_string &fullpath)
    {
        bool got_unc = false;
        VOGL_NOTE_UNUSED(got_unc);
        dynamic_string cur_path;

        const int l = fullpath.get_len();

        int n = 0;
        while (n < l)
        {
            const char c = fullpath.get_ptr()[n];

            const bool sep = is_path_separator(c);
            const bool back_sep = is_path_separator(cur_path.back());
            const bool is_last_char = (n == (l - 1));

            if (((sep) && (!back_sep)) || (is_last_char))
            {
                if ((is_last_char) && (!sep))
                    cur_path.append_char(c);

                bool valid = !cur_path.is_empty();

                #if defined(PLATFORM_WINDOWS)
                    // reject obvious stuff (drives, beginning of UNC paths):
                    // c:\b\cool
                    // \\machine\blah
                    // \cool\blah
                    if ((cur_path.get_len() == 2) && (cur_path[1] == ':'))
                        valid = false;
                    else if ((cur_path.get_len() >= 2) && (cur_path[0] == '\\') && (cur_path[1] == '\\'))
                    {
                        if (!got_unc)
                            valid = false;
                        got_unc = true;
                    }
                    else if (cur_path == "\\")
                        valid = false;
                #endif
                if (cur_path == "/")
                    valid = false;

                if (valid)
                {
                    create_directory(cur_path.get_ptr());
                }
            }

            cur_path.append_char(c);

            n++;
        }

        return true;
    }

    void file_utils::trim_trailing_seperator(dynamic_string &path)
    {
        if ((path.get_len()) && (is_path_separator(path.back())))
            path.truncate(path.get_len() - 1);
    }

    bool file_utils::add_default_extension(dynamic_string &path, const char *pExt)
    {
        dynamic_string ext(path);
        get_extension(ext);
        if (ext.is_empty())
        {
            path.append(pExt);
            return true;
        }
        return false;
    }

    // See http://www.codeproject.com/KB/string/wildcmp.aspx
    int file_utils::wildcmp(const char *pWild, const char *pString)
    {
        const char *cp = NULL, *mp = NULL;

        while ((*pString) && (*pWild != '*'))
        {
            if ((*pWild != *pString) && (*pWild != '?'))
                return 0;
            pWild++;
            pString++;
        }

        // Either *pString=='\0' or *pWild='*' here.

        while (*pString)
        {
            if (*pWild == '*')
            {
                if (!*++pWild)
                    return 1;
                mp = pWild;
                cp = pString + 1;
            }
            else if ((*pWild == *pString) || (*pWild == '?'))
            {
                pWild++;
                pString++;
            }
            else
            {
                pWild = mp;
                pString = cp++;
            }
        }

        while (*pWild == '*')
            pWild++;

        return !*pWild;
    }

    bool file_utils::read_file_to_vec(const char *pPath, uint8_vec &data)
    {
        size_t data_size;
        void *p = read_file_to_heap(pPath, data_size);
        if (!p)
            return false;

        if (static_cast<uint32_t>(data_size) != data_size)
            return false;

        if (!data.try_resize(static_cast<uint32_t>(data_size)))
            return false;

        if (!data.grant_ownership(static_cast<uint8_t *>(p), static_cast<uint32_t>(data_size), static_cast<uint32_t>(data_size)))
        {
            vogl_free(p);
            return false;
        }

        return true;
    }

    void *file_utils::read_file_to_heap(const char *pPath, size_t &data_size)
    {
        data_size = 0;

        FILE *pFile = vogl_fopen(pPath, "rb");
        if (!pFile)
            return NULL;

        vogl_fseek(pFile, 0, SEEK_END);
        uint64_t file_size = vogl_ftell(pFile);
        vogl_fseek(pFile, 0, SEEK_SET);

        if (file_size > VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE)
        {
            vogl_fclose(pFile);
            return NULL;
        }

        data_size = static_cast<size_t>(file_size);
        VOGL_ASSERT(data_size == file_size);

        void *p = vogl_malloc(data_size);
        if (!p)
        {
            vogl_fclose(pFile);
            data_size = 0;
            return NULL;
        }

        bool success = (vogl_fread(p, 1, data_size, pFile) == data_size);

        vogl_fclose(pFile);

        if (!success)
        {
            vogl_free(p);
            data_size = 0;
            return NULL;
        }

        return p;
    }

	#if VOGL_HAS_PROC_FILESYSTEM
		bool file_utils::read_proc_file(const char *filename, vogl::growable_array<char, 2048> &data)
		{
			bool ret = false;
			uint32_t file_length = 0;
			int nbytes = data.cMaxFixedElements;
    
			int fd = open(filename, O_RDONLY);
			if (fd != -1)
			{
				for (;;)
				{
					// Make sure we've got enough room to read in another nbytes.
					data.resize(file_length + nbytes);
    
					// Try to read in nbytes. read returns 0:end of file, -1:error.
					ssize_t length = read(fd, data.get_ptr() + file_length, nbytes);
					if (length < 0)
						break;
    
					file_length += length;
					if (length != nbytes)
					{
						ret = true;
						break;
					}
				}
    
				close(fd);
			}
    
			// Trim trailing whitespace.
			while ((file_length > 0) && vogl::vogl_isspace(data[file_length - 1]))
				file_length--;
    
			data.resize(file_length + 1);
			data[file_length] = 0;
			return ret;
		}
	#endif

    bool file_utils::write_buf_to_file(const char *pPath, const void *pData, size_t data_size)
    {
        FILE *pFile = vogl_fopen(pPath, "wb");
        if (!pFile)
            return false;

        bool success = vogl_fwrite(pData, 1, data_size, pFile) == data_size;

        if (vogl_fclose(pFile) == EOF)
            return false;

        return success;
    }

    bool file_utils::write_vec_to_file(const char *pPath, const uint8_vec &data)
    {
        return write_buf_to_file(pPath, data.get_ptr(), data.size());
    }

    // This helper only works on files with valid file sizes (i.e. it won't work on some files under proc such as /proc/self/status).
    bool file_utils::read_text_file(const char *pPath, dynamic_string_array &lines, uint32_t flags)
    {
        cfile_stream stream;
        if (!stream.open(pPath, cDataStreamReadable))
        {
            if (flags & cRTFPrintErrorMessages)
                vogl_error_printf("Failed opening text file \"%s\" for reading\n", pPath);
            else if (flags & cRTFPrintWarningMessages)
                vogl_warning_printf("Failed opening text file \"%s\" for reading\n", pPath);

            return false;
        }

        int line_count = 0;
        dynamic_string line_str;
        while (stream.get_remaining())
        {
            ++line_count;
            if (!stream.read_line(line_str))
            {
                if (flags & cRTFPrintErrorMessages)
                    vogl_error_printf("Failed reading from text file \"%s\"\n", pPath);
                else if (flags & cRTFPrintWarningMessages)
                    vogl_warning_printf("Failed reading from text file \"%s\"\n", pPath);

                break;
            }

            if (flags & cRTFTrim)
                line_str.trim();

            if (flags & cRTFTrimEnd)
                line_str.trim_end();

            if (flags & cRTFIgnoreEmptyLines)
            {
                if (line_str.is_empty())
                    continue;
            }

            if ((flags & cRTFIgnoreCommentedLines) && (line_str.get_len() >= 2))
            {
                bool found_comment = false;

                for (int i = 0; i < static_cast<int>(line_str.get_len()); ++i)
                {
                    char c = line_str[i];

                    if ((c == ' ') || (c == '\t'))
                        continue;
                    else if ((c == '/') && (line_str[i + 1] == '/'))
                        found_comment = true;

                    break;
                }

                if (found_comment)
                    continue;
            }

            lines.push_back(line_str);
        }

        return true;
    }

    bool file_utils::read_text_file_crt(const char *pPath, dynamic_string_array &lines)
    {
        FILE *pFile = fopen(pPath, "r");
        if (!pFile)
            return false;

        while (!feof(pFile))
        {
            char buf[4096];
            buf[0] = '\0';
            fgets(buf, sizeof(buf), pFile);
            lines.push_back(buf);
        }

        fclose(pFile);

        return true;
    }

    bool file_utils::write_text_file(const char *pPath, const dynamic_string_array &lines, bool unix_line_endings)
    {
        cfile_stream stream;
        if (!stream.open(pPath, cDataStreamWritable))
            return false;

        for (uint32_t i = 0; i < lines.size(); i++)
        {
            const dynamic_string &str = lines[i];
            if (str.get_len())
                stream.write(str.get_ptr(), str.get_len());

            if (unix_line_endings)
                stream.write("\n", 1);
            else
                stream.write("\r\n", 2);
        }

        return !stream.get_error();
    }

    bool file_utils::write_string_to_file(const char *pPath, const dynamic_string &str)
    {
        cfile_stream stream;
        if (!stream.open(pPath, cDataStreamWritable))
            return false;

        if (str.get_len())
            stream.write(str.get_ptr(), str.get_len());

        return !stream.get_error();
    }

    dynamic_string file_utils::generate_temp_filename(const char *prefix)
    {
        char buffer[FILENAME_MAX];

        plat_gettmpfname(buffer, sizeof(buffer), prefix);
        return dynamic_string(buffer);
    }

    bool file_utils::change_directory(const char *pPath)
    {
        return plat_chdir(pPath) == 0;
    }

	char *file_utils::get_exec_filename(char *pPath, size_t dest_len)
	{
	#if VOGL_HAS_PROC_FILESYSTEM
		ssize_t s = readlink("/proc/self/exe", pPath, dest_len);
		if (s >= 0)
		{
			pPath[s] = '\0';
			return pPath;
		}

		VOGL_VERIFY(0);
		pPath[0] = '\0';

		return pPath;

	#elif defined(VOGL_USE_OSX_API)
		char **argv = *_NSGetArgv();
		
		strncpy(pPath, argv[0], dest_len);
	
		vogl_warning_printf("UNTESTED: file_utils::get_exec_filename returning [%s]\n", pPath);
		return pPath;
	
	#elif defined(VOGL_USE_WIN32_API)
		DWORD result = GetModuleFileNameA(0, pPath, safe_int_cast<DWORD>(dest_len));
		VOGL_VERIFY(result != 0);
		return pPath;
	#else
		VOGL_ASSUME(!"Implement get_exec_filename for this platform.");
	#endif
	}

} // namespace vogl
