// Bitmaps.c
// OpenGL SuperBible
// Demonstrates loading and displaying bitmaps
// Program by Richard S. Wright Jr.

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit

// Bitmap of camp fire
GLubyte fire[128] = { 0x00, 0x00, 0x00, 0x00, 
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0xc0,
				   0x00, 0x00, 0x01, 0xf0,
				   0x00, 0x00, 0x07, 0xf0,
				   0x0f, 0x00, 0x1f, 0xe0,
				   0x1f, 0x80, 0x1f, 0xc0,
				   0x0f, 0xc0, 0x3f, 0x80,	
				   0x07, 0xe0, 0x7e, 0x00,
				   0x03, 0xf0, 0xff, 0x80,
				   0x03, 0xf5, 0xff, 0xe0,
				   0x07, 0xfd, 0xff, 0xf8,
				   0x1f, 0xfc, 0xff, 0xe8,
				   0xff, 0xe3, 0xbf, 0x70, 
				   0xde, 0x80, 0xb7, 0x00,
				   0x71, 0x10, 0x4a, 0x80,
				   0x03, 0x10, 0x4e, 0x40,
				   0x02, 0x88, 0x8c, 0x20,
				   0x05, 0x05, 0x04, 0x40,
				   0x02, 0x82, 0x14, 0x40,
				   0x02, 0x40, 0x10, 0x80, 
				   0x02, 0x64, 0x1a, 0x80,
				   0x00, 0x92, 0x29, 0x00,
				   0x00, 0xb0, 0x48, 0x00,
				   0x00, 0xc8, 0x90, 0x00,
				   0x00, 0x85, 0x10, 0x00,
				   0x00, 0x03, 0x00, 0x00,
				   0x00, 0x00, 0x10, 0x00 };
	
        
//////////////////////////////////////////////////////////////////
// This function does any needed initialization on the rendering
// context. 
void SetupRC()
    {
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	}


//////////////////////////////////////////////////////////
// Set coordinate system to match window coordinates
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

	// Psuedo window coordinates
	gluOrtho2D(0.0, (GLfloat) w, 0.0f, (GLfloat) h);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();    
    }


/////////////////////////////////////////////////////////////////
// Called to draw scene
void RenderScene(void)
    {
	int x, y;
	
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT);
        
	// Set color to white
	glColor3f(1.0f, 1.0f, 1.0f);
	
	// Loop through 16 rows and columns
	for(y = 0; y < 16; y++)
		{
		// Set raster position for this "square"
		glRasterPos2i(0, y * 32);
		for(x = 0; x < 16; x++)
			// Draw the "fire" bitmap, advance raster position
			glBitmap(32, 32, 0.0, 0.0, 32.0, 0.0, fire);
		}
		
    // Do the buffer Swap
    glutSwapBuffers();
    }


/////////////////////////////////////////////////////////////
// Main program entrypoint
int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(512, 512);
    glutCreateWindow("OpenGL Bitmaps");
    glutReshapeFunc(ChangeSize);
    glutDisplayFunc(RenderScene);
    
    SetupRC();
    glutMainLoop();

    return 0;
    }
