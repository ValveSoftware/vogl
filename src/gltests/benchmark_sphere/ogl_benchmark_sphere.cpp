//-----------------------------------------------------------------------------
//           Name: ogl_benchmark_sphere.cpp
//         Author: Kevin Harris
//  Last Modified: 02/10/05
//    Description: Renders a textured sphere using either Immediate Mode calls,
//                 Immediate Mode calls cached in a Display List, or as a
//                 collection of geometric data stored in an interleaved
//                 fashion within a Vertex Array.
//
//   Control Keys: Left Mouse Button - Spin the view.
//                 F1 - Decrease sphere precision.
//                 F2 - Increase sphere precision.
//                 F3 - Use Immediate mode
//                 F4 - Use a Display List
//                 F5 - Use a Vertex Array
//                 F6 - Perform Benchmarking
//                 F7 - Toggle wire-frame mode.
//
//
// Linux tweaks: Rich Geldreich 11/24/13
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
// DEFINES
//-----------------------------------------------------------------------------
#define IMMEDIATE_MODE 0
#define DISPLAY_LIST   1
#define VERTEX_ARRAY   2

//-----------------------------------------------------------------------------
// GLOBALS
//-----------------------------------------------------------------------------
Display   *g_pDisplay = NULL;
Window     g_window;
GLXContext g_glxContext;
bool       g_bDoubleBuffered = GL_TRUE;

float g_fSpinX           = 0.0f;
float g_fSpinY           = 0.0f;
int   g_nLastMousePositX = 0;
int   g_nLastMousePositY = 0;
bool  g_bMousing         = false;

GLuint g_sphereDList;
GLuint g_textureID = 0;

bool    g_bRenderInWireFrame = false;
GLuint  g_nCurrentMode = IMMEDIATE_MODE;
GLuint  g_nPrecision  = 100;
GLuint  g_nNumSphereVertices;
GLfloat g_fMarsSpin   = 0.0f;

// A custom data structure for our interleaved vertex attributes
// The interleaved layout will be, GL_T2F_N3F_V3F
struct Vertex
{
   float tu, tv;
   float nx, ny, nz;
   float vx, vy, vz;
};

Vertex *g_pSphereVertices = NULL; // Points to Vertex Array

struct BMPImage
{
   int   width;
   int   height;
   char *data;
};

//-----------------------------------------------------------------------------
// PROTOTYPES
//-----------------------------------------------------------------------------
int main(int argc, char **argv);
void render(void);
void init(void);
void shutDown(void);
void getBitmapImageData(char *pFileName, BMPImage *pImage);
void loadTexture(void);
void renderSphere(float cx, float cy, float cz, float r, int n);
void createSphereDisplayList();
void createSphereGeometry( float cx, float cy, float cz, float r, int n);
void setVertData(int index,float tu, float tv, float nx, float ny, float nz,
                 float vx, float vy, float vz);
void doBenchmark(void);

//-----------------------------------------------------------------------------
// Name: main()
// Desc:
//-----------------------------------------------------------------------------
int main( int argc, char **argv )
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

   XSetStandardProperties( g_pDisplay, g_window, "OpenGL - Benchmark Sphere",
                           "ogl_benchmark_sphere", None, argv, argc, NULL );

   glXMakeCurrent( g_pDisplay, g_window, g_glxContext );
   XMapWindow( g_pDisplay, g_window );

   init();

   //
   // Enter the render loop and don't forget to dispatch X events as
   // they occur.
   //

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

               //switch( XKeycodeToKeysym( g_pDisplay, event.xkey.keycode, 0 ) )
               switch( xsym )
               {
                  case XK_Escape:
                     bRunning = false;
                     break;

                  case XK_F1:
                     if( g_nPrecision > 5 )
                        g_nPrecision -= 2;

                     if( g_nCurrentMode == DISPLAY_LIST )
                        createSphereDisplayList();

                     if( g_nCurrentMode == VERTEX_ARRAY ||
                           g_nCurrentMode == IMMEDIATE_MODE)
                     {
                        createSphereGeometry( 0.0f, 0.0f, 0.0f, 1.5f, g_nPrecision );
                     }

                     cout << "Sphere Resolution = " << g_nPrecision << endl;
                     break;

                  case XK_F2:
                     if( g_nPrecision < 30000 )
                        g_nPrecision += 2;

                     if( g_nCurrentMode == DISPLAY_LIST )
                     {
                        createSphereDisplayList();
                     }

                     if( g_nCurrentMode == VERTEX_ARRAY ||
                           g_nCurrentMode == IMMEDIATE_MODE )
                     {
                        createSphereGeometry( 0.0f, 0.0f, 0.0f, 1.5f, g_nPrecision );
                     }

                     cout << "Sphere Resolution = " << g_nPrecision << endl;
                     break;

                  case XK_F3:
                     g_nCurrentMode = IMMEDIATE_MODE;
                     cout << "Render Method: Immediate Mode" << endl;
                     createSphereGeometry( 0.0f, 0.0f, 0.0f, 1.5f, g_nPrecision );
                     break;

                  case XK_F4:
                     g_nCurrentMode = DISPLAY_LIST;
                     createSphereDisplayList();
                     cout << "Render Method: Display List" << endl;
                     break;

                  case XK_F5:
                     g_nCurrentMode = VERTEX_ARRAY;
                     createSphereGeometry( 0.0f, 0.0f, 0.0f, 1.5f, g_nPrecision );
                     cout << "Render Method: Vertex Array" << endl;
                     break;

                  case XK_F6:
                     cout << endl;
                     cout << "Benchmark Initiated - Standby..." << endl;
                     doBenchmark();
                     break;

                  case XK_F7:
                     g_bRenderInWireFrame = !g_bRenderInWireFrame;
                     break;
               }

