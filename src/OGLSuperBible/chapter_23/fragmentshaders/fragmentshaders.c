// FragmentShaders.c
// OpenGL SuperBible, Chapter 23
// Demonstrates fragment shaders
// Program by Benjamin Lipchak

#include "../../common/openglsb.h"      // System and OpenGL Stuff
#include "../../common/gltools.h"       // System and OpenGL Stuff
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifdef APIENTRY
#undef APIENTRY
#endif // APIENTRY
#define APIENTRY


#define GL_TEXTURE0                                 0x84C0
#define GL_TEXTURE1                                 0x84C1
#define GL_TEXTURE2                                 0x84C2
#define GL_TEXTURE3                                 0x84C3

#define GL_FRAGMENT_PROGRAM_ARB                     0x8804
#define GL_PROGRAM_ERROR_POSITION_ARB               0x864B
#define GL_PROGRAM_ERROR_STRING_ARB                 0x8874
#define GL_PROGRAM_FORMAT_ASCII_ARB                 0x8875

#define GL_OBJECT_COMPILE_STATUS_ARB                0x8B81
#define GL_OBJECT_LINK_STATUS_ARB                   0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB               0x8B83
#define GL_FRAGMENT_SHADER_ARB                      0x8B30

typedef GLvoid (APIENTRY *PFNGLPROGRAMSTRINGARBPROC)(GLenum target, GLenum format, GLsizei len, const GLvoid *string);
typedef GLvoid (APIENTRY *PFNGLBINDPROGRAMARBPROC)(GLenum target, GLuint program);
typedef GLvoid (APIENTRY *PFNGLDELETEPROGRAMSARBPROC)(GLsizei n, const GLuint *programs);
typedef GLvoid (APIENTRY *PFNGLGENPROGRAMSARBPROC)(GLsizei n, GLuint *programs);
typedef GLvoid (APIENTRY *PFNGLPROGRAMLOCALPARAMETER4FARBPROC)(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef GLvoid (APIENTRY *PFNGLPROGRAMLOCALPARAMETER4FVARBPROC)(GLenum target, GLuint index, const GLfloat *params);

typedef char         GLcharARB;
typedef unsigned int GLhandleARB;

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
typedef GLvoid      (APIENTRY *PFNGLUNIFORM1IARBPROC)(GLint location,
                                                      GLint v0);
typedef GLvoid      (APIENTRY *PFNGLUNIFORM1FARBPROC)(GLint location,
                                                      GLfloat v0);
// typedef GLvoid      (APIENTRY *PFNGLUNIFORM3FVARBPROC)(GLint location,
//                                                        GLsizei count,
//                                                        GLfloat *value);
typedef GLint       (APIENTRY *PFNGLGETUNIFORMLOCATIONARBPROC)(GLhandleARB programObj,
                                                               const GLcharARB *name);

typedef GLvoid (APIENTRY *PFNGLACTIVETEXTUREPROC) (GLenum texture);

PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
PFNGLBINDPROGRAMARBPROC glBindProgramARB;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB;

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
PFNGLUNIFORM1FARBPROC glUniform1iARB;
PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;

GLboolean lowLevelAvailable = GL_FALSE;
GLboolean highLevelAvailable = GL_FALSE;
GLboolean useHighLevel = GL_FALSE;

#define SIMPLE        0
#define GRAYSCALE     1
#define SEPIA         2
#define HEATSIG       3
#define FOG           4
#define GRAYINVERT    5
#define COLORINVERT   6
#define TOTAL_SHADERS 7

GLuint ids[TOTAL_SHADERS];                                   // low-level shader object names
GLhandleARB fShader[TOTAL_SHADERS], progObj[TOTAL_SHADERS];  // high-level shader object handles
GLboolean needsValidation[TOTAL_SHADERS];
char *shaderNames[TOTAL_SHADERS] = {"simple", "grayscale", "sepia", "heatsig", "fog",
                                    "grayinvert", "colorinvert"};

GLint whichShader = SIMPLE;             // current shader

#define DENSITY_SLOT        0           // locations for low-level program local parameters

GLint windowWidth = 512;                // window size
GLint windowHeight = 512;

GLint mainMenu, shaderMenu;             // menu handles

GLint maxTexSize;                       // maximum allowed size for 1D/2D texture

GLfloat cameraPos[] = { 100.0f, 75.0f, 150.0f, 1.0f};
GLfloat lightPos[] = { 140.0f, 250.0f, 140.0f, 1.0f};
GLfloat ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f};
GLfloat diffuseLight[] = { 0.7f, 0.7f, 0.7f, 1.0f};
GLfloat specularMaterial[] = { 1.0f, 1.0f, 1.0f, 1.0f};

