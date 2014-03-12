These are files that are used in common between many of the sample programs in this book 

************************************
The following files are for the Win32 platform only:

glext.h		- OpenGL extensions
wglext.h	- wgl specific extensions
glut32.dll	- GLUT DLL - put in Windows\System32
glut32.lib	- GLUT import library. Put in your compilers library directory
glut.h		- GLUT header file. Put in your compilers include\gl directory

These libraries and equivalent functionality already exist on the Mac and are not needed by either Project Builder or XCode. Linux users... sorry, you're on your own.

**********************************
The following headers and source files are portable at least between Win32 and Mac OS X. Platform dependencies are accounted for when necessary.

OpenGLSB.h	 	- The OpenGL SuperBible main header. Includes necessary framework files for Win32 and OS X.
gltools.h		- Handful of useful utility functions and macros used throughout the book.
LoadTGA.c	 	- Loads vanilla 32/24/8 bit targas. No palettes or RLE compression support.
Torus.c		 	- Draws a torus. Generates normals and texture coordinates.
IsExtSupported.c 	- Tests and extension string to see if it is supported.
GetExtensionPointer.c 	- Platform independent implementation to get a pointer to an extension function.
MatrixMath.c		- Assorted matrix functions that are not in-lined in gltools.h
********************************
That's all for now.

Richard S. Wright Jr.
opengl@bellsouth.net


