// Block.c
// OpenGL SuperBible, Chapter 1
// Demonstrates an assortment of basic 3D concepts
// Program by Richard S. Wright Jr.


#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>


// Keep track of effects step
int nStep = 0;


// Lighting data
GLfloat lightAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat lightDiffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
GLfloat lightSpecular[] = { 0.9f, 0.9f, 0.9f };
GLfloat materialColor[] = { 0.8f, 0.0f, 0.0f };
GLfloat lightpos[] = { -80.0f, 120.0f, 100.0f, 0.0f };
GLfloat ground[3][3] = { { 0.0f, -25.0f, 0.0f },
                        { 10.0f, -25.0f, 0.0f },
                        { 10.0f, -25.0f, -10.0f } };

GLuint textures[4];


// Reduces a normal vector specified as a set of three coordinates,
// to a unit normal vector of length one.
void ReduceToUnit(float vector[3])
	{
	float length;
	
	// Calculate the length of the vector		
	length = (float)sqrt((vector[0]*vector[0]) + 
						(vector[1]*vector[1]) +
						(vector[2]*vector[2]));

	// Keep the program from blowing up by providing an exceptable
	// value for vectors that may calculated too close to zero.
	if(length == 0.0f)
		length = 1.0f;

	// Dividing each element by the length will result in a
	// unit normal vector.
	vector[0] /= length;
	vector[1] /= length;
	vector[2] /= length;
	}


// Points p1, p2, & p3 specified in counter clock-wise order
void calcNormal(float v[3][3], float out[3])
	{
	float v1[3],v2[3];
	static const int x = 0;
	static const int y = 1;
	static const int z = 2;

	// Calculate two vectors from the three points
	v1[x] = v[0][x] - v[1][x];
	v1[y] = v[0][y] - v[1][y];
	v1[z] = v[0][z] - v[1][z];

	v2[x] = v[1][x] - v[2][x];
	v2[y] = v[1][y] - v[2][y];
	v2[z] = v[1][z] - v[2][z];

	// Take the cross product of the two vectors to get
	// the normal vector which will be stored in out
	out[x] = v1[y]*v2[z] - v1[z]*v2[y];
	out[y] = v1[z]*v2[x] - v1[x]*v2[z];
	out[z] = v1[x]*v2[y] - v1[y]*v2[x];

	// Normalize the vector (shorten length to one)
	ReduceToUnit(out);
	}


// Creates a shadow projection matrix out of the plane equation
// coefficients and the position of the light. The return value is stored
// in destMat[][]
void MakeShadowMatrix(GLfloat points[3][3], GLfloat lightPos[4], GLfloat destMat[4][4])
	{
	GLfloat planeCoeff[4];
	GLfloat dot;

	// Find the plane equation coefficients
	// Find the first three coefficients the same way we
	// find a normal.
	calcNormal(points,planeCoeff);

	// Find the last coefficient by back substitutions
	planeCoeff[3] = - (
		(planeCoeff[0]*points[2][0]) + (planeCoeff[1]*points[2][1]) +
		(planeCoeff[2]*points[2][2]));


	// Dot product of plane and light position
	dot = planeCoeff[0] * lightPos[0] +
			planeCoeff[1] * lightPos[1] +
			planeCoeff[2] * lightPos[2] +
			planeCoeff[3] * lightPos[3];

	// Now do the projection
	// First column
    destMat[0][0] = dot - lightPos[0] * planeCoeff[0];
    destMat[1][0] = 0.0f - lightPos[0] * planeCoeff[1];
    destMat[2][0] = 0.0f - lightPos[0] * planeCoeff[2];
    destMat[3][0] = 0.0f - lightPos[0] * planeCoeff[3];

	// Second column
	destMat[0][1] = 0.0f - lightPos[1] * planeCoeff[0];
	destMat[1][1] = dot - lightPos[1] * planeCoeff[1];
	destMat[2][1] = 0.0f - lightPos[1] * planeCoeff[2];
	destMat[3][1] = 0.0f - lightPos[1] * planeCoeff[3];

	// Third Column
	destMat[0][2] = 0.0f - lightPos[2] * planeCoeff[0];
	destMat[1][2] = 0.0f - lightPos[2] * planeCoeff[1];
	destMat[2][2] = dot - lightPos[2] * planeCoeff[2];
	destMat[3][2] = 0.0f - lightPos[2] * planeCoeff[3];

	// Fourth Column
	destMat[0][3] = 0.0f - lightPos[3] * planeCoeff[0];
	destMat[1][3] = 0.0f - lightPos[3] * planeCoeff[1];
	destMat[2][3] = 0.0f - lightPos[3] * planeCoeff[2];
	destMat[3][3] = dot - lightPos[3] * planeCoeff[3];
	}



