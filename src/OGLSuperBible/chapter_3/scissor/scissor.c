// Scissor.c
// OpenGL SuperBible, 3rd Edition
// Richard S. Wright Jr.
// rwright@starstonesoftware.com

#include "../../common/openglsb.h"	// System and OpenGL Stuff
	
///////////////////////////////////////////////////////////
// Called to draw scene
void RenderScene(void)
	{
        // Clear blue window
        glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Now set scissor to smaller red sub region
        glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
        glScissor(100, 100, 600, 400);
        glEnable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Finally, an even smaller green rectangle
        glClearColor(0.0f, 1.0f, 0.0f, 0.0f);
        glScissor(200, 200, 400, 200);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Turn scissor back off for next render
        glDisable(GL_SCISSOR_TEST);

	glutSwapBuffers();
	}


///////////////////////////////////////////////////////////
// Set viewport and projection
void ChangeSize(int w, int h)
	{
	// Prevent a divide by zero
	if(h == 0)
		h = 1;

	// Set Viewport to window dimensions
        glViewport(0, 0, w, h);


	// Set the perspective coordinate system
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Set 2D Coordinate system
	gluOrtho2D(-4.0, 4.0, -3.0, 3.0);

	// Modelview matrix reset
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	}


///////////////////////////////////////////////////////////
// Program entry point
int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(800,600);
	glutCreateWindow("OpenGL Scissor");
	glutReshapeFunc(ChangeSize);
	glutDisplayFunc(RenderScene);
	glutMainLoop();

	return 0;
	}
