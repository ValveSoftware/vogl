// Star.c
// OpenGL SuperBible, Chapter 4
// Demonstrates OpenGL Primative GL_POINTS with point size
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include <math.h>


// Define a constant for the value of PI
#define GL_PI 3.1415f

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;

// Flags for effects
#define MODE_SOLID 0
#define MODE_LINE  1
#define MODE_POINT 2

int iMode = MODE_SOLID;
GLboolean bEdgeFlag = GL_TRUE;

///////////////////////////////////////////////////////////////////////////////
// Reset flags as appropriate in response to menu selections
void ProcessMenu(int value)
	{
	switch(value)
		{
		case 1:
			iMode = MODE_SOLID;
			break;

		case 2:
			iMode = MODE_LINE;
			break;

		case 3:
			iMode = MODE_POINT;
			break;

		case 4:
			bEdgeFlag = GL_TRUE;
			break;

		case 5:
		default:
			bEdgeFlag = GL_FALSE;
			break;
		}

	glutPostRedisplay();
	}


// Called to draw scene
void RenderScene(void)
	{
	// Clear the window
	glClear(GL_COLOR_BUFFER_BIT);


	// Draw back side as a polygon only, if flag is set
	if(iMode == MODE_LINE)
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);

	if(iMode == MODE_POINT)
		glPolygonMode(GL_FRONT_AND_BACK,GL_POINT);

	if(iMode == MODE_SOLID)
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);


	// Save matrix state and do the rotation
	glPushMatrix();
	glRotatef(xRot, 1.0f, 0.0f, 0.0f);
	glRotatef(yRot, 0.0f, 1.0f, 0.0f);


	// Begin the triangles
	glBegin(GL_TRIANGLES);

		glEdgeFlag(bEdgeFlag);
		glVertex2f(-20.0f, 0.0f);
		glEdgeFlag(GL_TRUE);
		glVertex2f(20.0f, 0.0f);
		glVertex2f(0.0f, 40.0f);

		glVertex2f(-20.0f,0.0f);
		glVertex2f(-60.0f,-20.0f);
		glEdgeFlag(bEdgeFlag);
		glVertex2f(-20.0f,-40.0f);
		glEdgeFlag(GL_TRUE);

		glVertex2f(-20.0f,-40.0f);
		glVertex2f(0.0f, -80.0f);
		glEdgeFlag(bEdgeFlag);
		glVertex2f(20.0f, -40.0f);
		glEdgeFlag(GL_TRUE);

		glVertex2f(20.0f, -40.0f);
		glVertex2f(60.0f, -20.0f);
		glEdgeFlag(bEdgeFlag);
		glVertex2f(20.0f, 0.0f);
		glEdgeFlag(GL_TRUE);

		// Center square as two triangles
		glEdgeFlag(bEdgeFlag);
		glVertex2f(-20.0f, 0.0f);
		glVertex2f(-20.0f,-40.0f);
		glVertex2f(20.0f, 0.0f);

		glVertex2f(-20.0f,-40.0f);
		glVertex2f(20.0f, -40.0f);
		glVertex2f(20.0f, 0.0f);
		glEdgeFlag(GL_TRUE);

	// Done drawing Triangles
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

	// Set drawing color to green
	glColor3f(0.0f, 1.0f, 0.0f);
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
	int nModeMenu;
	int nEdgeMenu;
	int nMainMenu;

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("Solid and Outlined Star");
	
	// Create the Menu
	nModeMenu = glutCreateMenu(ProcessMenu);
	glutAddMenuEntry("Solid",1);
	glutAddMenuEntry("Outline",2);
	glutAddMenuEntry("Points",3);

	nEdgeMenu = glutCreateMenu(ProcessMenu);
	glutAddMenuEntry("On",4);
	glutAddMenuEntry("Off",5);

	nMainMenu = glutCreateMenu(ProcessMenu);
	glutAddSubMenu("Mode", nModeMenu);
	glutAddSubMenu("Edges", nEdgeMenu);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
	
	glutReshapeFunc(ChangeSize);
	glutSpecialFunc(SpecialKeys);
	glutDisplayFunc(RenderScene);
	SetupRC();
	glutMainLoop();

	return 0;
	}
