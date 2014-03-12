//Nurbc.c
// OpenGL SuperBible
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit


// NURBS object pointer
GLUnurbsObj *pNurb = 0;

// The number of control points for this curve
GLint nNumPoints = 4; // 4 X 4

// Mesh extends four units -6 to +6 along x and y axis
// Lies in Z plane
//                 u  v  (x,y,z)	
GLfloat ctrlPoints[4][3]  = {{  -6.0f, -6.0f, 0.0f},
                             {   2.0f, -2.0f, 8.0f},
                             {   2.0f,  6.0f, 0.0f},
                             {   6.0f, 6.0f,  0.0f}};


// Knot sequence for the NURB
GLfloat Knots[8] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};



// Called to draw the control points in Red over the NURB
void DrawPoints(void)
    {
    int i;

    // Large Red Points
    glPointSize(5.0f);
    glColor3ub(255,0,0);

    // Draw all the points in the array
    glBegin(GL_POINTS);
        for(i = 0; i < 4; i++)
            glVertex3fv(ctrlPoints[i]);	
    glEnd();
    }

// NURBS callback error handler
#if 0
void CALLBACK NurbsErrorHandler(GLenum nErrorCode)
    {
    char cMessage[64];

    // Extract a text message of the error
    strcpy(cMessage,"NURBS error occured: ");
    strcat(cMessage,gluErrorString(nErrorCode));

    // Display the message to the user
    glutSetWindowTitle(cMessage);
    }
#endif



// Called to draw scene
void RenderScene()
	{
	// Draw in Blue
	glColor3ub(0,0,220);

	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Save the modelview matrix stack
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	// Rotate the mesh around to make it easier to see
	glRotatef(330.0f, 1.0f,0.0f,0.0f);
	
	// Render the NURB
	// Begin the NURB definition
    gluBeginCurve(pNurb);
	
	// Evaluate the surface
    gluNurbsCurve(pNurb, 
        8, Knots,
		3,
        &ctrlPoints[0][0], 
        4,
        GL_MAP1_VERTEX_3);
    
	// Done with surface
	gluEndCurve(pNurb);
	
	// Show the control points
	DrawPoints();

	// Restore the modelview matrix
	glPopMatrix();

	// Dispalay the image
	glutSwapBuffers();
	}

// This function does any needed initialization on the rendering
// context.  Here it sets up and initializes the lighting for
// the scene.
void SetupRC()
	{
	// Clear Window to white
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f );

	// Setup the Nurbs object
    pNurb = gluNewNurbsRenderer();
    gluNurbsProperty(pNurb, GLU_SAMPLING_TOLERANCE, 25.0f);
	gluNurbsProperty(pNurb, GLU_DISPLAY_MODE, (GLfloat)GLU_FILL);
	}


void ChangeSize(int w, int h)
	{
	// Prevent a divide by zero
	if(h == 0)
		h = 1;

	// Set Viewport to window dimensions
    glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Perspective view
	gluPerspective (45.0f, (GLdouble)w/(GLdouble)h, 1.0, 40.0f);

	// Modelview matrix reset
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Viewing transformation, position for better view
    glTranslatef (0.0f, 0.0f, -20.0f);
	}

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(800, 600);
	glutCreateWindow("NURBS Curve");
	glutReshapeFunc(ChangeSize);
	glutDisplayFunc(RenderScene);
	SetupRC();
	glutMainLoop();

	return 0;
	}
