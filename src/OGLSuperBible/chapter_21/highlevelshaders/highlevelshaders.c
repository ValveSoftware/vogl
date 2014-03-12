// HighLevelShaders.c
// OpenGL SuperBible, Chapter 21
// Demonstrates high-level shaders
// Program by Benjamin Lipchak

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#ifdef APIENTRY
#undef APIENTRY
#endif // APIENTRY
#define APIENTRY

#ifdef RAND_MAX
#undef RAND_MAX
#endif // RAND_MAX
#define RAND_MAX                                                       0x7FFF

#define GL_OBJECT_COMPILE_STATUS_ARB                0x8B81
#define GL_OBJECT_LINK_STATUS_ARB                   0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB               0x8B83
#define GL_VERTEX_SHADER_ARB                        0x8B31
#define GL_FRAGMENT_SHADER_ARB                      0x8B30

typedef char         GLcharARB;
typedef unsigned int GLhandleARB;

typedef void (APIENTRY *PFNGLGETATTACHEDSHADERS)(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *obj);
typedef GLboolean (APIENTRY *PFNGLISSHADER)(GLuint shader);
typedef void (APIENTRY *PFNGLDELETESHADER)(GLuint shader);

typedef GLvoid      (APIENTRY *PFNGLDELETEOBJECTARBPROC)(GLhandleARB obj);
typedef GLhandleARB (APIENTRY *PFNGLGETHANDLEARBPROC)(GLenum pname);
typedef GLvoid      (APIENTRY *PFNGLDETACHOBJECTARBPROC)(GLhandleARB containerObj,
                                                         GLhandleARB attachedObj);
typedef GLhandleARB (APIENTRY *PFNGLCREATESHADEROBJECTARBPROC)(GLenum shaderType);
typedef GLvoid      (APIENTRY *PFNGLSHADERSOURCEARBPROC)(GLhandleARB shaderObj,
                                                         GLsizei count,
                                                         const GLcharARB **string,
                                                         const GLint *length);
typedef GLvoid      (APIENTRY *PFNGLCOMPILESHADERARBPROC)(GLhandleARB shaderObj);
typedef GLhandleARB (APIENTRY *PFNGLCREATEPROGRAMOBJECTARBPROC)(GLvoid);
typedef GLvoid      (APIENTRY *PFNGLATTACHOBJECTARBPROC)(GLhandleARB containerObj,
                                                         GLhandleARB obj);
typedef GLvoid      (APIENTRY *PFNGLLINKPROGRAMARBPROC)(GLhandleARB programObj);
typedef GLvoid      (APIENTRY *PFNGLVALIDATEPROGRAMARBPROC)(GLhandleARB programObj);
typedef GLvoid      (APIENTRY *PFNGLUSEPROGRAMOBJECTARBPROC)(GLhandleARB programObj);
typedef GLvoid      (APIENTRY *PFNGLGETOBJECTPARAMETERIVARBPROC)(GLhandleARB obj,
                                                                 GLenum pname,
                                                                 GLint *params);
typedef GLvoid      (APIENTRY *PFNGLGETINFOLOGARBPROC)(GLhandleARB obj,
                                                       GLsizei maxLength,
                                                       GLsizei *length,
                                                       GLcharARB *infoLog);
typedef GLvoid      (APIENTRY *PFNGLUNIFORM1FARBPROC)(GLint location,
                                                      GLfloat v0);
typedef GLint       (APIENTRY *PFNGLGETUNIFORMLOCATIONARBPROC)(GLhandleARB programObj,
                                                               const GLcharARB *name);


PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
PFNGLGETHANDLEARBPROC glGetHandleARB;
PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
PFNGLUNIFORM1FARBPROC glUniform1fARB;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;

PFNGLSECONDARYCOLOR3FPROC glSecondaryColor3f;

PFNGLGETATTACHEDSHADERS glGetAttachedShaders;
PFNGLISSHADER glIsShader;
PFNGLDELETESHADER glDeleteShader;

GLboolean useVertexShader = GL_TRUE;
GLboolean useFragmentShader = GL_TRUE;
GLboolean doBlink = GL_FALSE;

GLboolean needsValidation = GL_TRUE;

GLhandleARB vShader, fShader, progObj;
GLint flickerLocation = -1;

GLint windowWidth = 512;                // window size
GLint windowHeight = 512;

GLfloat cameraPos[]    = { 100.0f, 150.0f, 200.0f, 1.0f};

#define MAX_INFO_LOG_SIZE 2048