// Called to draw scene
void RenderScene(void)
	{
	GLfloat cubeXform[4][4];

	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_NORMALIZE);

	glPushMatrix();

	// Draw plane that the cube rests on
	glDisable(GL_LIGHTING);
	if(nStep == 5)
		{
		glColor3ub(255,255,255);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, textures[0]);
		glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(-100.0f, -25.3f, -100.0f);
			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(-100.0f, -25.3f, 100.0f);		
			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(100.0f,  -25.3f, 100.0f);
			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(100.0f,  -25.3f, -100.0f);
		glEnd();
		}
	else
		{
		glColor3f(0.0f, 0.0f, 0.90f); // Blue
		glBegin(GL_QUADS);
			glVertex3f(-100.0f, -25.3f, -100.0f);
			glVertex3f(-100.0f, -25.3f, 100.0f);		
			glVertex3f(100.0f,  -25.3f, 100.0f);
			glVertex3f(100.0f,  -25.3f, -100.0f);
		glEnd();
		}


	// Set drawing color to Red
	glColor3f(1.0f, 0.0f, 0.0f);

	// Enable, disable lighting
	if(nStep > 2)
		{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_COLOR_MATERIAL);

		glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
		glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glMaterialfv(GL_FRONT, GL_SPECULAR,lightSpecular);
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, materialColor);
		glMateriali(GL_FRONT, GL_SHININESS,128);
		}

	// Move the cube slightly forward and to the left
	glTranslatef(-10.0f, 0.0f, 10.0f);

	switch(nStep)
		{
		// Just draw the wire framed cube
		case 0:
			glutWireCube(50.0f);
			break;

		// Same wire cube with hidden line removal simulated
		case 1:
			// Front Face (before rotation)
			glBegin(GL_LINES);
				glVertex3f(25.0f,25.0f,25.0f);
				glVertex3f(25.0f,-25.0f,25.0f);
				
				glVertex3f(25.0f,-25.0f,25.0f);
				glVertex3f(-25.0f,-25.0f,25.0f);

				glVertex3f(-25.0f,-25.0f,25.0f);
				glVertex3f(-25.0f,25.0f,25.0f);

				glVertex3f(-25.0f,25.0f,25.0f);
				glVertex3f(25.0f,25.0f,25.0f);
			glEnd();

			// Top of cube
			glBegin(GL_LINES);
				// Front Face
				glVertex3f(25.0f,25.0f,25.0f);
				glVertex3f(25.0f,25.0f,-25.0f);
				
				glVertex3f(25.0f,25.0f,-25.0f);
				glVertex3f(-25.0f,25.0f,-25.0f);

				glVertex3f(-25.0f,25.0f,-25.0f);
				glVertex3f(-25.0f,25.0f,25.0f);

				glVertex3f(-25.0f,25.0f,25.0f);
				glVertex3f(25.0f,25.0f,25.0f);
			glEnd();

			// Last two segments for effect
			glBegin(GL_LINES);
				glVertex3f(25.0f,25.0f,-25.0f);
				glVertex3f(25.0f,-25.0f,-25.0f);

				glVertex3f(25.0f,-25.0f,-25.0f);
				glVertex3f(25.0f,-25.0f,25.0f);
			glEnd();
		
			break;

		// Uniform colored surface, looks 2D and goofey
		case 2:
			glutSolidCube(50.0f);
			break;

		case 3:
			glutSolidCube(50.0f);
			break;

		// Draw a shadow with some lighting
		case 4:
			glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *) cubeXform);
			glutSolidCube(50.0f);
			glPopMatrix();

			// Disable lighting, we'll just draw the shadow as black
			glDisable(GL_LIGHTING);
			
			glPushMatrix();

			MakeShadowMatrix(ground, lightpos, cubeXform);
			glMultMatrixf((GLfloat *)cubeXform);
			
			glTranslatef(-10.0f, 0.0f, 10.0f);			
			
			// Set drawing color to Black
			glColor3f(0.0f, 0.0f, 0.0f);

			glutSolidCube(50.0f);
			break;

		case 5:
			glColor3ub(255,255,255);
			glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *) cubeXform);

			// Front Face (before rotation)
			glBindTexture(GL_TEXTURE_2D, textures[1]);
			glBegin(GL_QUADS);
				glTexCoord2f(1.0f, 1.0f);
				glVertex3f(25.0f,25.0f,25.0f);
				glTexCoord2f(1.0f, 0.0f);
				glVertex3f(25.0f,-25.0f,25.0f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(-25.0f,-25.0f,25.0f);
				glTexCoord2f(0.0f, 1.0f);
				glVertex3f(-25.0f,25.0f,25.0f);
			glEnd();

			// Top of cube
			glBindTexture(GL_TEXTURE_2D, textures[2]);
			glBegin(GL_QUADS);
				// Front Face
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(25.0f,25.0f,25.0f);
				glTexCoord2f(1.0f, 0.0f);
				glVertex3f(25.0f,25.0f,-25.0f);
				glTexCoord2f(1.0f, 1.0f);
				glVertex3f(-25.0f,25.0f,-25.0f);
				glTexCoord2f(0.0f, 1.0f);
				glVertex3f(-25.0f,25.0f,25.0f);
			glEnd();

			// Last two segments for effect
			glBindTexture(GL_TEXTURE_2D, textures[3]);
			glBegin(GL_QUADS);
				glTexCoord2f(1.0f, 1.0f);
				glVertex3f(25.0f,25.0f,-25.0f);
				glTexCoord2f(1.0f, 0.0f);
				glVertex3f(25.0f,-25.0f,-25.0f);
				glTexCoord2f(0.0f, 0.0f);
				glVertex3f(25.0f,-25.0f,25.0f);
				glTexCoord2f(0.0f, 1.0f);
				glVertex3f(25.0f,25.0f,25.0f);
			glEnd();
		

			glPopMatrix();

			// Disable lighting, we'll just draw the shadow as black
			glDisable(GL_LIGHTING);
			glDisable(GL_TEXTURE_2D);
			
			glPushMatrix();

			MakeShadowMatrix(ground, lightpos, cubeXform);
			glMultMatrixf((GLfloat *)cubeXform);
			
			glTranslatef(-10.0f, 0.0f, 10.0f);			
			
			// Set drawing color to Black
			glColor3f(0.0f, 0.0f, 0.0f);
			glutSolidCube(50.0f);
			break;
			
		}

	glPopMatrix();

	// Flush drawing commands
	glutSwapBuffers();
	}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
	{
        GLbyte *pBytes;
        GLint nWidth, nHeight, nComponents;
        GLenum format;

	// Black background
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f );

        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glGenTextures(4, textures);
        
	// Load the texture objects
        pBytes = gltLoadTGA("floor.tga", &nWidth, &nHeight, &nComponents, &format);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D,0,nComponents,nWidth, nHeight, 0,
		format, GL_UNSIGNED_BYTE, pBytes);
	free(pBytes);

	pBytes = gltLoadTGA("block4.tga", &nWidth, &nHeight, &nComponents, &format);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D,0,nComponents,nWidth, nHeight, 0,
		format, GL_UNSIGNED_BYTE, pBytes);
	free(pBytes);

	pBytes = gltLoadTGA("block5.tga", &nWidth, &nHeight, &nComponents, &format);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D,0,nComponents,nWidth, nHeight, 0,
		format, GL_UNSIGNED_BYTE, pBytes);
	free(pBytes);

	pBytes = gltLoadTGA("block6.tga", &nWidth, &nHeight, &nComponents, &format);
        glBindTexture(GL_TEXTURE_2D, textures[3]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D,0,nComponents,nWidth, nHeight, 0,
		format, GL_UNSIGNED_BYTE, pBytes);
	free(pBytes);
        }