GLfloat fogColor[] = {0.5f, 0.8f, 0.5f, 1.0f};
GLfloat lightRotation = 0.0f;
GLfloat density = 1.0f;

#define MAX_INFO_LOG_SIZE 2048

// Create 1D texture to map luminance to heat signature
GLvoid CreateHeatSigMap(GLfloat r, GLfloat g, GLfloat b)
{
    GLfloat texels[512 * 4];
    GLint texSize = (maxTexSize > 512) ? 512 : maxTexSize;
    GLint x;
    GLfloat p;

    for (x = 0; x < texSize; x++)
    {
        p = ((double)x / (double)(texSize-1));

        // Gradient from black to blue to green to yellow to red
        if (p < 0.25f)
        {
            // black to blue
            p *= 4.0f;

            texels[x*4+0] = 0.0f;
            texels[x*4+1] = 0.0f;
            texels[x*4+2] = p;
        }
        else if (p < 0.5f)
        {
            // blue to green
            p -= 0.25f;
            p *= 4.0f;

            texels[x*4+0] = 0.0f;
            texels[x*4+1] = p;
            texels[x*4+2] = 1.0f - p;
        }
        else if (p < 0.75f)
        {
            // green to yellow
            p -= 0.5f;
            p *= 4.0f;

            texels[x*4+0] = p;
            texels[x*4+1] = 1.0f;
            texels[x*4+2] = 0.0f;
        }
        else
        {
            // yellow to red
            p -= 0.75f;
            p *= 4.0f;

            texels[x*4+0] = 1.0f;
            texels[x*4+1] = 1.0f - p;
            texels[x*4+2] = 0.0f;
        }
        texels[x*4+3] = 1.0f;
    }

    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, texSize, 0, GL_RGBA, GL_FLOAT, texels);
}

