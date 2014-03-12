// Spot.c
// OpenGL SuperBible
// Demonstrates OpenGL Spotlight
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff


// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;

// Light values and coordinates
GLfloat	 lightPos[] = { 0.0f, 0.0f, 75.0f, 1.0f };
GLfloat  specular[] = { 1.0f, 1.0f, 1.0f, 1.0f};
GLfloat  specref[] =  { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat  ambientLight[] = { 0.5f, 0.5f, 0.5f, 1.0f};
GLfloat  spotDir[] = { 0.0f, 0.0f, -1.0f };

// Flags for effects
#define MODE_FLAT 1
#define MODE_SMOOTH  2
#define MODE_VERYLOW 3
#define MODE_MEDIUM  4
#define MODE_VERYHIGH 5

int iShade = MODE_FLAT;
int iTess = MODE_VERYLOW;

///////////////////////////////////////////////////////////////////////////////
// Reset flags as appropriate in response to menu selections
void ProcessMenu(int value)
    {
    switch(value)
        {
        case 1:
            iShade = MODE_FLAT;
            break;

        case 2:
            iShade = MODE_SMOOTH;
            break;

        case 3:
            iTess = MODE_VERYLOW;
            break;

        case 4:
            iTess = MODE_MEDIUM;
            break;

        case 5:
        default:
            iTess = MODE_VERYHIGH;
            break;
        }

    glutPostRedisplay();
    }



// Called to draw scene
void RenderScene(void)
    {
    if(iShade == MODE_FLAT)
        glShadeModel(GL_FLAT);
    else // 	iShade = MODE_SMOOTH;
        glShadeModel(GL_SMOOTH);

    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // First place the light 
    // Save the coordinate transformation
    glPushMatrix();	
        // Rotate coordinate system
        glRotatef(yRot, 0.0f, 1.0f, 0.0f);
        glRotatef(xRot, 1.0f, 0.0f, 0.0f);

        // Specify new position and direction in rotated coords.
        glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
        glLightfv(GL_LIGHT0,GL_SPOT_DIRECTION,spotDir);

        // Draw a red cone to enclose the light source
        glColor3ub(255,0,0);	

        // Translate origin to move the cone out to where the light
        // is positioned.
        glTranslatef(lightPos[0],lightPos[1],lightPos[2]);
        glutSolidCone(4.0f,6.0f,15,15);

        // Draw a smaller displaced sphere to denote the light bulb
        // Save the lighting state variables
        glPushAttrib(GL_LIGHTING_BIT);

            // Turn off lighting and specify a bright yellow sphere
            glDisable(GL_LIGHTING);
            glColor3ub(255,255,0);
            glutSolidSphere(3.0f, 15, 15);

        // Restore lighting state variables
        glPopAttrib();

    // Restore coordinate transformations
    glPopMatrix();


    // Set material color and draw a sphere in the middle
    glColor3ub(0, 0, 255);

    if(iTess == MODE_VERYLOW)
    	glutSolidSphere(30.0f, 7, 7);
    else 
        if(iTess == MODE_MEDIUM)
            glutSolidSphere(30.0f, 15, 15);
        else //  iTess = MODE_MEDIUM;
            glutSolidSphere(30.0f, 50, 50);

    // Display the results
    glutSwapBuffers();
    }

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    glEnable(GL_DEPTH_TEST);	// Hidden surface removal
    glFrontFace(GL_CCW);		// Counter clock-wise polygons face out
    glEnable(GL_CULL_FACE);		// Do not try to display the back sides

    // Enable lighting
    glEnable(GL_LIGHTING);

    // Setup and enable light 0
    // Supply a slight ambient light so the objects can be seen
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);
	
    // The light is composed of just a diffuse and specular components
    glLightfv(GL_LIGHT0,GL_DIFFUSE,ambientLight);
    glLightfv(GL_LIGHT0,GL_SPECULAR,specular);
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);

    // Specific spot effects
    // Cut off angle is 60 degrees
    glLightf(GL_LIGHT0,GL_SPOT_CUTOFF,50.0f);

    // Enable this light in particular
    glEnable(GL_LIGHT0);

    // Enable color tracking
    glEnable(GL_COLOR_MATERIAL);
	
    // Set Material properties to follow glColor values
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // All materials hereafter have full specular reflectivity
    // with a high shine
    glMaterialfv(GL_FRONT, GL_SPECULAR,specref);
    glMateriali(GL_FRONT, GL_SHININESS,128);


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


void ChangeSize(int w, int h)
    {
    GLfloat fAspect;
    
    // Prevent a divide by zero
    if(h == 0)
        h = 1;

    // Set Viewport to window dimensions
    glViewport(0, 0, w, h);

    // Reset coordinate system
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Establish viewing volume
    fAspect = (GLfloat) w / (GLfloat) h;
    gluPerspective(35.0f, fAspect, 1.0f, 500.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -250.0f);
    }

int main(int argc, char* argv[])
    {
    int nMenu;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Spot Light");
    
    // Create the Menu
    nMenu = glutCreateMenu(ProcessMenu);
    glutAddMenuEntry("Flat Shading",1);
    glutAddMenuEntry("Smooth Shading",2);
    glutAddMenuEntry("VL Tess",3);
    glutAddMenuEntry("MD Tess",4);
    glutAddMenuEntry("VH Tess",5);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
    SetupRC();
    glutMainLoop();

    return 0;
    }
