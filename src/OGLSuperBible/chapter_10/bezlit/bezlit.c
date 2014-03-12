// Bezlit.c
// OpenGL SuperBible
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit

// The number of control points for this curve
GLint nNumPoints = 3;

GLfloat ctrlPoints[3][3][3]= {{{  -4.0f, 0.0f, 4.0f},	
						      { -2.0f, 4.0f, 4.0f},	
							  {  4.0f, 0.0f, 4.0f }},
							 
							  {{  -4.0f, 0.0f, 0.0f},	
						      { -2.0f, 4.0f, 0.0f},	
							  {  4.0f, 0.0f, 0.0f }},
							  
							  {{  -4.0f, 0.0f, -4.0f},	
						      { -2.0f, 4.0f, -4.0f},	
							  {  4.0f, 0.0f, -4.0f }}};


// This function is used to superimpose the control points over the curve
void DrawPoints(void)
	{
	int i,j;	// Counting variables

	// Set point size larger to make more visible
	glPointSize(5.0f);

	// Loop through all control points for this example
	glBegin(GL_POINTS);
		for(i = 0; i < nNumPoints; i++)
			for(j = 0; j < 3; j++)
				glVertex3fv(ctrlPoints[i][j]);
	glEnd();
	}



// Called to draw scene
void RenderScene(void)
	{
	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Save the modelview matrix stack
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	// Rotate the mesh around to make it easier to see
	glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
	glRotatef(60.0f, 1.0f, 0.0f, 0.0f);


	// Sets up the bezier
	// This actually only needs to be called once and could go in
	// the setup function
	glMap2f(GL_MAP2_VERTEX_3,	// Type of data generated
	0.0f,						// Lower u range
	10.0f,						// Upper u range
	3,							// Distance between points in the data
	3,							// Dimension in u direction (order)
	0.0f,						// Lover v range
	10.0f,						// Upper v range
	9,							// Distance between points in the data
	3,							// Dimension in v direction (order)
	&ctrlPoints[0][0][0]);		// array of control points

	// Enable the evaluator
	glEnable(GL_MAP2_VERTEX_3);

	// Use higher level functions to map to a grid, then evaluate the
	// entire thing.

	// Map a grid of 10 points from 0 to 10
	glMapGrid2f(10,0.0f,10.0f,10,0.0f,10.0f);

	// Evaluate the grid, using lines
	glEvalMesh2(GL_FILL,0,10,0,10);

	// Restore the modelview matrix
	glPopMatrix();


	// Dispalay the image
	glutSwapBuffers();
	}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
	{
	// Light values and coordinates
	GLfloat  ambientLight[] = { 0.3f, 0.3f, 0.3f, 1.0f };
	GLfloat  diffuseLight[] = { 0.7f, 0.7f, 0.7f, 1.0f };
	GLfloat	 lightPos[] = { 20.0f, 0.0f, 0.0f, 0.0f };

	// Clear Window to white
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f );

	glEnable(GL_DEPTH_TEST);	// Hidden surface removal

	// Enable lighting
	glEnable(GL_LIGHTING);

	// Setup light 0
	glLightfv(GL_LIGHT0,GL_AMBIENT,ambientLight);
	glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuseLight);

	// Position and turn on the light
	glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
	glEnable(GL_LIGHT0);

	// Enable color tracking
	glEnable(GL_COLOR_MATERIAL);
	
	// Set Material properties to follow glColor values
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	// Automatically generate normals for evaluated surfaces
	glEnable(GL_AUTO_NORMAL);

	// Draw in Blue
	glColor3f(0.0f, 0.0f, 1.0f);
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

	glOrtho(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);

	// Modelview matrix reset
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	}

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
	glutCreateWindow("Lit 3D Bezier Surface");
	glutReshapeFunc(ChangeSize);
	glutDisplayFunc(RenderScene);
	SetupRC();
	glutMainLoop();

	return 0;
	}
