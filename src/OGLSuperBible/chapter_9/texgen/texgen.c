// TexGen.c
// OpenGL SuperBible
// Demonstrates OpenGL Texture Coordinate Generation
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;

GLuint toTextures[2];       // Two texture objects
int iRenderMode = 3;        // Sphere Mapped is default

///////////////////////////////////////////////////////////////////////////////
// Reset flags as appropriate in response to menu selections
void ProcessMenu(int value)
    {
    // Projection plane
    GLfloat zPlane[] = { 0.0f, 0.0f, 1.0f, 0.0f };
    
    // Store render mode
    iRenderMode = value;
    
    // Set up textgen based on menu selection
    switch(value)
        {
        case 1:
            // Object Linear
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGenfv(GL_S, GL_OBJECT_PLANE, zPlane);
            glTexGenfv(GL_T, GL_OBJECT_PLANE, zPlane);
            break;

		case 2:
            // Eye Linear
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
            glTexGenfv(GL_S, GL_EYE_PLANE, zPlane);
            glTexGenfv(GL_T, GL_EYE_PLANE, zPlane);
			break;

		case 3:
        default:
            // Sphere Map
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            break;
		}

    glutPostRedisplay();    // Redisplay
    }



///////////////////////////////////////////////////////////////////////////////
// Called to draw scene
void RenderScene(void)
	{
	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Switch to orthographic view for background drawing
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0f, 1.0f, 0.0f, 1.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glBindTexture(GL_TEXTURE_2D, toTextures[1]);    // Background texture

    // We will specify texture coordinates
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    
    // No depth buffer writes for background
    glDepthMask(GL_FALSE);

    // Background image
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);
    
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(1.0f, 0.0f);
    
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(1.0f, 1.0f);
        
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(0.0f, 1.0f);
    glEnd();

    // Back to 3D land
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Turn texgen and depth writing back on
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glDepthMask(GL_TRUE);

    // May need to swtich to stripe texture
    if(iRenderMode != 3)
        glBindTexture(GL_TEXTURE_2D, toTextures[0]);

	// Save the matrix state and do the rotations
	glPushMatrix();
    glTranslatef(0.0f, 0.0f, -2.0f);
	glRotatef(xRot, 1.0f, 0.0f, 0.0f);
	glRotatef(yRot, 0.0f, 1.0f, 0.0f);

    // Draw the tours
    gltDrawTorus(0.35, 0.15, 61, 37);
                
    // Restore the matrix state
    glPopMatrix();
    
    // Display the results
    glutSwapBuffers();
    }

///////////////////////////////////////////////////////////////////////////////
// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    GLbyte *pBytes;                     // Texture bytes
    GLint iComponents, iWidth, iHeight; // Texture sizes
    GLenum eFormat;                     // Texture format
    
    glEnable(GL_DEPTH_TEST);	// Hidden surface removal
    glFrontFace(GL_CCW);		// Counter clock-wise polygons face out
    glEnable(GL_CULL_FACE);		// Do not calculate inside of jet

    // White background
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f );
    
    // Decal texture environment
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    
    // Two textures
    glGenTextures(2, toTextures);

    ///////////////////////////////////////////
    // Load the main texture
    glBindTexture(GL_TEXTURE_2D, toTextures[0]);
    pBytes = gltLoadTGA("stripes.tga", &iWidth, &iHeight, &iComponents, &eFormat);    
    glTexImage2D(GL_TEXTURE_2D, 0, iComponents, iWidth, iHeight, 0, eFormat, GL_UNSIGNED_BYTE, (void *)pBytes);
    free(pBytes);
    
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glEnable(GL_TEXTURE_2D);

    ///////////////////////////////////////////
    // Load environment map
    glBindTexture(GL_TEXTURE_2D, toTextures[1]);
    pBytes = gltLoadTGA("environment.tga", &iWidth, &iHeight, &iComponents, &eFormat);    
    glTexImage2D(GL_TEXTURE_2D, 0, iComponents, iWidth, iHeight, 0, eFormat, GL_UNSIGNED_BYTE, (void *)pBytes);
    free(pBytes);
    
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glEnable(GL_TEXTURE_2D);

    // Turn on texture coordiante generation
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    
    // Sphere Map will be the default
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
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
    }

///////////////////////////////////////////////////////////////////////////////
// Program Entry Point
int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800,600);
    glutCreateWindow("Texture Coordinate Generation");
    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
    SetupRC();

	// Create the Menu
	glutCreateMenu(ProcessMenu);
    glutAddMenuEntry("Object Linear",1);
	glutAddMenuEntry("Eye Linear",2);
	glutAddMenuEntry("Sphere Map",3);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

    glutMainLoop();

    // Don't forget the texture objects
    glDeleteTextures(2, toTextures);

    return 0;
    }
