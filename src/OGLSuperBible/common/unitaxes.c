// UnitAxes.c
// Draws a lit representation of the orientation of the unit axes
// Richard S. Wright Jr.
// OpenGL SuperBible

#include "gltools.h"
#include <stdio.h>

///////////////////////////////////////////////////////////////////
// Draw the unit axis. A small white sphere represents the origin
// and the three axes are colored Red, Green, and Blue, which 
// corresponds to positive X, Y, and Z respectively. Each axis has
// an arrow on the end, and normals are provided should the axes
// be lit. These are all built using the quadric shapes. For best
// results, put this in a display list.
void gltDrawUnitAxes(void)
	{
	GLUquadricObj *pObj;	// Temporary, used for quadrics

	// Measurements
	float fAxisRadius = 0.025f;
	float fAxisHeight = 1.0f;
	float fArrowRadius = 0.06f;
	float fArrowHeight = 0.1f;

	// Setup the quadric object
	pObj = gluNewQuadric();
	gluQuadricDrawStyle(pObj, GLU_FILL);
	gluQuadricNormals(pObj, GLU_SMOOTH);
	gluQuadricOrientation(pObj, GLU_OUTSIDE);
	gluQuadricTexture(pObj, GLU_FALSE);

	///////////////////////////////////////////////////////
	// Draw the blue Z axis first, with arrowed head
	glColor3f(0.0f, 0.0f, 1.0f);
	gluCylinder(pObj, fAxisRadius, fAxisRadius, fAxisHeight, 10, 1);
	glPushMatrix();
		glTranslatef(0.0f, 0.0f, 1.0f);
	gluCylinder(pObj, fArrowRadius, 0.0f, fArrowHeight, 10, 1);
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(pObj, fAxisRadius, fArrowRadius, 10, 1);
	glPopMatrix();

	///////////////////////////////////////////////////////
	// Draw the Red X axis 2nd, with arrowed head
    glColor3f(1.0f, 0.0f, 0.0f);
	glPushMatrix();
		glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
		gluCylinder(pObj, fAxisRadius, fAxisRadius, fAxisHeight, 10, 1);
		glPushMatrix();
			glTranslatef(0.0f, 0.0f, 1.0f);
			gluCylinder(pObj, fArrowRadius, 0.0f, fArrowHeight, 10, 1);
            glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
            gluDisk(pObj, fAxisRadius, fArrowRadius, 10, 1);
		glPopMatrix();
	glPopMatrix();
	
	///////////////////////////////////////////////////////
	// Draw the Green Y axis 3rd, with arrowed head
	glColor3f(0.0f, 1.0f, 0.0f);
	glPushMatrix();
		glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
		gluCylinder(pObj, fAxisRadius, fAxisRadius, fAxisHeight, 10, 1);
		glPushMatrix();
			glTranslatef(0.0f, 0.0f, 1.0f);
			gluCylinder(pObj, fArrowRadius, 0.0f, fArrowHeight, 10, 1);
            glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
            gluDisk(pObj, fAxisRadius, fArrowRadius, 10, 1);
		glPopMatrix();
	glPopMatrix();

	////////////////////////////////////////////////////////
	// White Sphere at origin
	glColor3f(1.0f, 1.0f, 1.0f);
	gluSphere(pObj, 0.05f, 15, 15);

	// Delete the quadric
	gluDeleteQuadric(pObj);
	}
