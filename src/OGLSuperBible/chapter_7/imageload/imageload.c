// ImageLoad.c
// OpenGL SuperBible
// Demonstrates loading a color image
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>

        
//////////////////////////////////////////////////////////////////
// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	}


///////////////////////////////////////////////////////////////////////        
// Called to draw scene
void RenderScene(void)
    {
	GLubyte *pImage = 0;
	GLint iWidth, iHeight, iComponents;
	GLenum eFormat;

    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT);

    // Targa's are 1 byte aligned
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load the TGA file, get width, height, and component/format information
	pImage = (GLubyte *)gltLoadTGA("fire.tga", &iWidth, &iHeight, &iComponents, &eFormat);
	
    // Use Window coordinates to set raster position
	glRasterPos2i(0, 0);
	
    // Draw the pixmap
    if(pImage != 0)
        glDrawPixels(iWidth, iHeight, eFormat, GL_UNSIGNED_BYTE, pImage);
	
    // Don't need the image data anymore
	free(pImage);
		
    // Do the buffer Swap
    glutSwapBuffers();
    }


//////////////////////////////////////////////////////////////
// For this example, it really doesn't matter what the 
// projection is since we are using glWindowPos
void ChangeSize(int w, int h)
    {
    // Prevent a divide by zero, when window is too short
    // (you cant make a window of zero width).
    if(h == 0)
        h = 1;

    glViewport(0, 0, w, h);
        
	// Reset the coordinate system before modifying
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	
    // Set the clipping volume
    gluOrtho2D(0.0f, (GLfloat) w, 0.0, (GLfloat) h);
        
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();    
    }

/////////////////////////////////////////////////////////////
// Main program entrypoint
int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GL_DOUBLE);
    glutInitWindowSize(512 ,512);
    glutCreateWindow("OpenGL Image Loading");
    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    
    SetupRC();
    glutMainLoop();

    return 0;
    }
