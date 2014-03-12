// Jet.c
// A hand modeled Jet airplane
// OpenGL SuperBible
// Beginning of OpenGL lighting sample
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff


// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;


// Called to draw scene
void RenderScene(void)
    {
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Save matrix state and do the rotation
    glPushMatrix();
    glRotatef(xRot, 1.0f, 0.0f, 0.0f);
    glRotatef(yRot, 0.0f, 1.0f, 0.0f);


    // Nose Cone /////////////////////////////
    // White
    glColor3ub(255, 255, 255);
    glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, 0.0f, 60.0f);
        glVertex3f(-15.0f, 0.0f, 30.0f);
        glVertex3f(15.0f,0.0f,30.0f);

        // Black
        glColor3ub(0,0,0);
        glVertex3f(15.0f,0.0f,30.0f);
        glVertex3f(0.0f, 15.0f, 30.0f);
        glVertex3f(0.0f, 0.0f, 60.0f);
	
        // Red
        glColor3ub(255,0,0);
        glVertex3f(0.0f, 0.0f, 60.0f);
        glVertex3f(0.0f, 15.0f, 30.0f);
        glVertex3f(-15.0f,0.0f,30.0f);


	// Body of the Plane ////////////////////////
        // Green
        glColor3ub(0,255,0);
        glVertex3f(-15.0f,0.0f,30.0f);
        glVertex3f(0.0f, 15.0f, 30.0f);
        glVertex3f(0.0f, 0.0f, -56.0f);
		
        glColor3ub(255,255,0);
        glVertex3f(0.0f, 0.0f, -56.0f);
        glVertex3f(0.0f, 15.0f, 30.0f);
        glVertex3f(15.0f,0.0f,30.0f);	
	
        glColor3ub(0, 255, 255);
        glVertex3f(15.0f,0.0f,30.0f);
        glVertex3f(-15.0f, 0.0f, 30.0f);
        glVertex3f(0.0f, 0.0f, -56.0f);
	
	///////////////////////////////////////////////
	// Left wing
	// Large triangle for bottom of wing
        glColor3ub(128,128,128);
        glVertex3f(0.0f,2.0f,27.0f);
        glVertex3f(-60.0f, 2.0f, -8.0f);
        glVertex3f(60.0f, 2.0f, -8.0f);
	
        glColor3ub(64,64,64);
        glVertex3f(60.0f, 2.0f, -8.0f);
        glVertex3f(0.0f, 7.0f, -8.0f);
        glVertex3f(0.0f,2.0f,27.0f);
	
        glColor3ub(192,192,192);
        glVertex3f(60.0f, 2.0f, -8.0f);
        glVertex3f(-60.0f, 2.0f, -8.0f);
        glVertex3f(0.0f,7.0f,-8.0f);
	
	// Other wing top section
        glColor3ub(64,64,64);
        glVertex3f(0.0f,2.0f,27.0f);
        glVertex3f(0.0f, 7.0f, -8.0f);
        glVertex3f(-60.0f, 2.0f, -8.0f);
	
	// Tail section///////////////////////////////
	// Bottom of back fin
        glColor3ub(255,128,255);
        glVertex3f(-30.0f, -0.50f, -57.0f);
        glVertex3f(30.0f, -0.50f, -57.0f);
        glVertex3f(0.0f,-0.50f,-40.0f);
	
        // top of left side
        glColor3ub(255,128,0);
        glVertex3f(0.0f,-0.5f,-40.0f);
        glVertex3f(30.0f, -0.5f, -57.0f);
        glVertex3f(0.0f, 4.0f, -57.0f);
	
        // top of right side
        glColor3ub(255,128,0);
        glVertex3f(0.0f, 4.0f, -57.0f);
        glVertex3f(-30.0f, -0.5f, -57.0f);
        glVertex3f(0.0f,-0.5f,-40.0f);
	
        // back of bottom of tail
        glColor3ub(255,255,255);
        glVertex3f(30.0f,-0.5f,-57.0f);
        glVertex3f(-30.0f, -0.5f, -57.0f);
        glVertex3f(0.0f, 4.0f, -57.0f);
	
        // Top of Tail section left
        glColor3ub(255,0,0);
        glVertex3f(0.0f,0.5f,-40.0f);
        glVertex3f(3.0f, 0.5f, -57.0f);
        glVertex3f(0.0f, 25.0f, -65.0f);
	
        glColor3ub(255,0,0);
        glVertex3f(0.0f, 25.0f, -65.0f);
        glVertex3f(-3.0f, 0.5f, -57.0f);
        glVertex3f(0.0f,0.5f,-40.0f);

        // Back of horizontal section
        glColor3ub(128,128,128);
        glVertex3f(3.0f,0.5f,-57.0f);
        glVertex3f(-3.0f, 0.5f, -57.0f);
        glVertex3f(0.0f, 25.0f, -65.0f);
	
    glEnd(); // Of Jet

    glPopMatrix();

    // Display the results
    glutSwapBuffers();
    }

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    glEnable(GL_DEPTH_TEST);	// Hidden surface removal
    glEnable(GL_CULL_FACE);		// Do not calculate inside of jet
    glFrontFace(GL_CCW);		// Counter clock-wise polygons face out

    // Nice light blue
    glClearColor(0.0f, 0.0f, 05.f,1.0f);
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
    GLfloat nRange = 80.0f;
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
        glOrtho (-nRange, nRange, -nRange*h/w, nRange*h/w, -nRange, nRange);
    else 
        glOrtho (-nRange*w/h, nRange*w/h, -nRange, nRange, -nRange, nRange);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    }

int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800,600);
    glutCreateWindow("Jet");
    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
    SetupRC();
    glutMainLoop();

    return 0;
    }
