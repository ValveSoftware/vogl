// Stencil.c
// OpenGL SuperBible, 3rd Edition
// Richard S. Wright Jr.
// rwright@starstonesoftware.com

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include <math.h>

// Initial square position and size
GLfloat x = 0.0f;
GLfloat y = 0.0f;
GLfloat rsize = 25;

// Step size in x and y directions
// (number of pixels to move each time)
GLfloat xstep = 1.0f;
GLfloat ystep = 1.0f;

// Keep track of windows changing width and height
GLfloat windowWidth;
GLfloat windowHeight;
	
///////////////////////////////////////////////////////////
// Called to draw scene
void RenderScene(void)
    {
    GLdouble dRadius = 0.1; // Initial radius of spiral
    GLdouble dAngle;        // Looping variable
            
    // Clear blue window
    glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
        
    // Use 0 for clear stencil, enable stencil test
    glClearStencil(0.0f);
    glEnable(GL_STENCIL_TEST);

    // Clear color and stencil buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                
    // All drawing commands fail the stencil test, and are not
    // drawn, but increment the value in the stencil buffer. 
    glStencilFunc(GL_NEVER, 0x0, 0x0);
    glStencilOp(GL_INCR, GL_INCR, GL_INCR);

    // Spiral pattern will create stencil pattern
    // Draw the spiral pattern with white lines. We 
    // make the lines  white to demonstrate that the 
    // stencil function prevents them from being drawn
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_STRIP);
        for(dAngle = 0; dAngle < 400.0; dAngle += 0.1)
            {
            glVertex2d(dRadius * cos(dAngle), dRadius * sin(dAngle));
            dRadius *= 1.002;
            }
    glEnd();
            
    // Now, allow drawing, except where the stencil pattern is 0x1
    // and do not make any further changes to the stencil buffer
    glStencilFunc(GL_NOTEQUAL, 0x1, 0x1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        
    // Now draw red bouncing square
    // (x and y) are modified by a timer function
    glColor3f(1.0f, 0.0f, 0.0f);
    glRectf(x, y, x + rsize, y - rsize);
    
    // All done, do the buffer swap
    glutSwapBuffers();
    }

///////////////////////////////////////////////////////////
// Called by GLUT library when idle (window not being
// resized or moved)
void TimerFunction(int value)
    {
    // Reverse direction when you reach left or right edge
    if(x > windowWidth-rsize || x < -windowWidth)
        xstep = -xstep;

    // Reverse direction when you reach top or bottom edge
    if(y > windowHeight || y < -windowHeight + rsize)
        ystep = -ystep;


    // Check bounds. This is in case the window is made
    // smaller while the rectangle is bouncing and the 
	// rectangle suddenly finds itself outside the new
    // clipping volume
    if(x > windowWidth-rsize)
        x = windowWidth-rsize-1;

    if(y > windowHeight)
        y = windowHeight-1; 

	// Actually move the square
    x += xstep;
    y += ystep;

     // Redraw the scene with new coordinates
    glutPostRedisplay();
    glutTimerFunc(33,TimerFunction, 1);
    }


///////////////////////////////////////////////////////////
// Called by GLUT library when the window has chanaged size
void ChangeSize(int w, int h)
    {
    GLfloat aspectRatio;

    // Prevent a divide by zero
    if(h == 0)
        h = 1;
		
    // Set Viewport to window dimensions
    glViewport(0, 0, w, h);

    // Reset coordinate system
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Establish clipping volume (left, right, bottom, top, near, far)
    aspectRatio = (GLfloat)w / (GLfloat)h;
    if (w <= h) 
        {
        windowWidth = 100;
        windowHeight = 100 / aspectRatio;
        glOrtho (-100.0, 100.0, -windowHeight, windowHeight, 1.0, -1.0);
        }
    else 
        {
        windowWidth = 100 * aspectRatio;
        windowHeight = 100;
        glOrtho (-windowWidth, windowWidth, -100.0, 100.0, 1.0, -1.0);
        }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    }
    
    
///////////////////////////////////////////////////////////
// Program entry point
int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_STENCIL);
	glutInitWindowSize(800,600);
	glutCreateWindow("OpenGL Stencil Test");
	glutReshapeFunc(ChangeSize);
	glutDisplayFunc(RenderScene);
        glutTimerFunc(33, TimerFunction, 1);
	glutMainLoop();

	return 0;
	}
