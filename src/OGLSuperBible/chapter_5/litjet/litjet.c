// LitJet.c
// OpenGL SuperBible
// Demonstrates OpenGL Lighting
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;


// Called to draw scene
void RenderScene(void)
	{
	GLTVector3 vNormal;	// Storeage for calculated surface normal

	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Save the matrix state and do the rotations
	glPushMatrix();
	glRotatef(xRot, 1.0f, 0.0f, 0.0f);
	glRotatef(yRot, 0.0f, 1.0f, 0.0f);


	// Nose Cone - Points straight down
        // Set material color
	glColor3ub(128, 128, 128);
	glBegin(GL_TRIANGLES);
                glNormal3f(0.0f, -1.0f, 0.0f);
		glNormal3f(0.0f, -1.0f, 0.0f);
		glVertex3f(0.0f, 0.0f, 60.0f);
		glVertex3f(-15.0f, 0.0f, 30.0f);
		glVertex3f(15.0f,0.0f,30.0f);
                
	
                // Verticies for this panel
                {
                GLTVector3 vPoints[3] = {{ 15.0f, 0.0f,  30.0f},
                                        { 0.0f,  15.0f, 30.0f},
                                        { 0.0f,  0.0f,  60.0f}};

                // Calculate the normal for the plane
                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
		glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }	


                {
                GLTVector3 vPoints[3] = {{ 0.0f, 0.0f, 60.0f },
                                        { 0.0f, 15.0f, 30.0f },
                                        { -15.0f, 0.0f, 30.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }


                // Body of the Plane ////////////////////////
                {
                GLTVector3 vPoints[3] = {{ -15.0f, 0.0f, 30.0f },
                                        { 0.0f, 15.0f, 30.0f },
				        { 0.0f, 0.0f, -56.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                	
                {
                GLTVector3 vPoints[3] = {{ 0.0f, 0.0f, -56.0f },
                                        { 0.0f, 15.0f, 30.0f },
                                        { 15.0f,0.0f,30.0f }};
	
                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                		
    
		glNormal3f(0.0f, -1.0f, 0.0f);
		glVertex3f(15.0f,0.0f,30.0f);
		glVertex3f(-15.0f, 0.0f, 30.0f);
		glVertex3f(0.0f, 0.0f, -56.0f);
    
                ///////////////////////////////////////////////
                // Left wing
                // Large triangle for bottom of wing
                {
                GLTVector3 vPoints[3] = {{ 0.0f,2.0f,27.0f },
                                        { -60.0f, 2.0f, -8.0f },
                                        { 60.0f, 2.0f, -8.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                        	
                {
                GLTVector3 vPoints[3] = {{ 60.0f, 2.0f, -8.0f},
					{0.0f, 7.0f, -8.0f},
					{0.0f,2.0f,27.0f }};
                
                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                {
                GLTVector3 vPoints[3] = {{60.0f, 2.0f, -8.0f},
					{-60.0f, 2.0f, -8.0f},
					{0.0f,7.0f,-8.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                {
                GLTVector3 vPoints[3] = {{0.0f,2.0f,27.0f},
                                        {0.0f, 7.0f, -8.0f},
                                        {-60.0f, 2.0f, -8.0f}};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                        
                // Tail section///////////////////////////////
                // Bottom of back fin
		glNormal3f(0.0f, -1.0f, 0.0f);
		glVertex3f(-30.0f, -0.50f, -57.0f);
		glVertex3f(30.0f, -0.50f, -57.0f);
		glVertex3f(0.0f,-0.50f,-40.0f);

                {
                GLTVector3 vPoints[3] = {{ 0.0f,-0.5f,-40.0f },
					{30.0f, -0.5f, -57.0f},
					{0.0f, 4.0f, -57.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                        
                {
                GLTVector3 vPoints[3] = {{ 0.0f, 4.0f, -57.0f },
					{ -30.0f, -0.5f, -57.0f },
					{ 0.0f,-0.5f,-40.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }


                {
                GLTVector3 vPoints[3] = {{ 30.0f,-0.5f,-57.0f },
					{ -30.0f, -0.5f, -57.0f },
					{ 0.0f, 4.0f, -57.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                {
                GLTVector3 vPoints[3] = {{ 0.0f,0.5f,-40.0f },
					{ 3.0f, 0.5f, -57.0f },
					{ 0.0f, 25.0f, -65.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                        
                {
                GLTVector3 vPoints[3] = {{ 0.0f, 25.0f, -65.0f },
					{ -3.0f, 0.5f, -57.0f},
					{ 0.0f,0.5f,-40.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                {
                GLTVector3 vPoints[3] = {{ 3.0f,0.5f,-57.0f },
					{ -3.0f, 0.5f, -57.0f },
					{ 0.0f, 25.0f, -65.0f }};

                gltGetNormalVector(vPoints[0], vPoints[1], vPoints[2], vNormal);
                glNormal3fv(vNormal);
		glVertex3fv(vPoints[0]);
		glVertex3fv(vPoints[1]);
		glVertex3fv(vPoints[2]);
                }
                
                
        glEnd();
                
    	// Restore the matrix state
	glPopMatrix();
	// Display the results
	glutSwapBuffers();
	}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    // Light values and coordinates
    GLfloat  ambientLight[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat  diffuseLight[] = { 0.7f, 0.7f, 0.7f, 1.0f };

    glEnable(GL_DEPTH_TEST);	// Hidden surface removal
    glFrontFace(GL_CCW);		// Counter clock-wise polygons face out
    glEnable(GL_CULL_FACE);		// Do not calculate inside of jet

    // Enable lighting
    glEnable(GL_LIGHTING);

    // Setup and enable light 0
    glLightfv(GL_LIGHT0,GL_AMBIENT,ambientLight);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuseLight);
    glEnable(GL_LIGHT0);

    // Enable color tracking
    glEnable(GL_COLOR_MATERIAL);
	
    // Set Material properties to follow glColor values
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // Light blue background
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f );
    }

/////////////////////////////////////////////////////
// Handle arrow keys
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


//////////////////////////////////////////////////////////
// Reset projection and light position
void ChangeSize(int w, int h)
    {
    GLfloat fAspect;
    GLfloat lightPos[] = { -50.f, 50.0f, 100.0f, 1.0f };

    // Prevent a divide by zero
    if(h == 0)
        h = 1;

    // Set Viewport to window dimensions
    glViewport(0, 0, w, h);

    // Reset coordinate system
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    fAspect = (GLfloat) w / (GLfloat) h;
    gluPerspective(45.0f, fAspect, 1.0f, 225.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
    glTranslatef(0.0f, 0.0f, -150.0f);
    }

int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800,600);
    glutCreateWindow("Lighted Jet");
    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
    SetupRC();
    glutMainLoop();

    return 0;
    }