//KeySym key; // KeyPress Events
//char text[255]; // char buffer for KeyPress Events
//if( XLookupString( &event.xkey, text, 255, &key, 0 ) == 1 )
//{
//	if( (text[0]=='q') || (text[0]=='Q') )
//		cout << "You pressed the Q/q key!" << endl;
//	else
//		cout << "You pressed the " << text[0] << " key!"  << endl;
//}
            }
            break;

            case ButtonPress:
            {
               if( event.xbutton.button == 1 )
               {
                  g_nLastMousePositX = event.xmotion.x;
                  g_nLastMousePositY = event.xmotion.y;
                  g_bMousing = true;
               }
            }
            break;

            case ButtonRelease:
            {
               if( event.xbutton.button == 1 )
                  g_bMousing = false;
            }
            break;

            case MotionNotify:
            {
               if( g_bMousing )
               {
                  g_fSpinX -= (event.xmotion.x - g_nLastMousePositX);
                  g_fSpinY -= (event.xmotion.y - g_nLastMousePositY);

                  g_nLastMousePositX = event.xmotion.x;
                  g_nLastMousePositY = event.xmotion.y;
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
               gluPerspective( 45.0, (GLdouble)nWidth / (GLdouble)nHeight, 0.1, 100.0);
            }
            break;

            case DestroyNotify:
            {
               bRunning = false;
            }
            break;
         }
      }

      render();
   }

   shutDown();

   XDestroyWindow( g_pDisplay, g_window );
   XCloseDisplay( g_pDisplay );
}

//-----------------------------------------------------------------------------
// Name: init()
// Desc: Init OpenGL context for rendering
//-----------------------------------------------------------------------------
void init( void )
{
   glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
   glEnable( GL_TEXTURE_2D );
   glEnable( GL_DEPTH_TEST );

   loadTexture();

   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   gluPerspective( 45.0f, 640.0f / 480.0f, 0.1f, 100.0f );

   //
   // Create the first sphere...
   //

   // Inform the user of the current mode
   cout << "Render Method: Immediate Mode" << endl;

   createSphereGeometry( 0.0f, 0.0f, 0.0f, 1.5f, g_nPrecision );
}

//-----------------------------------------------------------------------------
// Name: shutDown()
// Desc:
//-----------------------------------------------------------------------------
void shutDown( void )
{
   glDeleteTextures( 1, &g_textureID );
   glDeleteLists( g_sphereDList, 0 );

   if( g_glxContext != NULL )
   {
      // Release the context
      glXMakeCurrent( g_pDisplay, None, NULL );

      // Delete the context
      glXDestroyContext( g_pDisplay, g_glxContext );
      g_glxContext = NULL;
   }
}

