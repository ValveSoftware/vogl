// Atom.c
// OpenGL SuperBible
// Demonstrates OpenGL coordinate transformation
// Program by Richard S. Wright Jr.
#include "../../common/openglsb.h"	// System and OpenGL Stuff

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;


// Called to draw scene
void RenderScene(void)
	{
	// Angle of revolution around the nucleus
	static GLfloat fElect1 = 0.0f;

	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Reset the modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Translate the whole scene out and into view	
	// This is the initial viewing transformation
	glTranslatef(0.0f, 0.0f, -100.0f);	

	// Red Nucleus
	glColor3ub(255, 0, 0);
	glutSolidSphere(10.0f, 15, 15);

	// Yellow Electrons
	glColor3ub(255,255,0);

	// First Electron Orbit
	// Save viewing transformation
	glPushMatrix();

	// Rotate by angle of revolution
	glRotatef(fElect1, 0.0f, 1.0f, 0.0f);

	// Translate out from origin to orbit distance
	glTranslatef(90.0f, 0.0f, 0.0f);

	// Draw the electron
	glutSolidSphere(6.0f, 15, 15);
    

	// Restore the viewing transformation
	glPopMatrix();

	// Second Electron Orbit
	glPushMatrix();
	glRotatef(45.0f, 0.0f, 0.0f, 1.0f);
	glRotatef(fElect1, 0.0f, 1.0f, 0.0f);
	glTranslatef(-70.0f, 0.0f, 0.0f);
	glutSolidSphere(6.0f, 15, 15);
	glPopMatrix();


	// Third Electron Orbit
	glPushMatrix();
	glRotatef(360.0f-45.0f,0.0f, 0.0f, 1.0f);
	glRotatef(fElect1, 0.0f, 1.0f, 0.0f);
	glTranslatef(0.0f, 0.0f, 60.0f);
	glutSolidSphere(6.0f, 15, 15);
	glPopMatrix();


	// Increment the angle of revolution
	fElect1 += 10.0f;
	if(fElect1 > 360.0f)
		fElect1 = 0.0f;

	// Show the image
	glutSwapBuffers();
	}


// This function does any needed initialization on the rendering
// context. 
void SetupRC()
	{
	glEnable(GL_DEPTH_TEST);	// Hidden surface removal
	glFrontFace(GL_CCW);		// Counter clock-wise polygons face out
	glEnable(GL_CULL_FACE);		// Do not calculate inside of jet

	// Black background
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f );	
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

void TimerFunc(int value)
    {
    glutPostRedisplay();
    glutTimerFunc(100, TimerFunc, 1);
    }


void ChangeSize(int w, int h)
    {
    GLfloat nRange = 100.0f;

    // Prevent a divide by zero
    if(h == 0)
        h = 1;

    // Set Viewport to window dimensions
    glViewport(0, 0, w, h);


    // Reset coordinate system
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Establish clipping volume (left, right, bottom, top, near, far)
    if (w <= h) 
        glOrtho (-nRange, nRange, nRange*h/w, -nRange*h/w, -nRange*2.0f, nRange*2.0f);
    else 
        glOrtho (-nRange*w/h, nRange*w/h, nRange, -nRange, -nRange*2.0f, nRange*2.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    }

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(800, 600);
	glutCreateWindow("OpenGL Atom");
	glutReshapeFunc(ChangeSize);
	glutSpecialFunc(SpecialKeys);
	glutDisplayFunc(RenderScene);
        glutTimerFunc(500, TimerFunc, 1);
	SetupRC();
	glutMainLoop();

	return 0;
	}
