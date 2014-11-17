/**************************************************************************
 *
 * Copyright 2014 LunarG, Inc.  All Rights Reserved.
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
 * Copyright (C) 2008  VMware, Inc.  All Rights Reserved.
 * Copyright (c) 2011 VMware, Inc.
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
 * This file was originally authored by Ian Elliott (ian@lunarg.com).
 *
 **************************************************************************/

#ifndef PXFMT_GL_H
#define PXFMT_GL_H

#if defined(__APPLE__)
	#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED

	#include <OpenGL/gl.h>
	#include <OpenGL/gl3.h>
	#include <OpenGL/glext.h>

	#define GL_ALPHA_INTEGER GL_ALPHA_INTEGER_EXT
#else
	#include <GL/gl.h>
	#include <GL/glext.h>
#endif

#endif // PXFMT_GL_H