//-----------------------------------------------------------------------------
// Name: getBitmapImageData()
// Desc: Simply image loader for 24 bit BMP files.
//-----------------------------------------------------------------------------
void getBitmapImageData( char *pFileName, BMPImage *pImage )
{
   FILE *pFile = NULL;
   unsigned short nNumPlanes;
   unsigned short nNumBPP;
   int i;

   if( (pFile = fopen(pFileName, "rb") ) == NULL )
      cout << "ERROR: getBitmapImageData - %s not found " << pFileName << "." << endl;

   // Seek forward to width and height info
   fseek( pFile, 18, SEEK_CUR );

   if( (i = fread(&pImage->width, 4, 1, pFile) ) != 1 )
      cout << "ERROR: getBitmapImageData - Couldn't read width from " << pFileName << "." << endl;

   if( (i = fread(&pImage->height, 4, 1, pFile) ) != 1 )
      cout << "ERROR: getBitmapImageData - Couldn't read height from " << pFileName << "." << endl;

   if( (fread(&nNumPlanes, 2, 1, pFile) ) != 1 )
      cout << "ERROR: getBitmapImageData - Couldn't read plane count from " << pFileName << "." << endl;

   if( nNumPlanes != 1 )
      cout << "ERROR: getBitmapImageData - Plane count from " << pFileName << " is not 1: " << nNumPlanes << "." << endl;

   if( (i = fread(&nNumBPP, 2, 1, pFile)) != 1 )
      cout << "ERROR: getBitmapImageData - Couldn't read BPP from " << pFileName << endl;

   if( nNumBPP != 24 )
      cout << "ERROR: getBitmapImageData - BPP from " << pFileName << " is not 24: " << nNumBPP << "." << endl;

   // Seek forward to image data
   fseek( pFile, 24, SEEK_CUR );

   // Calculate the image's total size in bytes. Note how we multiply the
   // result of (width * height) by 3. This is becuase a 24 bit color BMP
   // file will give you 3 bytes per pixel.
   int nTotalImagesize = (pImage->width * pImage->height) * 3;

   pImage->data = (char*) malloc( nTotalImagesize );

   if( (i = fread(pImage->data, nTotalImagesize, 1, pFile) ) != 1 )
      cout << "ERROR: getBitmapImageData - Couldn't read image data from " << pFileName << "." << endl;

   //
   // Finally, rearrange BGR to RGB
   //

   char charTemp;
   for( i = 0; i < nTotalImagesize; i += 3 )
   {
      charTemp = pImage->data[i];
      pImage->data[i] = pImage->data[i+2];
      pImage->data[i+2] = charTemp;
   }
}

//-----------------------------------------------------------------------------
// Name: loadTexture()
// Desc:
//-----------------------------------------------------------------------------
void loadTexture( void )
{
   BMPImage textureImage;

   getBitmapImageData( (char *)"mars.bmp", &textureImage );

   glGenTextures( 1, &g_textureID );
   glBindTexture( GL_TEXTURE_2D, g_textureID );

   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

   glTexImage2D( GL_TEXTURE_2D, 0, 3, textureImage.width, textureImage.height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, textureImage.data );
}

//-----------------------------------------------------------------------------
// Name: renderSphere()
// Desc: Create a sphere centered at cy, cx, cz with radius r, and
//       precision p. Based on a function Written by Paul Bourke.
//       http://astronomy.swin.edu.au/~pbourke/opengl/sphere/
//-----------------------------------------------------------------------------
void renderSphere( float cx, float cy, float cz, float r, int p )
{
   const float TWOPI  = 6.28318530717958f;
   const float PIDIV2 = 1.57079632679489f;

   float theta1 = 0.0;
   float theta2 = 0.0;
   float theta3 = 0.0;

   float ex = 0.0f;
   float ey = 0.0f;
   float ez = 0.0f;

   float px = 0.0f;
   float py = 0.0f;
   float pz = 0.0f;

   // Disallow a negative number for radius.
   if( r < 0 )
      r = -r;

   // Disallow a negative number for precision.
   if( p < 0 )
      p = -p;

   // If the sphere is too small, just render a OpenGL point instead.
   if( p < 4 || r <= 0 )
   {
      glBegin( GL_POINTS );
      glVertex3f( cx, cy, cz );
      glEnd();
      return;
   }

   for( int i = 0; i < p/2; ++i )
   {
      theta1 = i * TWOPI / p - PIDIV2;
      theta2 = (i + 1) * TWOPI / p - PIDIV2;

      glBegin( GL_TRIANGLE_STRIP );
      {
         for( int j = 0; j <= p; ++j )
         {
            theta3 = j * TWOPI / p;

            ex = cosf(theta2) * cosf(theta3);
            ey = sinf(theta2);
            ez = cosf(theta2) * sinf(theta3);
            px = cx + r * ex;
            py = cy + r * ey;
            pz = cz + r * ez;

            glNormal3f( ex, ey, ez );
            glTexCoord2f( -(j/(float)p) , 2*(i+1)/(float)p );
            glVertex3f( px, py, pz );

            ex = cosf(theta1) * cosf(theta3);
            ey = sinf(theta1);
            ez = cosf(theta1) * sinf(theta3);
            px = cx + r * ex;
            py = cy + r * ey;
            pz = cz + r * ez;

            glNormal3f( ex, ey, ez );
            glTexCoord2f( -(j/(float)p), 2*i/(float)p );
            glVertex3f( px, py, pz );
         }
      }
      glEnd();
   }
}

