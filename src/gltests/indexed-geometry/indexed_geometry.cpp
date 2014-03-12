//-----------------------------------------------------------------------------
//           Name: ogl_indexed_geometry.cpp
//         Author: Kevin Harris
//  Last Modified: 02/01/05
//    Description: This sample demonstrates how to optimize performance by
//                 using indexed geometry. As a demonstration, the sample
//                 reduces the vertex count of a simple cube from 24 to 8 by
//                 redefining the cube?s geometry using an indices array.
//
//   Control Keys: F1 - Toggle between indexed and non-indexed geoemtry.
//                      Shouldn't produce any noticeable change since they
//                      render the same cube.
//
// Ported to Linux (quickly): Rich Geldreich, 11/24/2013
//-----------------------------------------------------------------------------

#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <math.h>
#include <sys/timeb.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glu.h>

//-----------------------------------------------------------------------------
// GLOBALS
//-----------------------------------------------------------------------------
Display   *g_pDisplay = NULL;
Window     g_window;
GLXContext g_glxContext;
bool       g_bDoubleBuffered = GL_TRUE;

float g_fSpinZ = 0.0f;
float g_fSpinY = 0.0f;

bool g_bUseIndexedGeometry = true;

struct Vertex
{
   // GL_C3F_V3F
   float r, g, b;
   float x, y, z;
};

//
// To understand how indexed geometry works, we must first build something
// which can be optimized through the use of indices.
//
// Below, is the vertex data for a simple multi-colored cube, which is defined
// as 6 individual quads, one quad for each of the cube's six sides. At first,
// this doesn?t seem too wasteful, but trust me it is.
//
// You see, we really only need 8 vertices to define a simple cube, but since
// we're using a quad list, we actually have to repeat the usage of our 8
// vertices 3 times each. To make this more understandable, I've actually
// numbered the vertices below so you can see how the vertices get repeated
// during the cube's definition.
//
// Note how the first 8 vertices are unique. Everting else after that is just
// a repeat of the first 8.
//

Vertex g_cubeVertices[] =
{
   // Quad 0
   { 1.0f,0.0f,0.0f, -1.0f,-1.0f, 1.0f }, // 0 (unique)
   { 0.0f,1.0f,0.0f,  1.0f,-1.0f, 1.0f }, // 1 (unique)
   { 0.0f,0.0f,1.0f,  1.0f, 1.0f, 1.0f }, // 2 (unique)
   { 1.0f,1.0f,0.0f, -1.0f, 1.0f, 1.0f }, // 3 (unique)

   // Quad 1
   { 1.0f,0.0f,1.0f, -1.0f,-1.0f,-1.0f }, // 4 (unique)
   { 0.0f,1.0f,1.0f, -1.0f, 1.0f,-1.0f }, // 5 (unique)
   { 1.0f,1.0f,1.0f,  1.0f, 1.0f,-1.0f }, // 6 (unique)
   { 1.0f,0.0f,0.0f,  1.0f,-1.0f,-1.0f }, // 7 (unique)

   // Quad 2
   { 0.0f,1.0f,1.0f, -1.0f, 1.0f,-1.0f }, // 5 (start repeating here)
   { 1.0f,1.0f,0.0f, -1.0f, 1.0f, 1.0f }, // 3 (repeat of vertex 3)
   { 0.0f,0.0f,1.0f,  1.0f, 1.0f, 1.0f }, // 2 (repeat of vertex 2... etc.)
   { 1.0f,1.0f,1.0f,  1.0f, 1.0f,-1.0f }, // 6

   // Quad 3
   { 1.0f,0.0f,1.0f, -1.0f,-1.0f,-1.0f }, // 4
   { 1.0f,0.0f,0.0f,  1.0f,-1.0f,-1.0f }, // 7
   { 0.0f,1.0f,0.0f,  1.0f,-1.0f, 1.0f }, // 1
   { 1.0f,0.0f,0.0f, -1.0f,-1.0f, 1.0f }, // 0

   // Quad 4
   { 1.0f,0.0f,0.0f,  1.0f,-1.0f,-1.0f }, // 7
   { 1.0f,1.0f,1.0f,  1.0f, 1.0f,-1.0f }, // 6
   { 0.0f,0.0f,1.0f,  1.0f, 1.0f, 1.0f }, // 2
   { 0.0f,1.0f,0.0f,  1.0f,-1.0f, 1.0f }, // 1

   // Quad 5
   { 1.0f,0.0f,1.0f, -1.0f,-1.0f,-1.0f }, // 4
   { 1.0f,0.0f,0.0f, -1.0f,-1.0f, 1.0f }, // 0
   { 1.0f,1.0f,0.0f, -1.0f, 1.0f, 1.0f }, // 3
   { 0.0f,1.0f,1.0f, -1.0f, 1.0f,-1.0f }  // 5
};

//
// Now, to save ourselves the bandwidth of passing a bunch or redundant vertices
// down the graphics pipeline, we shorten our vertex list and pass only the
// unique vertices. We then create a indices array, which contains index values
// that reference vertices in our vertex array.
//
// In other words, the vertex array doens't actually define our cube anymore,
// it only holds the unique vertices; it's the indices array that now defines
// the cube's geometry.
//

