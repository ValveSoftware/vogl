// MotionBlur.c
// OpenGL SuperBible
// Demonstrates Motion Blur with the Accumulation buffer
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>

// Light and material Data
GLfloat fLightPos[4]   = { -100.0f, 100.0f, 50.0f, 1.0f };  // Point source
GLfloat fLightPosMirror[4] = { -100.0f, -100.0f, 50.0f, 1.0f };
GLfloat fNoLight[] = { 0.0f, 0.0f, 0.0f, 0.0f };
GLfloat fLowLight[] = { 0.25f, 0.25f, 0.25f, 1.0f };
GLfloat fBrightLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };

GLfloat yRot = 45.0f;
    
//////////////////////////////////////////////////////////////////
// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    // Grayish background
    glClearColor(fLowLight[0], fLowLight[1], fLowLight[2], fLowLight[3]);
   
    // Cull backs of polygons
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    
    // Setup light parameters
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, fNoLight);
    glLightfv(GL_LIGHT0, GL_AMBIENT, fLowLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, fBrightLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, fBrightLight);
    glLightfv(GL_LIGHT0, GL_POSITION, fLightPos);
    
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
     
    // Mostly use material tracking
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glMateriali(GL_FRONT, GL_SHININESS, 128);
    }


///////////////////////////////////////////////////////////
// Draw the ground as a series of triangle strips. The 
// shading model and colors are set such that we end up 
// with a black and white checkerboard pattern.
void DrawGround(void)
    {
    GLfloat fExtent = 20.0f;
    GLfloat fStep = 0.5f;
    GLfloat y = 0.0f;
    GLfloat fColor;
    GLfloat iStrip, iRun;
    GLint iBounce = 0;
    
    glShadeModel(GL_FLAT);
    for(iStrip = -fExtent; iStrip <= fExtent; iStrip += fStep)
        {
        glBegin(GL_TRIANGLE_STRIP);
            for(iRun = fExtent; iRun >= -fExtent; iRun -= fStep)
                {
                if((iBounce %2) == 0)
                    fColor = 1.0f;
                else
                    fColor = 0.0f;
                    
                glColor4f(fColor, fColor, fColor, 0.5f);
                glVertex3f(iStrip, y, iRun);
                glVertex3f(iStrip + fStep, y, iRun);
                
                iBounce++;
                }
        glEnd();
        }
    glShadeModel(GL_SMOOTH);
    }


/////////////////////////////////////////////////////////////
// Draw the ground and the revolving sphere
void DrawGeometry(void)
    {
     // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
    glPushMatrix();
        DrawGround();
        
        // Place the moving sphere
        glColor3f(1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, 0.5f, -3.5f);
        glRotatef(-(yRot * 2.0f), 0.0f, 1.0f, 0.0f);
        glTranslatef(1.0f, 0.0f, 0.0f);
        glutSolidSphere(0.1f, 17, 9);
    glPopMatrix();
    }

///////////////////////////////////////////////////////////////////////        
// Called to draw scene. The world is drawn multiple times with each
// frame blended with the last. The current rotation is advanced each
// time to create the illusion of motion blur.
void RenderScene(void)
    {
    GLfloat fPass;
    GLfloat fPasses = 10.0f;
    
    // Set the current rotation back a few degrees
    yRot = 35.0f;
            
    for(fPass = 0.0f; fPass < fPasses; fPass += 1.0f)
        {
        yRot += .75f; //1.0f / (fPass+1.0f);
       
        // Draw sphere
        DrawGeometry();
        
        // Accumulate to back buffer
        if(fPass == 0.0f)
            glAccum(GL_LOAD, 0.5f);
        else
            glAccum(GL_ACCUM, 0.5f * (1.0f / fPasses));
        }

    // copy accumulation buffer to color buffer and
    // do the buffer Swap
    glAccum(GL_RETURN, 1.0f);
    glutSwapBuffers();
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
    glTranslatef(0.0f, -0.4f, 0.0f);
    }

/////////////////////////////////////////////////////////////
// Main program entrypoint
int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ACCUM);
    glutInitWindowSize(800,600);
    glutCreateWindow("Motion Blur with the Accumulation Buffer");
    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    
    SetupRC();
    glutMainLoop();

    return 0;
    }