void Link(GLboolean firstTime)
{
    GLint success;

    glLinkProgramARB(progObj);
    glGetObjectParameterivARB(progObj, GL_OBJECT_LINK_STATUS_ARB, &success);
    if (!success)
    {
        GLbyte infoLog[MAX_INFO_LOG_SIZE];
        glGetInfoLogARB(progObj, MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
        fprintf(stderr, "Error in program linkage!\n");
        fprintf(stderr, "Info log: %s\n", infoLog);
        usleep(10000);
        exit(0);
    }

    if (firstTime)
        glUseProgramObjectARB(progObj);

    // Find out where the flicker constant lives
    flickerLocation = glGetUniformLocationARB(progObj, "flickerFactor");

    // Initially set the blink parameter to 1 (no flicker)
    if (flickerLocation != -1)
        glUniform1fARB(flickerLocation, 1.0f);

    // Program object has changed, so we should revalidate
    needsValidation = GL_TRUE;
}

void Relink()
{
    Link(GL_FALSE);
}

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

    if (doBlink && (flickerLocation != -1))
    {
        // Pick a random flicker factor
        flickerFactor += ((((GLfloat)rand())/((GLfloat)RAND_MAX)) - 0.5f) * 0.1f;
        if (flickerFactor > 1.0f) flickerFactor = 1.0f;
        if (flickerFactor < 0.0f) flickerFactor = 0.0f;
        glUniform1fARB(flickerLocation, flickerFactor);
    }

    // Validate our shader before first use
    if (needsValidation)
    {
        GLint success;

        glValidateProgramARB(progObj);
        glGetObjectParameterivARB(progObj, GL_OBJECT_VALIDATE_STATUS_ARB, &success);
        if (!success)
        {
            GLbyte infoLog[MAX_INFO_LOG_SIZE];
            glGetInfoLogARB(progObj, MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
            fprintf(stderr, "Error in program validation!\n");
            fprintf(stderr, "Info log: %s\n", infoLog);
            usleep(10000);
            exit(0);
        }

        needsValidation = GL_FALSE;
    }
    
    // Draw objects in the scene
    DrawModels();
    
    if (glGetError() != GL_NO_ERROR)
        fprintf(stderr, "GL Error!\n");

    // Flush drawing commands
    glutSwapBuffers();

    if (doBlink && (flickerLocation != -1))
        glutPostRedisplay();
}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
{
    GLint success;
    const GLubyte *version;
    GLcharARB *vsStringPtr[1];
    GLcharARB *fsStringPtr[1];

    GLcharARB vsString[] = 
        "void main(void)\n"
        "{\n"
        "    // This is our Hello World vertex shader\n"
        "    // notice how comments are preceded by '//'\n"
        "\n"
        "    // normal MVP transform\n"
        "    vec4 clipCoord = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "    gl_Position = clipCoord;\n"
        "\n"
        "    // Copy the primary color\n"
        "    gl_FrontColor = gl_Color;\n"
        "\n"
        "    // Calculate NDC\n"
        "    vec4 ndc = vec4(clipCoord.xyz, 0) / clipCoord.w;\n"
        "\n"
        "    // Map from [-1,1] to [0,1] before outputting\n"
        "    gl_FrontSecondaryColor = (ndc * 0.5) + 0.5;\n"
        "}\n";

    GLcharARB fsString[] = 
        "uniform float flickerFactor;\n"
        "\n"
        "void main(void)\n"
        "{\n"
        "    // Mix primary and secondary colors, 50/50\n"
        "    vec4 temp = mix(gl_Color, vec4(vec3(gl_SecondaryColor), 1.0), 0.5);\n"
        "\n"
        "    // Multiply by flicker factor\n"
        "    gl_FragColor = temp * flickerFactor;\n"
        "}\n";

    fprintf(stdout, "High-level Shaders Demo\n\n");

    // Make sure required functionality is available!
    if (!gltIsExtSupported("GL_ARB_vertex_shader") || 
        !gltIsExtSupported("GL_ARB_fragment_shader") ||
        !gltIsExtSupported("GL_ARB_shader_objects") ||
        !gltIsExtSupported("GL_ARB_shading_language_100"))
    {
        fprintf(stderr, "GLSL extensions not available!\n");
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

    glCreateShaderObjectARB = gltGetExtensionPointer("glCreateShaderObjectARB");
    glCreateProgramObjectARB = gltGetExtensionPointer("glCreateProgramObjectARB");
    glAttachObjectARB = gltGetExtensionPointer("glAttachObjectARB");
    glDetachObjectARB = gltGetExtensionPointer("glDetachObjectARB");
    glDeleteObjectARB = gltGetExtensionPointer("glDeleteObjectARB");
    glShaderSourceARB = gltGetExtensionPointer("glShaderSourceARB");
    glCompileShaderARB = gltGetExtensionPointer("glCompileShaderARB");
    glLinkProgramARB = gltGetExtensionPointer("glLinkProgramARB");
    glValidateProgramARB = gltGetExtensionPointer("glValidateProgramARB");
    glUseProgramObjectARB = gltGetExtensionPointer("glUseProgramObjectARB");
    glGetObjectParameterivARB = gltGetExtensionPointer("glGetObjectParameterivARB");
    glGetInfoLogARB = gltGetExtensionPointer("glGetInfoLogARB");
    glUniform1fARB = gltGetExtensionPointer("glUniform1fARB");
    glGetUniformLocationARB = gltGetExtensionPointer("glGetUniformLocationARB");

    if (gltIsExtSupported("GL_EXT_secondary_color"))
        glSecondaryColor3f = gltGetExtensionPointer("glSecondaryColor3fEXT");
    else
        glSecondaryColor3f = gltGetExtensionPointer("glSecondaryColor3f");

   // RG - For some testing on different drivers
    glGetAttachedShaders = gltGetExtensionPointer("glGetAttachedShaders");
    glIsShader = gltGetExtensionPointer("glIsShader");
    glDeleteShader = gltGetExtensionPointer("glDeleteShader");

    if (!glCreateShaderObjectARB || !glCreateProgramObjectARB || 
        !glAttachObjectARB || !glDetachObjectARB || !glDeleteObjectARB || 
        !glShaderSourceARB || !glCompileShaderARB || !glLinkProgramARB ||
        !glValidateProgramARB || !glUseProgramObjectARB || 
        !glGetObjectParameterivARB || !glGetInfoLogARB || 
        !glUniform1fARB || !glGetUniformLocationARB || 
        !glSecondaryColor3f || !glGetAttachedShaders || !glIsShader || !glDeleteShader)
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

    // Create shader objects and specify shader text
    vShader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    assert(glIsShader(vShader));

    fShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    assert(glIsShader(fShader));

    vsStringPtr[0] = vsString;
    glShaderSourceARB(vShader, 1, (const GLcharARB **)vsStringPtr, NULL);

    fsStringPtr[0] = fsString;
    glShaderSourceARB(fShader, 1, (const GLcharARB **)fsStringPtr, NULL);

    // Compile shaders and check for any errors
    glCompileShaderARB(vShader);
    glGetObjectParameterivARB(vShader, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if (!success)
    {
        GLbyte infoLog[MAX_INFO_LOG_SIZE];
        glGetInfoLogARB(vShader, MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
        fprintf(stderr, "Error in vertex shader compilation!\n");
        fprintf(stderr, "Info log: %s\n", infoLog);
        usleep(10000);
        exit(0);
    }

    glCompileShaderARB(fShader);
    glGetObjectParameterivARB(fShader, GL_OBJECT_COMPILE_STATUS_ARB, &success);
    if (!success)
    {
        GLbyte infoLog[MAX_INFO_LOG_SIZE];
        glGetInfoLogARB(fShader, MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
        fprintf(stderr, "Error in fragment shader compilation!\n");
        fprintf(stderr, "Info log: %s\n", infoLog);
        usleep(10000);
        exit(0);
    }

    // Create program object, attach shaders, then link
    progObj = glCreateProgramObjectARB();
    if (useVertexShader)
        glAttachObjectARB(progObj, vShader);

    if (useFragmentShader)
        glAttachObjectARB(progObj, fShader);

    Link(GL_TRUE);

#if 0
    glDeleteShader(vShader);
    assert(glIsShader(vShader));

    glDeleteShader(fShader);
    assert(glIsShader(fShader));

#if 0
    glDetachObjectARB(progObj, vShader);
    assert(!glIsShader(vShader));

    glDetachObjectARB(progObj, fShader);
    assert(!glIsShader(fShader));
#else
    glDeleteObjectARB(progObj);
    assert(glIsShader(vShader));
    assert(glIsShader(fShader));

    glUseProgramObjectARB(0);

    assert(!glIsShader(vShader));
    assert(!glIsShader(fShader));
#endif
#endif

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
            glAttachObjectARB(progObj, vShader);
        }
        else
        {
            glutChangeToMenuEntry(1, "Toggle vertex shader (currently OFF)", 1);
            glDetachObjectARB(progObj, vShader);
        }

        Relink();
        break;

    case 2:
        useFragmentShader = !useFragmentShader;
        if (useFragmentShader)
        {
            glutChangeToMenuEntry(2, "Toggle fragment shader (currently ON)", 2);
            glAttachObjectARB(progObj, fShader);
        }
        else
        {
            glutChangeToMenuEntry(2, "Toggle fragment shader (currently OFF)", 2);
            glDetachObjectARB(progObj, fShader);
        }

        Relink();
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
        }
        if (!doBlink && (flickerLocation != -1))
            glUniform1fARB(flickerLocation, 1.0f);
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
        break;
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
    glutCreateWindow("High-level Shaders Demo");
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

    if (glDeleteObjectARB)
    {
      glDeleteObjectARB(progObj);
      glDeleteObjectARB(fShader);
      glDeleteObjectARB(vShader);
    }

    return 0;
}