//-----------------------------------------------------------------------------
// Name: createSphereDisplayList()
// Desc: Build Sphere Display List
//-----------------------------------------------------------------------------
void createSphereDisplayList()
{
   glDeleteLists( g_sphereDList, 0 );

   static bool firstPass = true;

   if( firstPass )
   {
      g_sphereDList = glGenLists(1);
      firstPass     = false;
   }

   if( g_sphereDList != 0 )
   {
      glNewList( g_sphereDList, GL_COMPILE );
      // Cache the calls needed to render a sphere
      renderSphere( 0.0f, 0.0f, 0.0f, 1.5f, g_nPrecision );
      glEndList();
   }
}

//-----------------------------------------------------------------------------
// Name: setVertData()
// Desc: Helper function for createSphereGeometry()
//-----------------------------------------------------------------------------
void setVertData( int index,
                  float tu, float tv,
                  float nx, float ny, float nz,
                  float vx, float vy, float vz )
{
   (g_pSphereVertices+index)->tu = tu;
   (g_pSphereVertices+index)->tv = tv;
   (g_pSphereVertices+index)->nx = nx;
   (g_pSphereVertices+index)->ny = ny;
   (g_pSphereVertices+index)->nz = nz;
   (g_pSphereVertices+index)->vx = vx;
   (g_pSphereVertices+index)->vy = vy;
   (g_pSphereVertices+index)->vz = vz;
}

//-----------------------------------------------------------------------------
// Name: createSphereGeometry()
// Desc: Creates a sphere as an array of vertex data suitable to be fed into a
//       OpenGL vertex array. The sphere will be centered at cy, cx, cz with
//       radius r, and precision p. Based on a function Written by Paul Bourke.
//       http://astronomy.swin.edu.au/~pbourke/opengl/sphere/
//-----------------------------------------------------------------------------
void createSphereGeometry( float cx, float cy, float cz, float r, int p )
{
   const float TWOPI = 6.28318530717958f;
   const float PIDIV2 = 1.57079632679489f;

   float theta1 = 0.0;
   float theta2 = 0.0;
   float theta3 = 0.0;

   float ex = 0.0f;
   float ey = 0.0f;
   float ez = 0.0f;

   float px = 0.0f;
   float py = 0.0f;
   float pz = 0.0f;

   float tu  = 0.0f;
   float tv  = 0.0f;

   //-------------------------------------------------------------------------
   // If sphere precision is set to 4, then 20 verts will be needed to
   // hold the array of GL_TRIANGLE_STRIP(s) and so on...
   //
   // Example:
   //
   // total_verts = (p/2) * ((p+1)*2)
   // total_verts = (4/2) * (  5  *2)
   // total_verts =   2   *  10
   // total_verts =      20
   //-------------------------------------------------------------------------

   g_nNumSphereVertices = (p/2) * ((p+1)*2);

   if( g_pSphereVertices != NULL )
   {
      delete []g_pSphereVertices;
      g_pSphereVertices = NULL;
      g_pSphereVertices = new Vertex[g_nNumSphereVertices];
   }
   else
   {
      g_pSphereVertices = new Vertex[g_nNumSphereVertices];
   }

   // Disallow a negative number for radius.
   if( r < 0 )
      r = -r;

   // Disallow a negative number for precision.
   if( p < 4 )
      p = 4;

   int k = -1;

   for( int i = 0; i < p/2; ++i )
   {
      theta1 = i * TWOPI / p - PIDIV2;
      theta2 = (i + 1) * TWOPI / p - PIDIV2;

      for( int j = 0; j <= p; ++j )
      {
         theta3 = j * TWOPI / p;

         ex = cosf(theta2) * cosf(theta3);
         ey = sinf(theta2);
         ez = cosf(theta2) * sinf(theta3);
         px = cx + r * ex;
         py = cy + r * ey;
         pz = cz + r * ez;
         tu  = -(j/(float)p);
         tv  = 2*(i+1)/(float)p;

         ++k;
         setVertData( k, tu, tv, ex, ey, ez, px, py, pz );

         ex = cosf(theta1) * cosf(theta3);
         ey = sinf(theta1);
         ez = cosf(theta1) * sinf(theta3);
         px = cx + r * ex;
         py = cy + r * ey;
         pz = cz + r * ez;
         tu  = -(j/(float)p);
         tv  = 2*i/(float)p;

         ++k;
         setVertData( k, tu, tv, ex, ey, ez, px, py, pz );
      }
   }
}