Vertex g_cubeVertices_indexed[] =
{
   { 1.0f,0.0f,0.0f,  -1.0f,-1.0f, 1.0f }, // 0
   { 0.0f,1.0f,0.0f,   1.0f,-1.0f, 1.0f }, // 1
   { 0.0f,0.0f,1.0f,   1.0f, 1.0f, 1.0f }, // 2
   { 1.0f,1.0f,0.0f,  -1.0f, 1.0f, 1.0f }, // 3
   { 1.0f,0.0f,1.0f,  -1.0f,-1.0f,-1.0f }, // 4
   { 0.0f,1.0f,1.0f,  -1.0f, 1.0f,-1.0f }, // 5
   { 1.0f,1.0f,1.0f,   1.0f, 1.0f,-1.0f }, // 6
   { 1.0f,0.0f,0.0f,   1.0f,-1.0f,-1.0f }, // 7
};

GLubyte g_cubeIndices[] =
{
   0, 1, 2, 3, // Quad 0
   4, 5, 6, 7, // Quad 1
   5, 3, 2, 6, // Quad 2
   4, 7, 1, 0, // Quad 3
   7, 6, 2, 1, // Quad 4
   4, 0, 3, 5  // Quad 5
};

//
// Note: While the cube above makes for a good example of how indexed geometry
//       works. There are many situations which can prevent you from using
//       an indices array to its full potential.
//
//       For example, if our cube required normals for lighting, things would
//       become problematic since each vertex would be shared between three
//       faces of the cube. This would not give you the lighting effect that
//       you really want since the best you could do would be to average the
//       normal's value between the three faces which used it.
//
//       Another example would be texture coordinates. If our cube required
//       unique texture coordinates for each face, you really wouldn?t gain
//       much from using an indices array since each vertex would require a
//       different texture coordinate depending on which face it was being
//       used in.
//

//-----------------------------------------------------------------------------
// PROTOTYPES
//-----------------------------------------------------------------------------
void init( int argc, char **argv );
void render(void);
void shutDown(void);

struct POINT
{
   int x;
   int y;
};

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
   POINT ptLastMousePosit = {0,0};
   POINT ptCurrentMousePosit = {0,0};
   bool bMousing = false;

   init(argc, argv);

   Atom wmDeleteMessage = XInternAtom(g_pDisplay, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(g_pDisplay, g_window, &wmDeleteMessage, 1);

   XEvent event;
   bool bRunning = true;

   while( bRunning )
   {
      while( XPending(g_pDisplay) )
      {
         XNextEvent( g_pDisplay, &event );

         switch( event.type )
         {
         case KeyPress:
         {
            KeySym xsym = XLookupKeysym(&event.xkey, 0);

            switch( xsym)
            {
            case XK_Escape:
               bRunning = false;
               break;

            case XK_F1:
               g_bUseIndexedGeometry = !g_bUseIndexedGeometry;
               printf("Use indexed geometry: %u\n", g_bUseIndexedGeometry);
               break;
            case XK_F2:
               break;

            case XK_F3:
               break;

            case XK_F4:
               break;

            case XK_F5:
               break;

            case XK_F6:
               break;

            case XK_F7:
               break;
            }
         }
         break;

         case ButtonPress:
         {
            if( event.xbutton.button == 1 )
            {
               ptLastMousePosit.x = ptCurrentMousePosit.x = event.xmotion.x;
               ptLastMousePosit.y = ptCurrentMousePosit.y = event.xmotion.y;
               bMousing = true;
            }
         }
         break;

         case ButtonRelease:
         {
            if( event.xbutton.button == 1 )
            {
               bMousing = false;
            }
         }
         break;

         case MotionNotify:
         {
            if( bMousing )
            {
               ptCurrentMousePosit.x = event.xmotion.x;
               ptCurrentMousePosit.y = event.xmotion.y;

               if( bMousing )
               {
                  g_fSpinZ -= (ptCurrentMousePosit.x - ptLastMousePosit.x);
                  g_fSpinY -= (ptCurrentMousePosit.y - ptLastMousePosit.y);
               }

               ptLastMousePosit.x = ptCurrentMousePosit.x;
               ptLastMousePosit.y = ptCurrentMousePosit.y;
            }
         }
         break;

         case ConfigureNotify:
         {
            int nWidth  = event.xconfigure.width;
            int nHeight = event.xconfigure.height;
            glViewport(0, 0, nWidth, nHeight);

            glMatrixMode( GL_PROJECTION );
            glLoadIdentity();
            gluPerspective( 45.0, (GLdouble)nWidth / (GLdouble)nHeight, 0.1, 1000.0);
         }
         break;

         case DestroyNotify:
         {
            bRunning = false;
         }
         break;
         case ClientMessage:
         {
            if(event.xclient.data.l[0] == (int)wmDeleteMessage)
               bRunning = false;
         }
         break;
         default: break;
         }
      }

      render();
   }

   shutDown();

   return 0;
}

