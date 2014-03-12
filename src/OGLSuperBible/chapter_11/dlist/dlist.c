// dlist.c
// OpenGL SuperBible
// Demonstrates rendering with and without display lists
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>

GLuint dlShip;          // Display list identifier for the ship

// Function in ship.c
void DrawShip(void);
        
// Called to draw scene
void RenderScene(void)
    {
    static GLfloat yRot = 0.0f;
    yRot += 0.5f;
        
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();
        
    glTranslatef(0.0f, 0.0f, -15.f);
    glRotatef(-70.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(yRot, 0.0f, 0.0f, 1.0f);

    //DrawShip();
    glCallList(dlShip);
    
    glPopMatrix();
    
    // Do the buffer Swap
    glutSwapBuffers();
    }


// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    // Bluish background
    glClearColor(0.0f, 0.0f, .50f, 1.0f );
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
        
    // Lit texture environment
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    
    // Load the ship texture
    GLint iWidth, iHeight,iComponents;
    GLenum eFormat;
    GLbyte *pBytes = gltLoadTGA("YellowSub.tga", &iWidth, &iHeight, &iComponents, &eFormat);    
    glTexImage2D(GL_TEXTURE_2D, 0, iComponents, iWidth, iHeight, 0, eFormat, GL_UNSIGNED_BYTE, (void *)pBytes);
    free(pBytes);
    
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glEnable(GL_TEXTURE_2D);

    // Set up lighting
    GLfloat fAmbLight[] =   { 0.1f, 0.1f, 0.1f };
    GLfloat fDiffLight[] =  { 1.0f, 1.0f, 1.0f };
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glLightfv(GL_LIGHT0, GL_AMBIENT, fAmbLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, fDiffLight);
    
    // Build a single display list
    dlShip = glGenLists(1);
    glNewList(dlShip, GL_COMPILE);
    DrawShip();
    glEndList();
    }

///////////////////////////////////////////////////////////
// Called by GLUT library when idle (window not being
// resized or moved)
void TimerFunction(int value)
    {
    // Redraw the scene with new coordinates
    glutPostRedisplay();
    glutTimerFunc(5,TimerFunction, 1);
    }



void ChangeSize(int w, int h)
	{
	GLfloat fAspect;

	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0)
            h = 1;

        glViewport(0, 0, w, h);
        
        fAspect = (GLfloat)w / (GLfloat)h;

	// Reset the coordinate system before modifying
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	// Set the clipping volume
	gluPerspective(35.0f, fAspect, 1.0f, 50.0f);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Light never changes, put it here
        GLfloat lightPos[] = { -10.0f, 100.0f, 20.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
        }

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(800,600);
	glutCreateWindow("Display List Demo");
	glutReshapeFunc(ChangeSize);
	glutDisplayFunc(RenderScene);

	SetupRC();
        glutTimerFunc(5, TimerFunction, 1);

	glutMainLoop();
        
        glDeleteLists(dlShip,1);

	return 0;
	}
