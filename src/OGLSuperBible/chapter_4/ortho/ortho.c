// Ortho.c
// OpenGL SuperBible
// Richard S. Wright Jr.
// Demonstrates Orthographic Projection

#include "../../common/openglsb.h"	// System and OpenGL Stuff

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;


// Change viewing volume and viewport.  Called when window is resized
void ChangeSize(GLsizei w, GLsizei h)
    {
    GLfloat nRange = 120.0f;
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
        glOrtho (-nRange, nRange, -nRange*h/w, nRange*h/w, -nRange*2.0f, nRange*2.0f);
    else 
        glOrtho (-nRange*w/h, nRange*w/h, -nRange, nRange, -nRange*2.0f, nRange*2.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    }



// This function does any needed initialization on the rendering
// context.  Here it sets up and initializes the lighting for
// the scene.
void SetupRC()
    {
    // Light values and coordinates
    GLfloat  whiteLight[] = { 0.45f, 0.45f, 0.45f, 1.0f };
    GLfloat  sourceLight[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat	 lightPos[] = { -50.f, 25.0f, 250.0f, 0.0f };

    glEnable(GL_DEPTH_TEST);	// Hidden surface removal
    glFrontFace(GL_CCW);		// Counter clock-wise polygons face out
    glEnable(GL_CULL_FACE);		// Do not calculate inside of jet

    // Enable lighting
    glEnable(GL_LIGHTING);

    // Setup and enable light 0
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,whiteLight);
    glLightfv(GL_LIGHT0,GL_AMBIENT,sourceLight);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,sourceLight);
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
    glEnable(GL_LIGHT0);

    // Enable color tracking
    glEnable(GL_COLOR_MATERIAL);
	
    // Set Material properties to follow glColor values
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // Black blue background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    }

// Respond to arrow keys
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
                
        xRot = (GLfloat)((const int)xRot % 360);
        yRot = (GLfloat)((const int)yRot % 360);

	// Refresh the Window
	glutPostRedisplay();
	}


// Called to draw scene
void RenderScene(void)
    {
    float fZ,bZ;

    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    fZ = 100.0f;
    bZ = -100.0f;

    // Save the matrix state and do the rotations
    glPushMatrix();
    glRotatef(xRot, 1.0f, 0.0f, 0.0f);
    glRotatef(yRot, 0.0f, 1.0f, 0.0f);

    // Set material color, Red
    glColor3f(1.0f, 0.0f, 0.0f);

    // Front Face ///////////////////////////////////
    glBegin(GL_QUADS);
        // Pointing straight out Z
        glNormal3f(0.0f, 0.0f, 1.0f);	

        // Left Panel
        glVertex3f(-50.0f, 50.0f, fZ);
        glVertex3f(-50.0f, -50.0f, fZ);
        glVertex3f(-35.0f, -50.0f, fZ);
        glVertex3f(-35.0f,50.0f,fZ);

        // Right Panel
        glVertex3f(50.0f, 50.0f, fZ);
        glVertex3f(35.0f, 50.0f, fZ);
        glVertex3f(35.0f, -50.0f, fZ);
        glVertex3f(50.0f,-50.0f,fZ);

        // Top Panel
        glVertex3f(-35.0f, 50.0f, fZ);
        glVertex3f(-35.0f, 35.0f, fZ);
        glVertex3f(35.0f, 35.0f, fZ);
        glVertex3f(35.0f, 50.0f,fZ);

        // Bottom Panel
        glVertex3f(-35.0f, -35.0f, fZ);
        glVertex3f(-35.0f, -50.0f, fZ);
        glVertex3f(35.0f, -50.0f, fZ);
        glVertex3f(35.0f, -35.0f,fZ);

        // Top length section ////////////////////////////
        // Normal points up Y axis
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-50.0f, 50.0f, fZ);
        glVertex3f(50.0f, 50.0f, fZ);
        glVertex3f(50.0f, 50.0f, bZ);
        glVertex3f(-50.0f,50.0f,bZ);
		
        // Bottom section
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(-50.0f, -50.0f, fZ);
        glVertex3f(-50.0f, -50.0f, bZ);
        glVertex3f(50.0f, -50.0f, bZ);
        glVertex3f(50.0f, -50.0f, fZ);

        // Left section
        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3f(50.0f, 50.0f, fZ);
        glVertex3f(50.0f, -50.0f, fZ);
        glVertex3f(50.0f, -50.0f, bZ);
        glVertex3f(50.0f, 50.0f, bZ);

        // Right Section
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-50.0f, 50.0f, fZ);
        glVertex3f(-50.0f, 50.0f, bZ);
        glVertex3f(-50.0f, -50.0f, bZ);
        glVertex3f(-50.0f, -50.0f, fZ);
    glEnd();

    glFrontFace(GL_CW);		// clock-wise polygons face out

    glBegin(GL_QUADS);
        // Back section
        // Pointing straight out Z
        glNormal3f(0.0f, 0.0f, -1.0f);	

        // Left Panel
        glVertex3f(-50.0f, 50.0f, bZ);
        glVertex3f(-50.0f, -50.0f, bZ);
        glVertex3f(-35.0f, -50.0f, bZ);
        glVertex3f(-35.0f,50.0f,bZ);

        // Right Panel
        glVertex3f(50.0f, 50.0f, bZ);
        glVertex3f(35.0f, 50.0f, bZ);
        glVertex3f(35.0f, -50.0f, bZ);
        glVertex3f(50.0f,-50.0f,bZ);

        // Top Panel
        glVertex3f(-35.0f, 50.0f, bZ);
        glVertex3f(-35.0f, 35.0f, bZ);
        glVertex3f(35.0f, 35.0f, bZ);
        glVertex3f(35.0f, 50.0f,bZ);

        // Bottom Panel
        glVertex3f(-35.0f, -35.0f, bZ);
        glVertex3f(-35.0f, -50.0f, bZ);
        glVertex3f(35.0f, -50.0f, bZ);
        glVertex3f(35.0f, -35.0f,bZ);
	
        // Insides /////////////////////////////
    	glColor3f(0.75f, 0.75f, 0.75f);

        // Normal points up Y axis
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-35.0f, 35.0f, fZ);
        glVertex3f(35.0f, 35.0f, fZ);
        glVertex3f(35.0f, 35.0f, bZ);
        glVertex3f(-35.0f,35.0f,bZ);
		
        // Bottom section
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-35.0f, -35.0f, fZ);
        glVertex3f(-35.0f, -35.0f, bZ);
        glVertex3f(35.0f, -35.0f, bZ);
        glVertex3f(35.0f, -35.0f, fZ);

        // Left section
        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3f(-35.0f, 35.0f, fZ);
        glVertex3f(-35.0f, 35.0f, bZ);
        glVertex3f(-35.0f, -35.0f, bZ);
        glVertex3f(-35.0f, -35.0f, fZ);

        // Right Section
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(35.0f, 35.0f, fZ);
        glVertex3f(35.0f, -35.0f, fZ);
        glVertex3f(35.0f, -35.0f, bZ);
        glVertex3f(35.0f, 35.0f, bZ);
    glEnd();

    glFrontFace(GL_CCW);		// Counter clock-wise polygons face out

    // Restore the matrix state
    glPopMatrix();

    // Buffer swap
    glutSwapBuffers();
    }



int main(int argc, char *argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Orthographic Projection");
    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
    SetupRC();
    glutMainLoop();
    
    return 0;
    }
    
    
    
