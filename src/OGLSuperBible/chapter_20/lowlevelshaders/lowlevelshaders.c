// LowLevelShaders.c
// OpenGL SuperBible, Chapter 20
// Demonstrates low-level shaders
// Program by Benjamin Lipchak

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifdef APIENTRY
#undef APIENTRY
#endif // APIENTRY
#define APIENTRY

#ifdef RAND_MAX
#undef RAND_MAX
#endif // RAND_MAX
#define RAND_MAX                                   0x7FFF

#define GL_VERTEX_PROGRAM_ARB                      0x8620
#define GL_FRAGMENT_PROGRAM_ARB                    0x8804
#define GL_PROGRAM_ERROR_POSITION_ARB              0x864B
#define GL_PROGRAM_ERROR_STRING_ARB                0x8874
#define GL_PROGRAM_FORMAT_ASCII_ARB                0x8875
/*
typedef GLvoid (APIENTRY *PFNGLPROGRAMSTRINGARBPROC)(GLenum target, GLenum format, GLsizei len, const GLvoid *string);
typedef GLvoid (APIENTRY *PFNGLBINDPROGRAMARBPROC)(GLenum target, GLuint program);
typedef GLvoid (APIENTRY *PFNGLDELETEPROGRAMSARBPROC)(GLsizei n, const GLuint *programs);
typedef GLvoid (APIENTRY *PFNGLGENPROGRAMSARBPROC)(GLsizei n, GLuint *programs);
typedef GLvoid (APIENTRY *PFNGLPROGRAMLOCALPARAMETER4FARBPROC)(GLenum target,GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

typedef GLvoid (APIENTRY * PFNGLSECONDARYCOLOR3FPROC) (GLfloat r, GLfloat g, GLfloat b);
*/
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
PFNGLBINDPROGRAMARBPROC glBindProgramARB;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;

PFNGLSECONDARYCOLOR3FPROC glSecondaryColor3f;

GLboolean useVertexShader = GL_TRUE;
GLboolean useFragmentShader = GL_TRUE;
GLboolean doBlink = GL_FALSE;

GLuint ids[2];                          // Shader object names

GLint windowWidth = 512;                // window size
GLint windowHeight = 512;

GLfloat cameraPos[]    = { 100.0f, 150.0f, 200.0f, 1.0f};

// Called to draw scene objects
void DrawModels(void)
{
    // Draw plane that the objects rest on
    glColor3f(0.0f, 0.0f, 0.90f); // Blue
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
        glVertex3f(-100.0f, -25.0f, -100.0f);
        glVertex3f(-100.0f, -25.0f, 100.0f);		
        glVertex3f(100.0f,  -25.0f, 100.0f);
        glVertex3f(100.0f,  -25.0f, -100.0f);
    glEnd();

    // Draw red cube
    glColor3f(1.0f, 0.0f, 0.0f);
    glutSolidCube(48.0f);

    // Draw green sphere
    glColor3f(0.0f, 1.0f, 0.0f);
    glPushMatrix();
    glTranslatef(-60.0f, 0.0f, 0.0f);
    glutSolidSphere(25.0f, 50, 50);
    glPopMatrix();

    // Draw yellow cone
    glColor3f(1.0f, 1.0f, 0.0f);
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(60.0f, 0.0f, -24.0f);
    glutSolidCone(25.0f, 50.0f, 50, 50);
    glPopMatrix();

    // Draw magenta torus
    glColor3f(1.0f, 0.0f, 1.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 60.0f);
    glutSolidTorus(8.0f, 16.0f, 50, 50);
    glPopMatrix();

    // Draw cyan octahedron
    glColor3f(0.0f, 1.0f, 1.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -60.0f);
    glScalef(25.0f, 25.0f, 25.0f);
    glutSolidOctahedron();
    glPopMatrix();
}

