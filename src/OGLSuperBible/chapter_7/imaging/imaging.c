// Imaging.c
// OpenGL SuperBible
// Demonstrates Imaging Operations
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////
// Module globals to save source image data
static GLubyte *pImage = 0;
static GLint iWidth, iHeight, iComponents;
static GLenum eFormat;

// Global variable to store desired drawing mode
static GLint        iRenderMode = 1;    
static GLboolean    bHistogram = GL_FALSE;  

        

//////////////////////////////////////////////////////////////////
// Define function pointers for imaging subset
#ifdef _WIN32
// These typdefs are found in glext.h
PFNGLHISTOGRAMPROC				glHistogram = 0;
PFNGLGETHISTOGRAMPROC			glGetHistogram = 0;
PFNGLCOLORTABLEPROC				glColorTable = 0;
PFNGLCONVOLUTIONFILTER2DPROC	glConvolutionFilter2D = 0;
#endif


//////////////////////////////////////////////////////////////////
// This function does any needed initialization on the rendering
// context. 
void SetupRC(void)
    {
#ifdef _WIN32
	glHistogram = gltGetExtensionPointer("glHistogram");
	glGetHistogram = gltGetExtensionPointer("glGetHistogram");
	glColorTable = gltGetExtensionPointer("glColorTable");
	glConvolutionFilter2D = gltGetExtensionPointer("glConvolutionFilter2D");
#endif

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
    // For historgram, do not change render mode, just set
    // histogram flag to true
    if(value == 6)  // Histogram
        {
        bHistogram = GL_TRUE;
        glutPostRedisplay();
        return;
        }
        
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
    GLint i;                    // Looping variable
    GLint iViewport[4];         // Viewport
	GLint iLargest;				// Largest histogram value

    static GLubyte invertTable[256][3];// Inverted color table

    // Do a black and white scaling
    static GLfloat lumMat[16] = { 0.30f, 0.30f, 0.30f, 0.0f,
                                  0.59f, 0.59f, 0.59f, 0.0f,
                                  0.11f, 0.11f, 0.11f, 0.0f,
                                  0.0f,  0.0f,  0.0f,  1.0f };
                   
    static GLfloat mSharpen[3][3] = {  // Sharpen convolution kernel
         {0.0f, -1.0f, 0.0f},
         {-1.0f, 5.0f, -1.0f },
         {0.0f, -1.0f, 0.0f }};

    static GLfloat mEmboss[3][3] = {   // Emboss convolution kernel
        { 2.0f, 0.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f }};
        
    static GLint histoGram[256];    // Storeage for histogram statistics
   
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Current Raster Position always at bottom left hand corner of window
    glRasterPos2i(0, 0);
    glGetIntegerv(GL_VIEWPORT, iViewport);
    glPixelZoom((GLfloat) iViewport[2] / (GLfloat)iWidth, (GLfloat) iViewport[3] / (GLfloat)iHeight); 

    if(bHistogram == GL_TRUE)   // Collect Historgram data
        {
        // We are collecting luminance data, use our conversion formula
        // instead of OpenGL's (which just adds color components together)
        glMatrixMode(GL_COLOR);
        glLoadMatrixf(lumMat);
        glMatrixMode(GL_MODELVIEW);

        // Start collecting histogram data, 256 luminance values
        glHistogram(GL_HISTOGRAM, 256, GL_LUMINANCE, GL_FALSE);
        glEnable(GL_HISTOGRAM);
        }
        
    // Do image operation, depending on rendermode index
    switch(iRenderMode)
        {
        case 5:     // Sharpen image
            glConvolutionFilter2D(GL_CONVOLUTION_2D, GL_RGB, 3, 3, GL_LUMINANCE, GL_FLOAT, mSharpen);
            glEnable(GL_CONVOLUTION_2D);
            break;

        case 4:     // Emboss image
            glConvolutionFilter2D(GL_CONVOLUTION_2D, GL_RGB, 3, 3, GL_LUMINANCE, GL_FLOAT, mEmboss);
            glEnable(GL_CONVOLUTION_2D);
            glMatrixMode(GL_COLOR);
            glLoadMatrixf(lumMat);
            glMatrixMode(GL_MODELVIEW);
            break;
        
        case 3:     // Invert Image
            for(i = 0; i < 255; i++)
                {
                invertTable[i][0] = (GLubyte)(255 - i);
                invertTable[i][1] = (GLubyte)(255 - i);
                invertTable[i][2] = (GLubyte)(255 - i);
                }
                
            glColorTable(GL_COLOR_TABLE, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, invertTable);
            glEnable(GL_COLOR_TABLE);
            break;
        
        case 2:     // Brighten Image
            glMatrixMode(GL_COLOR);
            glScalef(1.25f, 1.25f, 1.25f);
            glMatrixMode(GL_MODELVIEW);
            break;
            
        case 1:     // Just do a plain old image copy
        default:
                    // This line intentially left blank
            break;
        }
		
    // Do the pixel draw
    glDrawPixels(iWidth, iHeight, eFormat, GL_UNSIGNED_BYTE, pImage);
    
    // Fetch and draw histogram?
    if(bHistogram == GL_TRUE)  
        {
        // Read histogram data into buffer
        glGetHistogram(GL_HISTOGRAM, GL_TRUE, GL_LUMINANCE, GL_INT, histoGram);
        
        // Find largest value for scaling graph down
		iLargest = 0;
        for(i = 0; i < 255; i++)
            if(iLargest < histoGram[i])
                iLargest = histoGram[i];
        
        // White lines
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINE_STRIP);
            for(i = 0; i < 255; i++)
                glVertex2f((GLfloat)i, (GLfloat)histoGram[i] / (GLfloat) iLargest * 128.0f); 
        glEnd();

        bHistogram = GL_FALSE;
        glDisable(GL_HISTOGRAM);
        }
    
        
    // Reset everyting to default
    glMatrixMode(GL_COLOR);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_CONVOLUTION_2D);
    glDisable(GL_COLOR_TABLE);                                                    

    // Show our hard work...
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
    glutInitWindowSize(600 ,600);
    glutCreateWindow("OpenGL Imaging subset");

    // Check for imaging subset, must be done after window
    // is create or there won't be an OpenGL context to query
    if(gltIsExtSupported("GL_ARB_imaging") == 0)
        {
        printf("Imaging subset not supported\r\n");
        return 0;
        }

    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    
    // Create the Menu and add choices
	glutCreateMenu(ProcessMenu);
	glutAddMenuEntry("Save Image",0);
    glutAddMenuEntry("Raw Stretched Image",1);
    glutAddMenuEntry("Increase Contrast",2);
    glutAddMenuEntry("Invert Color", 3);
    glutAddMenuEntry("Emboss Image", 4);
    glutAddMenuEntry("Sharpen Image", 5);
    glutAddMenuEntry("Histogram", 6);
    
    glutAttachMenu(GLUT_RIGHT_BUTTON);
    
    SetupRC();          // Do setup
    
    glutMainLoop();     // Main program loop

    ShutdownRC();       // Do shutdown

    return 0;
    }