//-----------------------------------------------------------------------------
// Name: init()
// Desc:
//-----------------------------------------------------------------------------
void init( int argc, char **argv )
{
   // Open a connection to the X server
   g_pDisplay = XOpenDisplay( NULL );

   if( g_pDisplay == NULL )
      exit(1);

   int errorBase;
   int eventBase;

   // Make sure OpenGL's GLX extension supported
   if( !glXQueryExtension( g_pDisplay, &errorBase, &eventBase ) )
      exit(1);

   // Find an appropriate visual

   int doubleBufferVisual[]  =
   {
      GLX_RGBA,           // Needs to support OpenGL
      GLX_DEPTH_SIZE, 16, // Needs to support a 16 bit depth buffer
      GLX_DOUBLEBUFFER,   // Needs to support double-buffering
      None                // end of list
   };

   int singleBufferVisual[] =
   {
      GLX_RGBA,           // Needs to support OpenGL
      GLX_DEPTH_SIZE, 16, // Needs to support a 16 bit depth buffer
      None                // end of list
   };

   // Try for the double-bufferd visual first
   XVisualInfo *visualInfo = NULL;
   visualInfo = glXChooseVisual( g_pDisplay, DefaultScreen(g_pDisplay), doubleBufferVisual );

   if( visualInfo == NULL )
   {
      // If we can't find a double-bufferd visual, try for a single-buffered visual...
      visualInfo = glXChooseVisual( g_pDisplay, DefaultScreen(g_pDisplay), singleBufferVisual );

      if( visualInfo == NULL )
         exit(1);

      g_bDoubleBuffered = false;
   }

   // Create an OpenGL rendering context
   g_glxContext = glXCreateContext( g_pDisplay, visualInfo,
                                    NULL,      // No sharing of display lists
                                    GL_TRUE ); // Direct rendering if possible

   if( g_glxContext == NULL )
      exit(1);

   // Create an X colormap since we're probably not using the default visual
   Colormap colorMap;
   colorMap = XCreateColormap( g_pDisplay,
                               RootWindow(g_pDisplay, visualInfo->screen),
                               visualInfo->visual,
                               AllocNone );

   XSetWindowAttributes winAttr;
   winAttr.colormap     = colorMap;
   winAttr.border_pixel = 0;
   winAttr.event_mask   = ExposureMask           |
                          VisibilityChangeMask   |
                          KeyPressMask           |
                          KeyReleaseMask         |
                          ButtonPressMask        |
                          ButtonReleaseMask      |
                          PointerMotionMask      |
                          StructureNotifyMask    |
                          SubstructureNotifyMask |
                          FocusChangeMask;

   // Create an X window with the selected visual
   g_window = XCreateWindow( g_pDisplay, RootWindow(g_pDisplay, visualInfo->screen),
                             0, 0, 640, 480, 0, visualInfo->depth, InputOutput,
                             visualInfo->visual, CWBorderPixel | CWColormap | CWEventMask,
                             &winAttr );

   XSetStandardProperties( g_pDisplay, g_window, "OpenGL - Indexed Geometry",
                           "ogl_indexed_geometry", None, argv, argc, NULL );

   glXMakeCurrent( g_pDisplay, g_window, g_glxContext );
   XMapWindow( g_pDisplay, g_window );

   glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
   glEnable( GL_DEPTH_TEST );

   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   gluPerspective( 45.0f, 640.0f / 480.0f, 0.1f, 100.0f);
}

//-----------------------------------------------------------------------------
// Name: shutDown()
// Desc:
//-----------------------------------------------------------------------------
void shutDown(void)
{
   if( g_glxContext != NULL )
   {
      glXMakeCurrent( g_pDisplay, (GLXDrawable)NULL, NULL );
      glXDestroyContext( g_pDisplay, g_glxContext );
      g_glxContext = NULL;
   }

   if (g_window)
   {
      XDestroyWindow( g_pDisplay, g_window );
      g_window = (Window)NULL;
   }

   if (g_pDisplay)
   {
      XCloseDisplay( g_pDisplay );
      g_pDisplay = NULL;
   }
}

//-----------------------------------------------------------------------------
// Name: render()
// Desc:
//-----------------------------------------------------------------------------
void render( void )
{
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
   glTranslatef( 0.0f, 0.0f, -5.0f );
   glRotatef( -g_fSpinY, 1.0f, 0.0f, 0.0f );
   glRotatef( -g_fSpinZ, 0.0f, 0.0f, 1.0f );

   if( g_bUseIndexedGeometry == true )
   {
      glInterleavedArrays( GL_C3F_V3F, 0, g_cubeVertices_indexed );
      glDrawElements( GL_QUADS, 24, GL_UNSIGNED_BYTE, g_cubeIndices );
   }
   else
   {
      glInterleavedArrays( GL_C3F_V3F, 0, g_cubeVertices );
      glDrawArrays( GL_QUADS, 0, 24 );
   }

   glXSwapBuffers( g_pDisplay, g_window );
}