//-----------------------------------------------------------------------------
// Name: doBenchmark()
// Desc:
//-----------------------------------------------------------------------------
void doBenchmark()
{
   timeb start;
   timeb finish;
   float  fElapsed = 0.0f;
   int    nFrames  = 1000;

   ftime( &start );  // Get the time

   while( nFrames-- ) // Loop away
      render();

   ftime( &finish ); // Get the time again

   fElapsed  = (float)(finish.time - start.time);                // This is accurate to one second
   fElapsed += (float)((finish.millitm - start.millitm)/1000.0); // This gets it down to one ms

   cout << endl;
   cout << "-- Benchmark Report --" << endl;

   if( g_nCurrentMode == IMMEDIATE_MODE )
      cout << "Render Method:     Immediate Mode" << endl;
   if( g_nCurrentMode == DISPLAY_LIST )
      cout << "Render Method:     Display List" << endl;
   if( g_nCurrentMode == VERTEX_ARRAY )
      cout << "Render Method:     Vertex Array" << endl;

   cout << "Frames Rendered:   1000" << endl;
   cout << "Sphere Resolution: " << g_nPrecision << endl;
   cout << "Primitive Used:    GL_TRIANGLE_STRIP" << endl;
   cout << "Elapsed Time:      " << fElapsed << endl;
   cout << "Frames Per Second: " << 1000.0/fElapsed << endl;
   cout << endl;
}

//-----------------------------------------------------------------------------
// Name: render()
// Desc: Called when the GLX window is ready to render
//-----------------------------------------------------------------------------
void render( void )
{
   // Clear the screen and the depth buffer
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
   glTranslatef( 0.0f, 0.0f, -5.0f );
   glRotatef( -g_fSpinY, 1.0f, 0.0f, 0.0f );
   glRotatef( -g_fSpinX, 0.0f, 1.0f, 0.0f );

   if( g_bRenderInWireFrame == true )
      glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
   else
      glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

   //
   // Render test sphere...
   //

   glBindTexture( GL_TEXTURE_2D, g_textureID );

   if( g_nCurrentMode == IMMEDIATE_MODE )
   {
      // Render a textured sphere using immediate mode

      // To be fair to immediate mode, we won't force it incur the overhead
      // of calling hundreds of math subroutines to generate a sphere each
      // frame, instead, we'll use the same array that we would use for
      // testing the vertex array, but we'll make the immediate mode calls
      // ourselves. This is more typical of how a real app would use
      // immediate mode calls.

      glBegin( GL_TRIANGLE_STRIP );
      {
         for( GLuint i = 0; i < g_nNumSphereVertices; ++i )
         {
            glNormal3f( (g_pSphereVertices+i)->nx,
                        (g_pSphereVertices+i)->ny,
                        (g_pSphereVertices+i)->nz );

            glTexCoord2f( (g_pSphereVertices+i)->tu,
                          (g_pSphereVertices+i)->tv );

            glVertex3f( (g_pSphereVertices+i)->vx,
                        (g_pSphereVertices+i)->vy,
                        (g_pSphereVertices+i)->vz );
         }
      }
      glEnd();
   }

   if( g_nCurrentMode == DISPLAY_LIST )
   {
      // Render a textured sphere as a display list
      glCallList( g_sphereDList );
   }

   if( g_nCurrentMode == VERTEX_ARRAY )
   {
      // Render a textured sphere using a vertex array
      glInterleavedArrays( GL_T2F_N3F_V3F, 0, g_pSphereVertices );
      glDrawArrays( GL_TRIANGLE_STRIP, 0, g_nNumSphereVertices );
   }

   if( g_bDoubleBuffered )
      glXSwapBuffers( g_pDisplay, g_window ); // Buffer swap does implicit glFlush
   else
      glFlush(); // Explicit flush for single buffered case
}