// Called to draw scene
void RenderScene(void)
{
    static GLfloat flickerFactor = 1.0f;

    // Track camera angle
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 1.0f, 1.0f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(cameraPos[0], cameraPos[1], cameraPos[2], 
              0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    glViewport(0, 0, windowWidth, windowHeight);
    
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (doBlink)
    {
        // Pick a random flicker factor
        flickerFactor += ((((GLfloat)rand())/((GLfloat)RAND_MAX)) - 0.5f) * 0.1f;
        if (flickerFactor > 1.0f) flickerFactor = 1.0f;
        if (flickerFactor < 0.0f) flickerFactor = 0.0f;
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, flickerFactor, flickerFactor, flickerFactor, 1.0f);
    }

    // Draw objects in the scene
    DrawModels();
    
    if (glGetError() != GL_NO_ERROR)
        fprintf(stderr, "GL Error!\n");

    // Flush drawing commands
    glutSwapBuffers();

    glutPostRedisplay();
}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
{
    GLint errorPos;
    const GLubyte *version;

    GLbyte vpString[] = 
        "!!ARBvp1.0\n"
        "# This is our Hello World vertex shader\n"
        "# notice how comments are preceded by '#'\n"
        "\n"
        "ATTRIB iPos = vertex.position;         # input position\n"
        "ATTRIB iPrC = vertex.color.primary;    # input primary color\n"
        "\n"
        "OUTPUT oPos = result.position;         # output position\n"
        "OUTPUT oPrC = result.color.primary;    # output primary color\n"
        "OUTPUT oScC = result.color.secondary;  # output secondary color\n"
        "\n"
        "PARAM mvp[4] = { state.matrix.mvp };   # model-view * projection matrix\n"
        "\n"
        "TEMP tmp;                              # temporary register\n"
        "\n"
        "DP4 tmp.x, iPos, mvp[0];               # multiply input position by MVP\n"
        "DP4 tmp.y, iPos, mvp[1];\n"
        "DP4 tmp.z, iPos, mvp[2];\n"
        "DP4 tmp.w, iPos, mvp[3];\n"
        "\n"
        "MOV oPos, tmp;                         # output clip-space coord\n"
        "\n"
        "MOV oPrC, iPrC;                        # copy primary color input to output\n"
        "\n"
        "RCP tmp.w, tmp.w;                      # tmp now contains 1/W instead of W\n"
        "MUL tmp.xyz, tmp, tmp.w;               # tmp now contains persp-divided coords\n"
        "MAD oScC, tmp, 0.5, 0.5;               # map from [-1,1] to [0,1] and output\n"
        "END\n";

    GLbyte fpString[] = 
        "!!ARBfp1.0\n"
        "# This is our Hello World fragment shader\n"
        "\n"
        "ATTRIB iPrC = fragment.color.primary;  # input primary color\n"
        "ATTRIB iScC = fragment.color.secondary;# input secondary color\n"
        "TEMP tmp;                              # temporary register\n"
        "\n"
        "OUTPUT oCol = result.color;            # output color\n"
        "\n"
        "LRP tmp.rgb, 0.5, iPrC, iScC;          # 50/50 mix of two colors\n"
        "MOV tmp.a, iPrC.a;                     # ignore secondary alpha\n"
        "\n"
        "MUL oCol, tmp, program.local[0];       # multiply by flicker factor\n"
        "END\n";

    fprintf(stdout, "Low-level Shaders Demo\n\n");

    // Make sure required functionality is available!
    if (!gltIsExtSupported("GL_ARB_vertex_program"))
    {
        fprintf(stderr, "GL_ARB_vertex_program extension is unavailable!\n");
        usleep(2000);
        exit(0);
    }
    if (!gltIsExtSupported("GL_ARB_fragment_program"))
    {
        fprintf(stderr, "GL_ARB_fragment_program extension is unavailable!\n");
        usleep(2000);
        exit(0);
    }
    version = glGetString(GL_VERSION);
    if (((version[0] != '1') || (version[1] != '.') || 
         (version[2] < '4') || (version[2] > '9')) &&   // 1.4+
        (!gltIsExtSupported("GL_EXT_secondary_color")))
    {
        fprintf(stderr, "Neither OpenGL 1.4 nor GL_EXT_secondary_color"
                        " extension is available!\n");
        usleep(2000);
        exit(0);
    }

    glGenProgramsARB = gltGetExtensionPointer("glGenProgramsARB");
    glBindProgramARB = gltGetExtensionPointer("glBindProgramARB");
    glProgramStringARB = gltGetExtensionPointer("glProgramStringARB");
    glDeleteProgramsARB = gltGetExtensionPointer("glDeleteProgramsARB");
    glProgramLocalParameter4fARB = gltGetExtensionPointer("glProgramLocalParameter4fARB");

    if (gltIsExtSupported("GL_EXT_secondary_color"))
        glSecondaryColor3f = gltGetExtensionPointer("glSecondaryColor3fEXT");
    else
        glSecondaryColor3f = gltGetExtensionPointer("glSecondaryColor3f");

    if (!glGenProgramsARB || !glBindProgramARB || !glProgramStringARB || 
        !glDeleteProgramsARB || !glProgramLocalParameter4fARB || 
        !glSecondaryColor3f)
    {
        fprintf(stderr, "Not all entrypoints were available!\n");
        usleep(2000);
        exit(0);
    }

    fprintf(stdout, "Controls:\n");
    fprintf(stdout, "\tRight-click for menu\n\n");
    fprintf(stdout, "\tx/X\t\tMove +/- in x direction\n");
    fprintf(stdout, "\ty/Y\t\tMove +/- in y direction\n");
    fprintf(stdout, "\tz/Z\t\tMove +/- in z direction\n\n");
    fprintf(stdout, "\tq\t\tExit demo\n\n");
    
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    glSecondaryColor3f(1.0f, 1.0f, 1.0f);

    // Hidden surface removal
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glShadeModel(GL_SMOOTH);

    // Create, set and enable shaders
    glGenProgramsARB(2, ids);

    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[0]);
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen((const char *)vpString), vpString);
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    if (errorPos != -1)
    {
        fprintf(stderr, "Error in vertex shader at position %d!\n", errorPos);
        fprintf(stderr, "Error string: %s\n", glGetString(GL_PROGRAM_ERROR_STRING_ARB));
        usleep(5000);
        exit(0);
    }

    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[1]);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen((const char *)fpString), fpString);
    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    if (errorPos != -1)
    {
        fprintf(stderr, "Error in fragment shader at position %d!\n", errorPos);
        fprintf(stderr, "Error string: %s\n", glGetString(GL_PROGRAM_ERROR_STRING_ARB));
        usleep(5000);
        exit(0);
    }

    if (useVertexShader)
        glEnable(GL_VERTEX_PROGRAM_ARB);
    if (useFragmentShader)
        glEnable(GL_FRAGMENT_PROGRAM_ARB);

    // Initially set the blink parameter to 1 (no flicker)
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 1.0f, 1.0f, 1.0f, 1.0f);
}

