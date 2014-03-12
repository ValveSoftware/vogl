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

/* 7zFile.h -- File IO
2008-11-22 : Igor Pavlov : Public domain */

#ifndef __7Z_FILE_H
#define __7Z_FILE_H

#ifdef _WIN32
#define USE_WINDOWS_FILE
#endif

#ifdef USE_WINDOWS_FILE
#include <windows.h>
#else
#include <stdio.h>
#endif

#include "lzma_Types.h"

namespace vogl
{

    /* ---------- File ---------- */

    typedef struct
    {
#ifdef USE_WINDOWS_FILE
        HANDLE handle;
#else
        FILE *file;
#endif
    } CSzFile;

    void File_Construct(CSzFile *p);
    WRes InFile_Open(CSzFile *p, const char *name);
    WRes OutFile_Open(CSzFile *p, const char *name);
    WRes File_Close(CSzFile *p);

    /* reads max(*size, remain file's size) bytes */
    WRes File_Read(CSzFile *p, void *data, size_t *size);

    /* writes *size bytes */
    WRes File_Write(CSzFile *p, const void *data, size_t *size);

    WRes File_Seek(CSzFile *p, Int64 *pos, ESzSeek origin);
    WRes File_GetLength(CSzFile *p, UInt64 *length);

    /* ---------- FileInStream ---------- */

    typedef struct
    {
        ISeqInStream s;
        CSzFile file;
    } CFileSeqInStream;

    void FileSeqInStream_CreateVTable(CFileSeqInStream *p);

    typedef struct
    {
        ISeekInStream s;
        CSzFile file;
    } CFileInStream;

    void FileInStream_CreateVTable(CFileInStream *p);

    typedef struct
    {
        ISeqOutStream s;
        CSzFile file;
    } CFileOutStream;

    void FileOutStream_CreateVTable(CFileOutStream *p);
}

#endif
