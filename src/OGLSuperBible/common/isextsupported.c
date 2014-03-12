// GLTools Library
// gltIsExtSupported.c
// OpenGL SuperBible
// Richard S. Wright Jr.
// Function based on code from the ATI OpenGL SDK

#include "gltools.h"
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// This function determines if the named OpenGL Extension is supported
// Returns 1 or 0
int gltIsExtSupported(const char *extension)
	{
	GLubyte *extensions = 0;
	const GLubyte *start;
	GLubyte *where, *terminator;

	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return 0;

	extensions = (GLubyte *)glGetString(GL_EXTENSIONS);

	start = extensions;
	for (;;) 
		{
		where = (GLubyte *) strstr((const char *) start, extension);

		if (!where)
			break;
    
		terminator = where + strlen(extension);
    
		if (where == start || *(where - 1) == ' ') 
			{
			if (*terminator == ' ' || *terminator == '\0') 
				return 1;
			}
		start = terminator;
		}
	return 0;
	}

#ifdef _WIN32
///////////////////////////////////////////////////////////////////////////////
// Win32 Only, check for WGL extension
#include "wglext.h"
int gltIsWGLExtSupported(HDC hDC, const char *szExtension)
	{
	GLubyte *extensions = NULL;
	const GLubyte *start;
	GLubyte *where, *terminator;
    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;

    // Just look or the entry point
    wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
    if(wglGetExtensionsStringARB == NULL)
        return 0;

	where = (GLubyte *) strchr(szExtension, ' ');
	if (where || *szExtension == '\0')
		return 0;

	extensions = (GLubyte *)wglGetExtensionsStringARB(hDC);

	start = extensions;
	for (;;) 
		{
		where = (GLubyte *) strstr((const char *) start, szExtension);

		if (!where)
			break;
    
		terminator = where + strlen(szExtension);
    
		if (where == start || *(where - 1) == ' ') 
			{
			if (*terminator == ' ' || *terminator == '\0') 
				return 1;
			}
		start = terminator;
		}
	return 0;
	}

  #endif
