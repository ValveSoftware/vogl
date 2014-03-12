// Transform.c
// OpenGL SuperBible
// Demonstrates manual transformations
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>


// Draw a torus (doughnut), using the current 1D texture for light shading
void DrawTorus(GLTMatrix mTransform)
    {
    GLfloat majorRadius = 0.35f;
    GLfloat minorRadius = 0.15f;
    GLint   numMajor = 40;
    GLint   numMinor = 20;
    GLTVector3 objectVertex;         // Vertex in object/eye space
    GLTVector3 transformedVertex;    // New Transformed vertex   
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
            objectVertex[0] = x0*r;
            objectVertex[1] = y0*r;
            objectVertex[2] = z;
            gltTransformPoint(objectVertex, mTransform, transformedVertex);
            glVertex3fv(transformedVertex);

            // Second point
            objectVertex[0] = x1*r;
            objectVertex[1] = y1*r;
            objectVertex[2] = z;
            gltTransformPoint(objectVertex, mTransform, transformedVertex);
            glVertex3fv(transformedVertex);
            }
        glEnd();
        }
    }
        
        
// Called to draw scene
void RenderScene(void)
    {
    GLTMatrix   transformationMatrix;   // Storeage for rotation matrix
    static GLfloat yRot = 0.0f;         // Rotation angle for animation
    yRot += 0.5f;
        
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
    // Build a rotation matrix
    gltRotationMatrix(gltDegToRad(yRot), 0.0f, 1.0f, 0.0f, transformationMatrix);
    transformationMatrix[12] = 0.0f;
    transformationMatrix[13] = 0.0f;
    transformationMatrix[14] = -2.5f;
        
    DrawTorus(transformationMatrix);

    // Do the buffer Swap
    glutSwapBuffers();
    }

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    // Bluish background
    glClearColor(0.0f, 0.0f, .50f, 1.0f );
         
    // Draw everything as wire frame
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

///////////////////////////////////////////////////////////
// Called by GLUT library when idle (window not being
// resized or moved)
void TimerFunction(int value)
    {
    // Redraw the scene with new coordinates
    glutPostRedisplay();
    glutTimerFunc(33,TimerFunction, 1);
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
    }

int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800,600);
    glutCreateWindow("Manual Transformations Demo");
    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);

    SetupRC();
    glutTimerFunc(33, TimerFunction, 1);

    glutMainLoop();

    return 0;
    }
