// Pyramid.c
// Demonstrates Simple Texture Mapping
// OpenGL SuperBible
// Richard S. Wright Jr.
#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;


// Change viewing volume and viewport.  Called when window is resized
void ChangeSize(int w, int h)
    {
    GLfloat fAspect;

    // Prevent a divide by zero
    if(h == 0)
        h = 1;

    // Set Viewport to window dimensions
    glViewport(0, 0, w, h);

    fAspect = (GLfloat)w/(GLfloat)h;

    // Reset coordinate system
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Produce the perspective projection
    gluPerspective(35.0f, fAspect, 1.0, 40.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    }


// This function does any needed initialization on the rendering
// context.  Here it sets up and initializes the lighting for
// the scene.
void SetupRC()
    {
    GLubyte *pBytes;
    GLint iWidth, iHeight, iComponents;
    GLenum eFormat;
    
    // Light values and coordinates
    GLfloat  whiteLight[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    GLfloat  sourceLight[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat	 lightPos[] = { -10.f, 5.0f, 5.0f, 1.0f };

    glEnable(GL_DEPTH_TEST);	// Hidden surface removal
    glFrontFace(GL_CCW);		// Counter clock-wise polygons face out
    glEnable(GL_CULL_FACE);		// Do not calculate inside of jet

    // Enable lighting
    glEnable(GL_LIGHTING);

    // Setup and enable light 0
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,whiteLight);
    glLightfv(GL_LIGHT0,GL_AMBIENT,sourceLight);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,sourceLight);
    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
    glEnable(GL_LIGHT0);

    // Enable color tracking
    glEnable(GL_COLOR_MATERIAL);
	
    // Set Material properties to follow glColor values
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // Black blue background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    
    // Load texture
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    pBytes = (GLubyte *)gltLoadTGA("stone.tga", &iWidth, &iHeight, &iComponents, &eFormat);
    glBindTexture(GL_TEXTURE_2D, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, iComponents, iWidth, iHeight, 0, eFormat, GL_UNSIGNED_BYTE, pBytes);
    free(pBytes);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_TEXTURE_2D);
    }

// Respond to arrow keys
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
                
        xRot = (GLfloat)((const int)xRot % 360);
        yRot = (GLfloat)((const int)yRot % 360);

	// Refresh the Window
	glutPostRedisplay();
	}


// Called to draw scene
void RenderScene(void)
    {
    GLTVector3 vNormal;
    GLTVector3 vCorners[5] = { { 0.0f, .80f, 0.0f },     // Top           0
                              { -0.5f, 0.0f, -.50f },    // Back left     1
                              { 0.5f, 0.0f, -0.50f },    // Back right    2
                              { 0.5f, 0.0f, 0.5f },      // Front right   3
                              { -0.5f, 0.0f, 0.5f }};    // Front left    4
                              
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Save the matrix state and do the rotations
    glPushMatrix();
        // Move object back and do in place rotation
        glTranslatef(0.0f, -0.25f, -4.0f);
        glRotatef(xRot, 1.0f, 0.0f, 0.0f);
        glRotatef(yRot, 0.0f, 1.0f, 0.0f);

        // Draw the Pyramid
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_TRIANGLES);
            // Bottom section - two triangles
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3fv(vCorners[2]);
            
            glTexCoord2f(0.0f, 0.0f);
            glVertex3fv(vCorners[4]);
            
            glTexCoord2f(0.0f, 1.0f);
            glVertex3fv(vCorners[1]);
            
            
            glTexCoord2f(1.0f, 1.0f);
            glVertex3fv(vCorners[2]);
            
            glTexCoord2f(1.0f, 0.0f);
            glVertex3fv(vCorners[3]);
            
            glTexCoord2f(0.0f, 0.0f);
            glVertex3fv(vCorners[4]);
            
            // Front Face
            gltGetNormalVector(vCorners[0], vCorners[4], vCorners[3], vNormal);
            glNormal3fv(vNormal);
            glTexCoord2f(0.5f, 1.0f);
            glVertex3fv(vCorners[0]);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3fv(vCorners[4]);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3fv(vCorners[3]);
            
            // Left Face
            gltGetNormalVector(vCorners[0], vCorners[1], vCorners[4], vNormal);
            glNormal3fv(vNormal);
            glTexCoord2f(0.5f, 1.0f);
            glVertex3fv(vCorners[0]);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3fv(vCorners[1]);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3fv(vCorners[4]);

            // Back Face
            gltGetNormalVector(vCorners[0], vCorners[2], vCorners[1], vNormal);
            glNormal3fv(vNormal);
            glTexCoord2f(0.5f, 1.0f);
            glVertex3fv(vCorners[0]);
            
            glTexCoord2f(0.0f, 0.0f);
            glVertex3fv(vCorners[2]);
            
            glTexCoord2f(1.0f, 0.0f);
            glVertex3fv(vCorners[1]);
            
            // Right Face
            gltGetNormalVector(vCorners[0], vCorners[3], vCorners[2], vNormal);
            glNormal3fv(vNormal);
            glTexCoord2f(0.5f, 1.0f);
            glVertex3fv(vCorners[0]);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3fv(vCorners[3]);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3fv(vCorners[2]);
        glEnd();
    

    // Restore the matrix state
    glPopMatrix();

    // Buffer swap
    glutSwapBuffers();
    }



int main(int argc, char *argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Textured Pyramid");
    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
    SetupRC();
    glutMainLoop();
    
    return 0;
    }
    
    
  
