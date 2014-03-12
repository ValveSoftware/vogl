////////////////////////////////////////////////
// Here, we will try to isolate as many platform
// dependencies here as possible so that all sample
// programs contain as few headers and are as self
// contained as possible.

// Windows... Visual C++
#ifdef _WIN32
// Standard Windows includes
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include "glut.h"
#include "glext.h"
#include "wglext.h"
// Mac OS X
#elif defined __APPLE__

#include <Carbon/Carbon.h>
#include <GLUT/glut.h>
#include <OpenGL/glext.h>

#define APIENTRY
#define INT_PTR int*
#define Sleep(x)

// Assume Linux
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include <sys/time.h>
#endif
