// PStipple.c
// OpenGL SuperBible, Chapter 3
// Demonstrates OpenGL Polygon Stippling
// Program by Richard S. Wright Jr.
#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include <math.h>


// Define a constant for the value of PI
#define GL_PI 3.1415f

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;

// Bitmap of camp fire
GLubyte fire[128] = { 0x00, 0x00, 0x00, 0x00, 
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0xc0,
				   0x00, 0x00, 0x01, 0xf0,
				   0x00, 0x00, 0x07, 0xf0,
				   0x0f, 0x00, 0x1f, 0xe0,
				   0x1f, 0x80, 0x1f, 0xc0,
				   0x0f, 0xc0, 0x3f, 0x80,	
				   0x07, 0xe0, 0x7e, 0x00,
				   0x03, 0xf0, 0xff, 0x80,
				   0x03, 0xf5, 0xff, 0xe0,
				   0x07, 0xfd, 0xff, 0xf8,
				   0x1f, 0xfc, 0xff, 0xe8,
				   0xff, 0xe3, 0xbf, 0x70, 
				   0xde, 0x80, 0xb7, 0x00,
				   0x71, 0x10, 0x4a, 0x80,
				   0x03, 0x10, 0x4e, 0x40,
				   0x02, 0x88, 0x8c, 0x20,
				   0x05, 0x05, 0x04, 0x40,
				   0x02, 0x82, 0x14, 0x40,
				   0x02, 0x40, 0x10, 0x80, 
				   0x02, 0x64, 0x1a, 0x80,
				   0x00, 0x92, 0x29, 0x00,
				   0x00, 0xb0, 0x48, 0x00,
				   0x00, 0xc8, 0x90, 0x00,
				   0x00, 0x85, 0x10, 0x00,
				   0x00, 0x03, 0x00, 0x00,
				   0x00, 0x00, 0x10, 0x00 };
				   

// Called to draw scene
void RenderScene(void)
	{
	// Clear the window
	glClear(GL_COLOR_BUFFER_BIT);

	// Save matrix state and do the rotation
	glPushMatrix();
	glRotatef(xRot, 1.0f, 0.0f, 0.0f);
	glRotatef(yRot, 0.0f, 1.0f, 0.0f);

	// Begin the stop sign shape,
	// use a standard polygon for simplicity
	glBegin(GL_POLYGON);
		glVertex2f(-20.0f, 50.0f);
		glVertex2f(20.0f, 50.0f);
		glVertex2f(50.0f, 20.0f);
		glVertex2f(50.0f, -20.0f);
		glVertex2f(20.0f, -50.0f);
		glVertex2f(-20.0f, -50.0f);
		glVertex2f(-50.0f, -20.0f);
		glVertex2f(-50.0f, 20.0f);
	glEnd();

	// Restore transformations
	glPopMatrix();

	// Flush drawing commands
	glutSwapBuffers();
	}


// This function does any needed initialization on the rendering
// context. 
void SetupRC()
	{
	// Black background
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f );

	// Set drawing color to red
	glColor3f(1.0f, 0.0f, 0.0f);
	
	// Enable polygon stippling
	glEnable(GL_POLYGON_STIPPLE);
	
	// Specify a specific stipple pattern
	glPolygonStipple(fire);
	}

void SpecialKeys(int key, int x, int y)
	{
	if(key == GLUT_KEY_UP)
		xRot-= 5.0f;

	if(key == GLUT_KEY_DOWN)
		xRot += 5.0f;

	if(key == GLUT_KEY_LEFT)
		yRot -= 5.0f;

	if(key == GLUT_KEY_RIGHT)
		yRot += 5.0f;

	if(key > 356.0f)
		xRot = 0.0f;

	if(key < -1.0f)
		xRot = 355.0f;

	if(key > 356.0f)
		yRot = 0.0f;

	if(key < -1.0f)
		yRot = 355.0f;

	// Refresh the Window
	glutPostRedisplay();
	}


void ChangeSize(int w, int h)
	{
	GLfloat nRange = 100.0f;

	// Prevent a divide by zero
	if(h == 0)
		h = 1;

	// Set Viewport to window dimensions
    glViewport(0, 0, w, h);

	// Reset projection matrix stack
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Establish clipping volume (left, right, bottom, top, near, far)
    if (w <= h) 
		glOrtho (-nRange, nRange, -nRange*h/w, nRange*h/w, -nRange, nRange);
    else 
		glOrtho (-nRange*w/h, nRange*w/h, -nRange, nRange, -nRange, nRange);

	// Reset Model view matrix stack
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	}

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("Polygon Stippling");
	glutReshapeFunc(ChangeSize);
	glutSpecialFunc(SpecialKeys);
	glutDisplayFunc(RenderScene);
	SetupRC();
	glutMainLoop();

	return 0;
	}