void KeyPressFunc(unsigned char key, int x, int y)
	{
	if(key == 32)
		{
		nStep++;

		if(nStep > 5)
			nStep = 0;
		}

	// Refresh the Window
	glutPostRedisplay();
	}


void ChangeSize(int w, int h)
	{
	// Calculate new clipping volume
	GLfloat windowWidth;
	GLfloat windowHeight;

	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0)
		h = 1;

	// Keep the square square
	if (w <= h) 
		{
		windowHeight = 100.0f*(GLfloat)h/(GLfloat)w;
		windowWidth = 100.0f;
		}
    else 
		{
		windowWidth = 100.0f*(GLfloat)w/(GLfloat)h;
		windowHeight = 100.0f;
		}

        // Set the viewport to be the entire window
        glViewport(0, 0, w, h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

	// Set the clipping volume
	glOrtho(-100.0f, windowWidth, -100.0f, windowHeight, -200.0f, 200.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

	glLightfv(GL_LIGHT0,GL_POSITION, lightpos);

	glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(330.0f, 0.0f, 1.0f, 0.0f);
	}

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("3D Effects Demo");
	glutReshapeFunc(ChangeSize);
	glutKeyboardFunc(KeyPressFunc);
	glutDisplayFunc(RenderScene);

	SetupRC();

	glutMainLoop();
	glDeleteTextures(4,textures);
	return 0;
	}