// Compile shaders
void PrepareShader(GLint shaderNum)
{
    char fullFileName[255];
    GLubyte *fsString;

    // Create low-level shader objects and specify shader text
    if (lowLevelAvailable)
    {
        GLint errorPos;

        sprintf(fullFileName, ".\\shaders\\%s.fp", shaderNames[shaderNum]);
        fsString = LoadShaderText(fullFileName);
        if (!fsString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[shaderNum]);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen((const char *)fsString), fsString);
        free(fsString);
        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
        if (errorPos != -1)
        {
            fprintf(stderr, "Error in low-level fragment shader #%d at position %d!\n", shaderNum, errorPos);
            fprintf(stderr, "Error string: %s\n", glGetString(GL_PROGRAM_ERROR_STRING_ARB));
            usleep(5000);
            exit(0);
        }
    }

    // Create high-level shader objects and specify shader text
    if (highLevelAvailable)
    {
        GLcharARB *fsStringPtr[1];
        GLint success;

        sprintf(fullFileName, ".\\shaders\\%s.fs", shaderNames[shaderNum]);
        fsString = LoadShaderText(fullFileName);
        if (!fsString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        fShader[shaderNum] = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
        fsStringPtr[0] = (GLcharARB *)fsString;
        glShaderSourceARB(fShader[shaderNum], 1, (const GLcharARB **)fsStringPtr, NULL);
        free(fsString);

        // Compile shaders and check for any errors
        glCompileShaderARB(fShader[shaderNum]);
        glGetObjectParameterivARB(fShader[shaderNum], GL_OBJECT_COMPILE_STATUS_ARB, &success);
        if (!success)
        {
            GLbyte infoLog[MAX_INFO_LOG_SIZE];
            glGetInfoLogARB(fShader[shaderNum], MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
            fprintf(stderr, "Error in high-level fragment shader #%d compilation!\n", shaderNum);
            fprintf(stderr, "Info log: %s\n", infoLog);
            usleep(10000);
            exit(0);
        }

        // Create program object, attach shader, then link
        progObj[shaderNum] = glCreateProgramObjectARB();
        glAttachObjectARB(progObj[shaderNum], fShader[shaderNum]);

        glLinkProgramARB(progObj[shaderNum]);
        glGetObjectParameterivARB(progObj[shaderNum], GL_OBJECT_LINK_STATUS_ARB, &success);
        if (!success)
        {
            GLbyte infoLog[MAX_INFO_LOG_SIZE];
            glGetInfoLogARB(progObj[shaderNum], MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
            fprintf(stderr, "Error in high-level program #%d linkage!\n", shaderNum);
            fprintf(stderr, "Info log: %s\n", infoLog);
            usleep(10000);
            exit(0);
        }

        // Program object has changed, so we should revalidate
        needsValidation[shaderNum] = GL_TRUE;
    }
}

// Called to draw scene objects
void DrawModels(void)
{
    GLTVector3 lightPosEye;
    GLTMatrix mv;

    // Transform light position to eye space
    glPushMatrix();
    glRotatef(lightRotation, 0.0, 1.0, 0.0);
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    gltTransformPoint(lightPos, mv, lightPosEye);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glPopMatrix();

    if (useHighLevel)
    {
        GLint uniformLoc = glGetUniformLocationARB(progObj[whichShader], "sampler0");
        if (uniformLoc != -1)
        {
            glUniform1iARB(uniformLoc, 0);
        }
        uniformLoc = glGetUniformLocationARB(progObj[whichShader], "density");
        if (uniformLoc != -1)
        {
            glUniform1fARB(uniformLoc, density);
        }
    }
    else
    {
        if (whichShader == FOG)
        {
            glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, DENSITY_SLOT, density, 0.0f, 0.0f, 0.0f);
        }
    }

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

    // Draw magenta torus
    glColor3f(1.0f, 0.0f, 1.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 60.0f);
    glutSolidTorus(8.0f, 16.0f, 50, 50);
    glPopMatrix();

    // Draw yellow cone
    glColor3f(1.0f, 1.0f, 0.0f);
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(60.0f, 0.0f, -24.0f);
    glutSolidCone(25.0f, 50.0f, 50, 50);
    glPopMatrix();

    // Draw cyan teapot
    glColor3f(0.0f, 1.0f, 1.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -60.0f);
    glutSolidTeapot(25.0f);
    glPopMatrix();
}

// Called to draw scene
void RenderScene(void)
{
    GLenum glErrorCode = GL_NO_ERROR;
    // Track camera angle
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 1.0f, 1.0f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(cameraPos[0], cameraPos[1], cameraPos[2], 
              0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    
    if (whichShader == FOG)
    {
        // Use a green-gray color for fog
        glClearColor(fogColor[0], fogColor[1], fogColor[2], fogColor[3]);
    }
    else if ((whichShader == GRAYINVERT) || (whichShader == COLORINVERT))
    {
        // Use white for color inversion
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    }
    
    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Validate our shader before first use
    if (needsValidation[whichShader])
    {
        GLint success;

        glValidateProgramARB(progObj[whichShader]);
        glGetObjectParameterivARB(progObj[whichShader], GL_OBJECT_VALIDATE_STATUS_ARB, &success);
        if (!success)
        {
            GLbyte infoLog[MAX_INFO_LOG_SIZE];
            glGetInfoLogARB(progObj[whichShader], MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
            fprintf(stderr, "Error in program #%d validation!\n", whichShader);
            fprintf(stderr, "Info log: %s\n", infoLog);
            usleep(10000);
            exit(0);
        }

        needsValidation[whichShader] = GL_FALSE;
    }
    
    // Draw objects in the scene
    DrawModels();
    
    glErrorCode = glGetError();
    if (glErrorCode != GL_NO_ERROR)
        fprintf(stderr, "GL Error! (%d)\n", glErrorCode);

    // Flush drawing commands
    glutSwapBuffers();
}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
{
    const GLubyte *version;
    GLint i;

    fprintf(stdout, "Fragment Shaders Demo\n\n");

    // Make sure required functionality is available!
    if (gltIsExtSupported("GL_ARB_fragment_shader") &&
        gltIsExtSupported("GL_ARB_shader_objects") &&
        gltIsExtSupported("GL_ARB_shading_language_100"))
    {
        highLevelAvailable = GL_TRUE;
    }
    if (gltIsExtSupported("GL_ARB_fragment_program"))
    {
        lowLevelAvailable = GL_TRUE;
    }
    if (!lowLevelAvailable && !highLevelAvailable)
    {
        fprintf(stderr, "Neither fragment shader"
                        " extension is available!\n");
        usleep(2000);
        exit(0);
    }

    // Make sure we have multitexture and cube maps!
    version = glGetString(GL_VERSION);
    if (((version[0] < '1') || 
         (version[1] != '.') || 
         ((version[0] == '1') && (version[2] < '3')) || 
         (version[2] > '9') ))    // 1.3 - 9.9 
    {
        fprintf(stderr, "OpenGL 1.3 or better is not available!\n");
        usleep(2000);
        exit(0);
    }

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

    if (lowLevelAvailable)
    {
        glGenProgramsARB = gltGetExtensionPointer("glGenProgramsARB");
        glBindProgramARB = gltGetExtensionPointer("glBindProgramARB");
        glProgramStringARB = gltGetExtensionPointer("glProgramStringARB");
        glDeleteProgramsARB = gltGetExtensionPointer("glDeleteProgramsARB");
        glProgramLocalParameter4fARB = gltGetExtensionPointer("glProgramLocalParameter4fARB");
        glProgramLocalParameter4fvARB = gltGetExtensionPointer("glProgramLocalParameter4fvARB");

        if (!glGenProgramsARB || !glBindProgramARB ||
            !glProgramStringARB || !glDeleteProgramsARB ||
            !glProgramLocalParameter4fvARB || 
            !glProgramLocalParameter4fARB)
        {
            fprintf(stderr, "Not all entrypoints were available!\n");
            usleep(2000);
            exit(0);
        }
    }

    if (highLevelAvailable)
    {
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
        glUniform1iARB = gltGetExtensionPointer("glUniform1iARB");
        glUniform3fvARB = gltGetExtensionPointer("glUniform3fvARB");
        glGetUniformLocationARB = gltGetExtensionPointer("glGetUniformLocationARB");

        if (!glCreateShaderObjectARB || !glCreateProgramObjectARB || 
            !glAttachObjectARB || !glDetachObjectARB || !glDeleteObjectARB || 
            !glShaderSourceARB || !glCompileShaderARB || !glLinkProgramARB ||
            !glValidateProgramARB || !glUseProgramObjectARB || 
            !glGetObjectParameterivARB || !glGetInfoLogARB || 
            !glUniform3fvARB || !glUniform1fARB || 
            !glUniform1iARB || !glGetUniformLocationARB)
        {
            fprintf(stderr, "Not all entrypoints were available!\n");
            usleep(2000);
            exit(0);
        }

        useHighLevel = GL_TRUE;
    }

    fprintf(stdout, "Controls:\n");
    fprintf(stdout, "\tRight-click for menu\n\n");
    fprintf(stdout, "\tR/L arrows\t+/- fog density for \"fog\" shader\n");
    fprintf(stdout, "\tR/L arrows\t+/- rotate lights for others shaders\n\n");
    fprintf(stdout, "\tx/X\t\tMove +/- in x direction\n");
    fprintf(stdout, "\ty/Y\t\tMove +/- in y direction\n");
    fprintf(stdout, "\tz/Z\t\tMove +/- in z direction\n\n");
    fprintf(stdout, "\tq\t\tExit demo\n\n");
    
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );

    // Misc. state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularMaterial);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 128.0f);

    // Texture state
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, 1);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    CreateHeatSigMap(1.0, 1.0, 1.0);

    if (lowLevelAvailable)
    {
        glGenProgramsARB(TOTAL_SHADERS, ids);
        // Low-level will always be enabled, but high-level 
        // will take precedence if they're both enabled
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    }

    // Load and compile low- and high-level shaders
    for (i = 0; i < TOTAL_SHADERS; i++)
    {
        PrepareShader(i);
    }

    // Install first shader
    if (lowLevelAvailable)
    {
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[0]);
    }
    if (useHighLevel)
    {
        glUseProgramObjectARB(progObj[whichShader]);
    }
}

