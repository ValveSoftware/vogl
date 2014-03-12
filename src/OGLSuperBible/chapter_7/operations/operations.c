// Operations.c
// OpenGL SuperBible
// Demonstrates Imaging Operations
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>

//////////////////////////////////////////////////////////////////
// Module globals to save source image data
static GLubyte *pImage = 0;
static GLint iWidth, iHeight, iComponents;
static GLenum eFormat;

// Global variable to store desired drawing mode
static GLint    iRenderMode = 1;    

        
//////////////////////////////////////////////////////////////////
// This function does any needed initialization on the rendering
// context. 
void SetupRC(void)
    {
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
    // Load the horse image
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   	pImage = (GLubyte *)gltLoadTGA("horse.tga", &iWidth, &iHeight, &iComponents, &eFormat);
    }

void ShutdownRC(void)
    {
    // Free the original image data
    free(pImage);
    }


///////////////////////////////////////////////////////////////////////////////
// Reset flags as appropriate in response to menu selections
void ProcessMenu(int value)
	{
    if(value == 0)
	     // Save image
        gltWriteTGA("ScreenShot.tga");
    else
        // Change render mode index to match menu entry index
        iRenderMode = value;
        
    // Trigger Redraw
	glutPostRedisplay();
	}



///////////////////////////////////////////////////////////////////////        
// Called to draw scene
void RenderScene(void)
    {
    GLint iViewport[4];
    GLbyte *pModifiedBytes = 0;
    GLfloat invertMap[256];
    GLint i;
    
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Current Raster Position always at bottom left hand corner of window
    glRasterPos2i(0, 0);
        
    // Do image operation, depending on rendermode index
    switch(iRenderMode)
        {
        case 2:     // Flip the pixels
            glPixelZoom(-1.0f, -1.0f);
            glRasterPos2i(iWidth, iHeight);
            break;
            
        case 3:     // Zoom pixels to fill window
            glGetIntegerv(GL_VIEWPORT, iViewport);
            glPixelZoom((GLfloat) iViewport[2] / (GLfloat)iWidth, (GLfloat) iViewport[3] / (GLfloat)iHeight); 
            break;
            
        case 4:     // Just Red
            glPixelTransferf(GL_RED_SCALE, 1.0f);         
            glPixelTransferf(GL_GREEN_SCALE, 0.0f);
            glPixelTransferf(GL_BLUE_SCALE, 0.0f); 
            break;
            
        case 5:     // Just Green
            glPixelTransferf(GL_RED_SCALE, 0.0f);         
            glPixelTransferf(GL_GREEN_SCALE, 1.0f);
            glPixelTransferf(GL_BLUE_SCALE, 0.0f); 
            break;
            
        case 6:     // Just Blue
            glPixelTransferf(GL_RED_SCALE, 0.0f);         
            glPixelTransferf(GL_GREEN_SCALE, 0.0f);
            glPixelTransferf(GL_BLUE_SCALE, 1.0f); 
            break;
            
        case 7:     // Black & White, more tricky
            // First draw image into color buffer
            glDrawPixels(iWidth, iHeight, eFormat, GL_UNSIGNED_BYTE, pImage);
            
            // Allocate space for the luminance map
            pModifiedBytes = (GLbyte *)malloc(iWidth * iHeight);
            
            // Scale colors according to NSTC standard
            glPixelTransferf(GL_RED_SCALE, 0.3f);         
            glPixelTransferf(GL_GREEN_SCALE, 0.59f);
            glPixelTransferf(GL_BLUE_SCALE, 0.11f);
            
            // Read pixles into buffer (scale above will be applied)
            glReadPixels(0,0,iWidth, iHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, pModifiedBytes);
            
            // Return color scaling to normal
            glPixelTransferf(GL_RED_SCALE, 1.0f);
            glPixelTransferf(GL_GREEN_SCALE, 1.0f);
            glPixelTransferf(GL_BLUE_SCALE, 1.0f);
            break;
            
        case 8:     // Invert colors
            invertMap[0] = 1.0f;
            for(i = 1; i < 256; i++)
                invertMap[i] = 1.0f - (1.0f / 255.0f * (GLfloat)i);
                
            glPixelMapfv(GL_PIXEL_MAP_R_TO_R, 255, invertMap);
            glPixelMapfv(GL_PIXEL_MAP_G_TO_G, 255, invertMap);
            glPixelMapfv(GL_PIXEL_MAP_B_TO_B, 255, invertMap);
            glPixelTransferi(GL_MAP_COLOR, GL_TRUE);
            break;

        case 1:     // Just do a plain old image copy
        default:
                    // This line intentially left blank
            break;
        }
		
    // Do the pixel draw
    if(pModifiedBytes == 0)
        glDrawPixels(iWidth, iHeight, eFormat, GL_UNSIGNED_BYTE, pImage);
    else
        {
        glDrawPixels(iWidth, iHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, pModifiedBytes);
        free(pModifiedBytes);
        }


    // Reset everyting to default
    glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
    glPixelTransferf(GL_RED_SCALE, 1.0f);
    glPixelTransferf(GL_GREEN_SCALE, 1.0f);
    glPixelTransferf(GL_BLUE_SCALE, 1.0f);
    glPixelZoom(1.0f, 1.0f);                    // No Pixel Zooming
        
    // Do the buffer Swap
    glutSwapBuffers();
    }



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
    glutInitWindowSize(800 ,600);
    glutCreateWindow("OpenGL Image Operations");
    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    
    // Create the Menu and add choices
	glutCreateMenu(ProcessMenu);
	glutAddMenuEntry("Save Image",0);
    glutAddMenuEntry("Draw Pixels",1);
    glutAddMenuEntry("Flip Pixels",2);
    glutAddMenuEntry("Zoom Pixels",3);
    glutAddMenuEntry("Just Red Channel",4);
    glutAddMenuEntry("Just Green Channel",5);
    glutAddMenuEntry("Just Blue Channel",6);
    glutAddMenuEntry("Black and White", 7);
    glutAddMenuEntry("Invert Colors", 8);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    
    SetupRC();          // Do setup
    
    glutMainLoop();     // Main program loop

    ShutdownRC();       // Do shutdown

    return 0;
    }
