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
#pragma once


#include "vogl_build_options.h"

// Int type large enough to hold a pointer
typedef intptr_t GLsizeiptr;
typedef intptr_t GLintptr;

// ptrdiff_t is only used by these API's, which should always accept positive values:
// glBufferDataARB 1 size GLsizeiptrARB
// glBufferSubDataARB 2 size GLsizeiptrARB
// glGetBufferSubDataARB 2 size GLsizeiptrARB
// glBufferSubDataARB 1 offset GLintptrARB
// glGetBufferSubDataARB 1 offset GLintptrARB
// glBindVideoCaptureStreamBufferNV 3 offset GLintptrARB
typedef ptrdiff_t GLsizeiptrARB;
typedef ptrdiff_t GLintptrARB;

typedef char GLcharARB;
typedef unsigned char GLchar;

typedef unsigned int GLhandleARB;

typedef unsigned short GLhalfARB;
typedef unsigned short GLhalfNV;
typedef unsigned short GLhalf;

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed char GLbyte;      /* 1-byte signed */
typedef short GLshort;           /* 2-byte signed */
typedef int GLint;               /* 4-byte signed */
typedef unsigned char GLubyte;   /* 1-byte unsigned */
typedef unsigned short GLushort; /* 2-byte unsigned */
typedef unsigned int GLuint;     /* 4-byte unsigned */
typedef int GLsizei;             /* 4-byte signed */
typedef float GLfloat;           /* single precision float */
typedef float GLclampf;          /* single precision float in [0,1] */
typedef double GLdouble;         /* double precision float */
typedef double GLclampd;         /* double precision float in [0,1] */

typedef int64_t GLint64;
typedef uint64_t GLuint64;
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64EXT;

typedef void *GLsync;

typedef int GLfixed;
struct _cl_context;
struct _cl_event;

typedef GLintptr GLvdpauSurfaceNV;

#define GLAPIENTRY
typedef void(GLAPIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *userParam);
typedef void(GLAPIENTRY *GLDEBUGPROCARB)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *userParam);
typedef void(GLAPIENTRY *GLDEBUGPROCAMD)(GLuint id, GLenum category, GLenum severity, GLsizei length, const GLchar *message, GLvoid *userParam);

enum PlatUndefinedType 
{ 
    PLATUNDEFINEDTYPE = 0
#if (!VOGL_PLATFORM_HAS_GLX)
  , None = 1 
#endif
};
 

#define UndefinedTypeThisPlatform(_typename) \
    struct _typename \
    { \
        PlatUndefinedType unused; \
        _typename(PlatUndefinedType _unused=PLATUNDEFINEDTYPE) : unused(_unused) { } \
        _typename(int _ptr) : unused(PLATUNDEFINEDTYPE) { } \
        bool operator==(const _typename& ) { return false; } \
    }

#define UndefinedPointerTypeThisPlatform(_typename) \
    typedef UndefinedTypeThisPlatform(_typename##__) *_typename

typedef unsigned long XID;

#if (!VOGL_PLATFORM_HAS_GLX)
    typedef int Bool;

    UndefinedTypeThisPlatform(Colormap);
    UndefinedTypeThisPlatform(Font);
    UndefinedTypeThisPlatform(Pixmap);
    UndefinedTypeThisPlatform(Status);
    UndefinedTypeThisPlatform(Window);

    UndefinedTypeThisPlatform(_XDisplay);
    UndefinedTypeThisPlatform(XFontStruct);
    UndefinedTypeThisPlatform(XVisualInfo);

    typedef _XDisplay Display;
#endif

#if (!VOGL_PLATFORM_HAS_WGL)

    typedef char CHAR;
    typedef unsigned long DWORD;
    typedef int INT;
    typedef int64_t INT64;
    typedef float FLOAT;
    typedef void* HANDLE;
    typedef char* LPCSTR;
    typedef void* LPVOID;
    typedef uint32_t UINT;
    typedef uint16_t USHORT;

    UndefinedPointerTypeThisPlatform(HDC);
    UndefinedPointerTypeThisPlatform(HGLRC);
    UndefinedPointerTypeThisPlatform(PROC);

    UndefinedTypeThisPlatform(COLORREF);
    UndefinedTypeThisPlatform(LAYERPLANEDESCRIPTOR);
    UndefinedTypeThisPlatform(PIXELFORMATDESCRIPTOR);
    UndefinedTypeThisPlatform(RECT);
#endif

typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;

typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;

typedef void (*__GLXextFuncPtr)(void);
typedef XID GLXVideoCaptureDeviceNV;
typedef unsigned int GLXVideoDeviceNV;

#define DECLARE_WGL_HANDLE(name) struct name##__{int unused;}; typedef struct name##__ *name

DECLARE_WGL_HANDLE(HPBUFFERARB);
DECLARE_WGL_HANDLE(HPBUFFEREXT);
DECLARE_WGL_HANDLE(HVIDEOINPUTDEVICENV);
DECLARE_WGL_HANDLE(HVIDEOOUTPUTDEVICENV);
DECLARE_WGL_HANDLE(HPVIDEODEV);
DECLARE_WGL_HANDLE(HGPUNV);

typedef unsigned long VOGL_WIN_DWORD;
typedef char VOGL_WIN_CHAR;
typedef long VOGL_WIN_LONG;

typedef struct VOGL_WIN_RECT
{
    VOGL_WIN_LONG    left;
    VOGL_WIN_LONG    top;
    VOGL_WIN_LONG    right;
    VOGL_WIN_LONG    bottom;
} VOGL_WIN_RECT;

typedef struct VOGL_WIN_GPU_DEVICE 
{
    VOGL_WIN_DWORD  cb;
    VOGL_WIN_CHAR   DeviceName[32];
    VOGL_WIN_CHAR   DeviceString[128];
    VOGL_WIN_DWORD  Flags;
    VOGL_WIN_RECT   rcVirtualScreen;
} GPU_DEVICE, *PGPU_DEVICE;


#include "gl_enums.inc"

#include "glx_enums.inc"
#include "glx_ext_enums.inc"
