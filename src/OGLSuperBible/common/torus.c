// torus.c
// Draws a lit, textured torus
// Richard S. Wright Jr.
// OpenGL SuperBible
// 

#include "gltools.h"
#include <stdio.h>

// For best results, put this in a display list
// Draw a torus (doughnut)  at z = fZVal... torus is in xy plane
void gltDrawTorus(GLfloat majorRadius, GLfloat minorRadius, GLint numMajor, GLint numMinor)
    {
    GLTVector3 vNormal;
    double majorStep = 2.0f*GLT_PI / numMajor;
    double minorStep = 2.0f*GLT_PI / numMinor;
    int i, j;

    for (i=0; i<numMajor; ++i) 
            {
            double a0 = i * majorStep;
            double a1 = a0 + majorStep;
            GLfloat x0 = (GLfloat) cos(a0);
            GLfloat y0 = (GLfloat) sin(a0);
            GLfloat x1 = (GLfloat) cos(a1);
            GLfloat y1 = (GLfloat) sin(a1);

            glBegin(GL_TRIANGLE_STRIP);
            for (j=0; j<=numMinor; ++j) 
                    {
                    double b = j * minorStep;
                    GLfloat c = (GLfloat) cos(b);
                    GLfloat r = minorRadius * c + majorRadius;
                    GLfloat z = minorRadius * (GLfloat) sin(b);

                    // First point
                    glTexCoord2f((float)(i)/(float)(numMajor), (float)(j)/(float)(numMinor));
                    vNormal[0] = x0*c;
                    vNormal[1] = y0*c;
                    vNormal[2] = z/minorRadius;
                    gltNormalizeVector(vNormal);
                    glNormal3fv(vNormal);
                    glVertex3f(x0*r, y0*r, z);

                    glTexCoord2f((float)(i+1)/(float)(numMajor), (float)(j)/(float)(numMinor));
                    vNormal[0] = x1*c;
                    vNormal[1] = y1*c;
                    vNormal[2] = z/minorRadius;
                    glNormal3fv(vNormal);
                    glVertex3f(x1*r, y1*r, z);
                    }
            glEnd();
            }
	}