void ProcessMenu(int value)
{
    switch(value)
    {
    case 1:
        useVertexShader = !useVertexShader;
        if (useVertexShader)
        {
            glutChangeToMenuEntry(1, "Toggle vertex shader (currently ON)", 1);
            glEnable(GL_VERTEX_PROGRAM_ARB);
        }
        else
        {
            glutChangeToMenuEntry(1, "Toggle vertex shader (currently OFF)", 1);
            glDisable(GL_VERTEX_PROGRAM_ARB);
        }
        break;

    case 2:
        useFragmentShader = !useFragmentShader;
        if (useFragmentShader)
        {
            glutChangeToMenuEntry(2, "Toggle fragment shader (currently ON)", 2);
            glEnable(GL_FRAGMENT_PROGRAM_ARB);
        }
        else
        {
            glutChangeToMenuEntry(2, "Toggle fragment shader (currently OFF)", 2);
            glDisable(GL_FRAGMENT_PROGRAM_ARB);
        }
        break;

    case 3:
        doBlink = !doBlink;
        if (doBlink)
        {
            glutChangeToMenuEntry(3, "Toggle flicker (currently ON)", 3);
        }
        else
        {
            glutChangeToMenuEntry(3, "Toggle flicker (currently OFF)", 3);
            glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        }
        break;

    default:
        break;
    }

    // Refresh the Window
    glutPostRedisplay();
}

void KeyPressFunc(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'x':
        cameraPos[0] += 5.0f;
        break;
    case 'X':
        cameraPos[0] -= 5.0f;
        break;
    case 'y':
        cameraPos[1] += 5.0f;
        break;
    case 'Y':
        cameraPos[1] -= 5.0f;
        break;
    case 'z':
        cameraPos[2] += 5.0f;
        break;
    case 'Z':
        cameraPos[2] -= 5.0f;
        break;
    case 'q':
    case 'Q':
    case 27 : /* ESC */
        exit(0);
    }

    // Refresh the Window
    glutPostRedisplay();
}

void SpecialKeys(int key, int x, int y)
{
    switch (key)
    {
    case GLUT_KEY_LEFT:
        cameraPos[0] -= 5.0f;
        break;
    case GLUT_KEY_RIGHT:
        cameraPos[0] += 5.0f;
        break;
    case GLUT_KEY_UP:
        cameraPos[1] += 5.0f;
        break;
    case GLUT_KEY_DOWN:
        cameraPos[1] -= 5.0f;
        break;
    default:
        break;
    }

    // Refresh the Window
    glutPostRedisplay();
}

void ChangeSize(int w, int h)
{
    windowWidth = w;
    windowHeight = h;
}

int main(int argc, char* argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Low-level Shaders Demo");
    glutReshapeFunc(ChangeSize);
    glutKeyboardFunc(KeyPressFunc);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);

    // Create the Menu
    glutCreateMenu(ProcessMenu);
    glutAddMenuEntry("Toggle vertex shader (currently ON)", 1);
    glutAddMenuEntry("Toggle fragment shader (currently ON)", 2);
    glutAddMenuEntry("Toggle flicker (currently OFF)", 3);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    SetupRC();

    glutMainLoop();

    if (glDeleteProgramsARB)
        glDeleteProgramsARB(2, ids);

    return 0;
}
