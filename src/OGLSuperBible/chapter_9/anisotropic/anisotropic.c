// Anisotropic.c
// Demonstrates anisotropic texture filtering
// OpenGL SuperBible
// Richard S. Wright Jr.
#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit

// Rotation amounts
static GLfloat zPos = -60.0f;

// Texture objects
#define TEXTURE_BRICK   0
#define TEXTURE_FLOOR  1
#define TEXTURE_CEILING 2
#define TEXTURE_COUNT   3
GLuint  textures[TEXTURE_COUNT];
const char *szTextureFiles[TEXTURE_COUNT] = { "brick.tga", "floor.tga", "ceiling.tga" };



///////////////////////////////////////////////////////////////////////////////
// Change texture filter for each texture object
void ProcessMenu(int value)
	{
    GLint iLoop;
    GLfloat fLargest;
    
    for(iLoop = 0; iLoop < TEXTURE_COUNT; iLoop++)
        {
        glBindTexture(GL_TEXTURE_2D, textures[iLoop]);
        
        switch(value)
            {
            case 0:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                break;
                
            case 1:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                break;
                
            case 2:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                break;
            
            case 3:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                break;
            
            case 4:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                break;
                
            case 5:
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                break;
                
            case 6:
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest);
                break;
                
            case 7:
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
                break;
            }
        }
        
    // Trigger Redraw
	glutPostRedisplay();
	}


//////////////////////////////////////////////////////////////////
// This function does any needed initialization on the rendering
// context.  Here it sets up and initializes the texture objects.
void SetupRC()
    {
    GLubyte *pBytes;
    GLint iWidth, iHeight, iComponents;
    GLenum eFormat;
    GLint iLoop;
    
	// Black background
	glClearColor(0.0f, 0.0f, 0.0f,1.0f);

    // Textures applied as decals, no lighting or coloring effects
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    // Load textures
    glGenTextures(TEXTURE_COUNT, textures);
    for(iLoop = 0; iLoop < TEXTURE_COUNT; iLoop++)
        {
        // Bind to next texture object
        glBindTexture(GL_TEXTURE_2D, textures[iLoop]);

        // Load texture, set filter and wrap modes
        pBytes = (GLubyte *) gltLoadTGA(szTextureFiles[iLoop],&iWidth, &iHeight, &iComponents, &eFormat);
        gluBuild2DMipmaps(GL_TEXTURE_2D, iComponents, iWidth, iHeight, eFormat, GL_UNSIGNED_BYTE, pBytes);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // Don't need original texture data any more
        free(pBytes);
        }
        
    
    }
    
///////////////////////////////////////////////////
// Shutdown the rendering context. Just deletes the
// texture objects
void ShutdownRC(void)
    {
    glDeleteTextures(TEXTURE_COUNT, textures);
    }
    

///////////////////////////////////////////////////
// Respond to arrow keys, move the viewpoint back
// and forth
void SpecialKeys(int key, int x, int y)
	{
	if(key == GLUT_KEY_UP)
		zPos += 1.0f;

	if(key == GLUT_KEY_DOWN)
		zPos -= 1.0f;

	// Refresh the Window
	glutPostRedisplay();
	}

/////////////////////////////////////////////////////////////////////
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
	gluPerspective(90.0f,fAspect,1,120);


    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    }

///////////////////////////////////////////////////////
// Called to draw scene
void RenderScene(void)
    {
    GLfloat z;
    
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT);

    // Save the matrix state and do the rotations
    glPushMatrix();
        // Move object back and do in place rotation
        glTranslatef(0.0f, 0.0f, zPos);
    
    	// Floor
        for(z = 60.0f; z >= 0.0f; z -= 10)
        {
        glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_FLOOR]);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-10.0f, -10.0f, z);

			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(10.0f, -10.0f, z);

			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(10.0f, -10.0f, z - 10.0f);

			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(-10.0f, -10.0f, z - 10.0f);
		glEnd();

		// Ceiling
		glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_CEILING]);
		glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(-10.0f, 10.0f, z - 10.0f);

			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(10.0f, 10.0f, z - 10.0f);

			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(10.0f, 10.0f, z);

			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(-10.0f, 10.0f, z);
		glEnd();

		
		// Left Wall
		glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_BRICK]);
		glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(-10.0f, -10.0f, z);

			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(-10.0f, -10.0f, z - 10.0f);

			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(-10.0f, 10.0f, z - 10.0f);

			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(-10.0f, 10.0f, z);
		glEnd();


		// Right Wall
		glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(10.0f, 10.0f, z);

			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(10.0f, 10.0f, z - 10.0f);

			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(10.0f, -10.0f, z - 10.0f);

			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(10.0f, -10.0f, z);
		glEnd();
		}

    // Restore the matrix state
    glPopMatrix();

    // Buffer swap
    glutSwapBuffers();
    }


//////////////////////////////////////////////////////
// Program entry point
int main(int argc, char *argv[])
    {
    // Standard initialization stuff
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Anisotropic Tunnel");
    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
    
    // Add menu entries to change filter
    glutCreateMenu(ProcessMenu);
    glutAddMenuEntry("GL_NEAREST",0);
    glutAddMenuEntry("GL_LINEAR",1);
    glutAddMenuEntry("GL_NEAREST_MIPMAP_NEAREST",2);
    glutAddMenuEntry("GL_NEAREST_MIPMAP_LINEAR", 3);
    glutAddMenuEntry("GL_LINEAR_MIPMAP_NEAREST", 4);
    glutAddMenuEntry("GL_LINEAR_MIPMAP_LINEAR", 5);
    glutAddMenuEntry("Anisotropic Filter", 6);
    glutAddMenuEntry("Anisotropic Off", 7);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
    
    // Startup, loop, shutdown
    SetupRC();
    glutMainLoop();
    ShutdownRC();
    
    return 0;
    }
    
    
  