void ProcessMenu(int value)
{
    switch(value)
    {
    case 1:
        useHighLevel = !useHighLevel;
        if (useHighLevel)
        {
            glutChangeToMenuEntry(2, "Switch to low-level fragment shader", 1);
            glUseProgramObjectARB(progObj[whichShader]);
        }
        else
        {
            glutChangeToMenuEntry(2, "Switch to high-level fragment shader", 1);
            glUseProgramObjectARB(0);
        }
        break;

    default:
        whichShader = value - 2;
        {
            char menuEntry[128];
            sprintf(menuEntry, "Choose fragment shader (currently \"%s\")", shaderNames[whichShader]);
            glutSetMenu(mainMenu);
            glutChangeToSubMenu(1, menuEntry, shaderMenu);
        }
        if (lowLevelAvailable)
        {
            glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, ids[whichShader]);
        }
        if (useHighLevel)
        {
            glUseProgramObjectARB(progObj[whichShader]);
        }
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
        if (whichShader == FOG)
            density -= 0.1f;
        else
            lightRotation -= 5.0f;
        break;
    case GLUT_KEY_RIGHT:
        if (whichShader == FOG)
            density += 0.1f;
        else
            lightRotation += 5.0f;
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

    if (density < 0.0f)
        density = 0.0f;

    // Refresh the Window
    glutPostRedisplay();
}

