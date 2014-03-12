// Triangle.c
// OpenGL SuperBible
// Demonstrates OpenGL color triangle
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff


// Called to draw scene
void RenderScene(void)
	{
	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT);

	// Enable smooth shading
	glShadeModel(GL_SMOOTH);

	// Draw the triangle
	glBegin(GL_TRIANGLES);
		// Red Apex
		glColor3ub((GLubyte)255,(GLubyte)0,(GLubyte)0);
		glVertex3f(0.0f,200.0f,0.0f);

		// Green on the right bottom corner
		glColor3ub((GLubyte)0,(GLubyte)255,(GLubyte)0);
		glVertex3f(200.0f,-70.0f,0.0f);

		// Blue on the left bottom corner
		glColor3ub((GLubyte)0,(GLubyte)0,(GLubyte)255);
		glVertex3f(-200.0f, -70.0f, 0.0f);
	glEnd();

	// Flush drawing commands
	glutSwapBuffers();
	}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
	{
	// Black background
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
	}


void ChangeSize(int w, int h)
	{
	GLfloat windowHeight,windowWidth;
	
	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0)
		h = 1;

	// Set the viewport to be the entire window
    glViewport(0, 0, w, h);

	// Reset the coordinate system before modifying
    glLoadIdentity();


	// Keep the square square.

	// Window is higher than wide
	if (w <= h) 
		{
		windowHeight = 250.0f*h/w;
		windowWidth = 250.0f;
		}
    else 
		{
		// Window is wider than high
		windowWidth = 250.0f*w/h;
		windowHeight = 250.0f;
		}

	// Set the clipping volume
	glOrtho(-windowWidth, windowWidth, -windowHeight, windowHeight, 1.0f, -1.0f);
	}

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(800,600);
	glutCreateWindow("RGB Triangle");
	glutReshapeFunc(ChangeSize);
	glutDisplayFunc(RenderScene);
	SetupRC();
	glutMainLoop();

	return 0;
	}