void ChangeSize(int w, int h)
{
    windowWidth = w;
    windowHeight = h;
    glViewport(0, 0, windowWidth, windowHeight);
}

int main(int argc, char* argv[])
{
    GLint i;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Fragment Shaders Demo");
    glutReshapeFunc(ChangeSize);
    glutKeyboardFunc(KeyPressFunc);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);

    SetupRC();

    // Create the menus
    shaderMenu = glutCreateMenu(ProcessMenu);
    for (i = 0; i < TOTAL_SHADERS; i++)
    {
        char menuEntry[128];
        sprintf(menuEntry, "\"%s\"", shaderNames[i]);
        glutAddMenuEntry(menuEntry, 2+i);
    }

    mainMenu = glutCreateMenu(ProcessMenu);
    {
        char menuEntry[128];
        sprintf(menuEntry, "Choose fragment shader (currently \"%s\")", shaderNames[0]);
        glutAddSubMenu(menuEntry, shaderMenu);
    }
    if (lowLevelAvailable && highLevelAvailable)
    {
        if (useHighLevel)
        {
            glutAddMenuEntry("Switch to low-level fragment shader", 1);
        }
        else
        {
            glutAddMenuEntry("Switch to high-level fragment shader", 1);
        }
    }
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    glutMainLoop();

    if (glDeleteObjectARB)
    {
        for (i = 0; i < TOTAL_SHADERS; i++)
        {
            glDeleteObjectARB(progObj[i]);
            glDeleteObjectARB(fShader[i]);
        }
    }

    return 0;
}
