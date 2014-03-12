// VertexShaders.c
// OpenGL SuperBible, Chapter 22
// Demonstrates vertex shaders
// Program by Benjamin Lipchak

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define GL_TEXTURE0                                 0x84C0
#define GL_TEXTURE1                                 0x84C1
#define GL_TEXTURE2                                 0x84C2
#define GL_TEXTURE3                                 0x84C3

#define GL_COLOR_SUM                                0x8458

#define GL_FOG_COORD_SRC                            0x8450
#define GL_FOG_COORD                                0x8451

#ifdef APIENTRY
#undef APIENTRY
#endif // APIENTRY
#define APIENTRY

#ifdef RAND_MAX
#undef RAND_MAX
#endif // RAND_MAX
#define RAND_MAX                                                     0x7FFF

#define GL_VERTEX_PROGRAM_ARB                       0x8620
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB            0x8642
#define GL_PROGRAM_ERROR_POSITION_ARB               0x864B
#define GL_PROGRAM_ERROR_STRING_ARB                 0x8874
#define GL_PROGRAM_FORMAT_ASCII_ARB                 0x8875

#define GL_OBJECT_COMPILE_STATUS_ARB                0x8B81
#define GL_OBJECT_LINK_STATUS_ARB                   0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB               0x8B83
#define GL_VERTEX_SHADER_ARB                        0x8B31

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
PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;

//PFNGLACTIVETEXTUREPROC glActiveTexture;

GLboolean lowLevelAvailable = GL_FALSE;
GLboolean highLevelAvailable = GL_FALSE;
GLboolean useHighLevel = GL_FALSE;

#define SIMPLE        0
#define DIFFUSE       1
#define SPECULAR      2
#define SEPSPEC       3
#define TEXSPEC       4
#define THREELIGHTS   5
#define FOGCOORD      6
#define FOG           7
#define PTSIZE        8
#define STRETCH       9
#define TOTAL_SHADERS 10

GLuint ids[TOTAL_SHADERS];                                   // low-level shader object names
GLhandleARB vShader[TOTAL_SHADERS], progObj[TOTAL_SHADERS];  // high-level shader object handles
GLboolean needsValidation[TOTAL_SHADERS];
char *shaderNames[TOTAL_SHADERS] = {"simple", "diffuse", "specular", "sepspec", "texspec", 
                                    "3lights", "fogcoord", "fog", "ptsize", "stretch"};

GLint whichShader = SIMPLE;             // current shader

#define LIGHT0_POS_SLOT     0           // locations for low-level program local parameters
#define LIGHT1_POS_SLOT     1
#define LIGHT2_POS_SLOT     2
#define SQUASH_STRETCH_SLOT 1
#define DENSITY_SLOT        1

GLint windowWidth = 512;                // window size
GLint windowHeight = 512;

GLint mainMenu, shaderMenu;             // menu handles

GLint maxTexSize;                       // maximum allowed size for 1D/2D texture

GLfloat cameraPos[] = { 100.0f, 75.0f, 150.0f, 1.0f};
GLfloat lightPos0[] = { 140.0f, 250.0f, 140.0f, 1.0f};
GLfloat lightPos1[] = { -140.0f, 250.0f, 140.0f, 1.0f};
GLfloat lightPos2[] = { 0.0f, 250.0f, -200.0f, 1.0f};

GLfloat squashStretch[] = {1.0f, 1.5f, 0.75f, 1.0f};
GLfloat fogColor[] = {0.5f, 0.8f, 0.5f, 1.0f};
GLfloat lightRotation = 0.0f;
GLfloat density = 0.005f;

#define MAX_INFO_LOG_SIZE 2048

// Create 1D texture to map NdotH to NdotH^128
GLvoid CreatePowMap(GLfloat r, GLfloat g, GLfloat b)
{
    GLfloat texels[512 * 4];
    GLint texSize = (maxTexSize > 512) ? 512 : maxTexSize;
    GLint x;

    for (x = 0; x < texSize; x++)
    {
        // Incoming N.H has been scaled by 8 and biased by -7 to take better
        // advantage of the texture space.  Otherwise, the texture will be
        // entirely zeros until ~7/8 of the way into it.  This way, we expand
        // the useful 1/8 of the range and get better precision.
        texels[x*4+0] = r * pow(((double)x / (double)(texSize-1)) * 0.125f + 0.875f, 128.0);
        texels[x*4+1] = g * pow(((double)x / (double)(texSize-1)) * 0.125f + 0.875f, 128.0);
        texels[x*4+2] = b * pow(((double)x / (double)(texSize-1)) * 0.125f + 0.875f, 128.0);
        texels[x*4+3] = 1.0f;
    }
    // Make sure the first texel is exactly zero.  Most
    // incoming texcoords will clamp to this texel.
    texels[0] = texels[1] = texels[2] = 0.0f;

    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, texSize, 0, GL_RGBA, GL_FLOAT, texels);
}

// Compile shaders
void PrepareShader(GLint shaderNum)
{
    char fullFileName[255];
    GLubyte *vsString;

    // Create low-level shader objects and specify shader text
    if (lowLevelAvailable)
    {
        GLint errorPos;

        sprintf(fullFileName, ".\\shaders\\%s.vp", shaderNames[shaderNum]);
        vsString = LoadShaderText(fullFileName);
        if (!vsString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[shaderNum]);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen((const char *)vsString), vsString);
        free(vsString);
        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
        if (errorPos != -1)
        {
            fprintf(stderr, "Error in low-level vertex shader #%d at position %d!\n", shaderNum, errorPos);
            fprintf(stderr, "Error string: %s\n", glGetString(GL_PROGRAM_ERROR_STRING_ARB));
            usleep(5000);
            exit(0);
        }
    }

    // Create high-level shader objects and specify shader text
    if (highLevelAvailable)
    {
        GLcharARB *vsStringPtr[1];
        GLint success;

        sprintf(fullFileName, ".\\shaders\\%s.vs", shaderNames[shaderNum]);
        vsString = LoadShaderText(fullFileName);
        if (!vsString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        vShader[shaderNum] = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        vsStringPtr[0] = (GLcharARB *)vsString;
        glShaderSourceARB(vShader[shaderNum], 1, (const GLcharARB **)vsStringPtr, NULL);
        free(vsString);

        // Compile shaders and check for any errors
        glCompileShaderARB(vShader[shaderNum]);
        glGetObjectParameterivARB(vShader[shaderNum], GL_OBJECT_COMPILE_STATUS_ARB, &success);
        if (!success)
        {
            GLbyte infoLog[MAX_INFO_LOG_SIZE];
            glGetInfoLogARB(vShader[shaderNum], MAX_INFO_LOG_SIZE, NULL, (GLcharARB *)infoLog);
            fprintf(stderr, "Error in high-level vertex shader #%d compilation!\n", shaderNum);
            fprintf(stderr, "Info log: %s\n", infoLog);
            usleep(10000);
            exit(0);
        }

        // Create program object, attach shader, then link
        progObj[shaderNum] = glCreateProgramObjectARB();
        glAttachObjectARB(progObj[shaderNum], vShader[shaderNum]);

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
    GLTVector3 lightPosEye0, lightPosEye1, lightPosEye2;
    GLTMatrix mv;

    // Transform light position to eye space
    glPushMatrix();
    glRotatef(lightRotation, 0.0, 1.0, 0.0);
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    gltTransformPoint(lightPos0, mv, lightPosEye0);
    if (whichShader == THREELIGHTS)
    {
        gltTransformPoint(lightPos1, mv, lightPosEye1);
        gltTransformPoint(lightPos2, mv, lightPosEye2);
    }
    glPopMatrix();

    if (useHighLevel)
    {
        GLint uniformLoc = glGetUniformLocationARB(progObj[whichShader], "lightPos0");
        if (uniformLoc != -1)
        {
            glUniform3fvARB(uniformLoc, 1, lightPosEye0);
        }
        uniformLoc = glGetUniformLocationARB(progObj[whichShader], "lightPos1");
        if (uniformLoc != -1)
        {
            glUniform3fvARB(uniformLoc, 1, lightPosEye1);
        }
        uniformLoc = glGetUniformLocationARB(progObj[whichShader], "lightPos2");
        if (uniformLoc != -1)
        {
            glUniform3fvARB(uniformLoc, 1, lightPosEye2);
        }
        uniformLoc = glGetUniformLocationARB(progObj[whichShader], "squashStretch");
        if (uniformLoc != -1)
        {
            glUniform3fvARB(uniformLoc, 1, squashStretch);
        }
        uniformLoc = glGetUniformLocationARB(progObj[whichShader], "density");
        if (uniformLoc != -1)
        {
            glUniform1fARB(uniformLoc, density);
        }
    }
    else
    {
        glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, LIGHT0_POS_SLOT, lightPosEye0);
        if (whichShader == THREELIGHTS)
        {
            glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, LIGHT1_POS_SLOT, lightPosEye1);
            glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, LIGHT2_POS_SLOT, lightPosEye2);
        } else if (whichShader == STRETCH)
        {
            glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, SQUASH_STRETCH_SLOT, squashStretch);
        } else if (whichShader == FOG)
        {
            glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, DENSITY_SLOT, density, 0.0f, 0.0f, 0.0f);
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

    if (whichShader == STRETCH)
    {
        // Cone and teapot are rotated such that their
        // Y and Z squash scales must be switched
        GLfloat rotatedSquashStretch[4];

        rotatedSquashStretch[0] = squashStretch[0];
        rotatedSquashStretch[1] = squashStretch[2];
        rotatedSquashStretch[2] = squashStretch[1];
        rotatedSquashStretch[3] = squashStretch[3];

        if (useHighLevel)
        {
            GLint uniformLoc = glGetUniformLocationARB(progObj[whichShader], "squashStretch");
            if (uniformLoc != -1)
            {
                glUniform3fvARB(uniformLoc, 1, rotatedSquashStretch);
            }
        }
        else
        {
            glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, SQUASH_STRETCH_SLOT, rotatedSquashStretch);
        }
    }

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
    glViewport(0, 0, windowWidth, windowHeight);

    if ((whichShader == FOGCOORD) || (whichShader == FOG))
    {
        // Use a green-gray color for fog
        glClearColor(fogColor[0], fogColor[1], fogColor[2], fogColor[3]);
        glFogf(GL_FOG_DENSITY, density);
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

    fprintf(stdout, "Vertex Shaders Demo\n\n");

    // Make sure required functionality is available!
    if (gltIsExtSupported("GL_ARB_vertex_shader") &&
        gltIsExtSupported("GL_ARB_shader_objects") &&
        gltIsExtSupported("GL_ARB_shading_language_100"))
    {
        highLevelAvailable = GL_TRUE;
    }
    if (gltIsExtSupported("GL_ARB_vertex_program"))
    {
        lowLevelAvailable = GL_TRUE;
    }
    if (!lowLevelAvailable && !highLevelAvailable)
    {
        fprintf(stderr, "Neither vertex shader"
                        " extension is available!\n");
        usleep(2000);
        exit(0);
    }

    // Make sure we have multitexture, cube maps, and texenv add!
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
/*
    glActiveTexture = gltGetExtensionPointer("glActiveTexture");
    if (!glActiveTexture)
    {
        glActiveTexture = gltGetExtensionPointer("glActiveTextureARB");
    }
    if (!glActiveTexture)
    {
        fprintf(stderr, "Not all entrypoints were available!\n");
        usleep(2000);
        exit(0);
    }
*/
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
        glUniform3fvARB = gltGetExtensionPointer("glUniform3fvARB");
        glGetUniformLocationARB = gltGetExtensionPointer("glGetUniformLocationARB");

        if (!glCreateShaderObjectARB || !glCreateProgramObjectARB || 
            !glAttachObjectARB || !glDetachObjectARB || !glDeleteObjectARB || 
            !glShaderSourceARB || !glCompileShaderARB || !glLinkProgramARB ||
            !glValidateProgramARB || !glUseProgramObjectARB || 
            !glGetObjectParameterivARB || !glGetInfoLogARB || 
            !glUniform3fvARB || !glUniform1fARB || !glGetUniformLocationARB)
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
    fprintf(stdout, "\tx/X\t\tMove +/- in x direction, stretch for \"stretch\" shader\n");
    fprintf(stdout, "\ty/Y\t\tMove +/- in y direction, stretch for \"stretch\" shader\n");
    fprintf(stdout, "\tz/Z\t\tMove +/- in z direction, stretch for \"stretch\" shader\n\n");
    fprintf(stdout, "\tq\t\tExit demo\n\n");
    
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );

    // Misc. state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Texture state
    glActiveTexture(GL_TEXTURE3);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    glBindTexture(GL_TEXTURE_1D, 3+1);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    CreatePowMap(0.25, 0.25, 1.0);
    glActiveTexture(GL_TEXTURE2);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    glBindTexture(GL_TEXTURE_1D, 2+1);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    CreatePowMap(0.25, 1.0, 0.25);
    glActiveTexture(GL_TEXTURE1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    glBindTexture(GL_TEXTURE_1D, 1+1);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    CreatePowMap(1.0, 0.25, 0.25);
    glActiveTexture(GL_TEXTURE0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    glBindTexture(GL_TEXTURE_1D, 0+1);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    CreatePowMap(1.0, 1.0, 1.0);

    if (lowLevelAvailable)
    {
        glGenProgramsARB(TOTAL_SHADERS, ids);
        // Low-level will always be enabled, but high-level 
        // will take precedence if they're both enabled
        glEnable(GL_VERTEX_PROGRAM_ARB);
    }

    // Load and compile low- and high-level shaders
    for (i = 0; i < TOTAL_SHADERS; i++)
    {
        PrepareShader(i);
    }

    // Install first shader
    if (lowLevelAvailable)
    {
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[0]);
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
            glutChangeToMenuEntry(2, "Switch to low-level vertex shader", 1);
            glUseProgramObjectARB(progObj[whichShader]);
        }
        else
        {
            glutChangeToMenuEntry(2, "Switch to high-level vertex shader", 1);
            glUseProgramObjectARB(0);
        }
        break;

    default:
        whichShader = value - 2;
        {
            char menuEntry[128];
            sprintf(menuEntry, "Choose vertex shader (currently \"%s\")", shaderNames[whichShader]);
            glutSetMenu(mainMenu);
            glutChangeToSubMenu(1, menuEntry, shaderMenu);
        }
        if (lowLevelAvailable)
        {
            glBindProgramARB(GL_VERTEX_PROGRAM_ARB, ids[whichShader]);
        }
        if (useHighLevel)
        {
            glUseProgramObjectARB(progObj[whichShader]);
        }
        if (whichShader == SEPSPEC)
        {
            // Separate specular shader needs
            // primary and secondary colors summed
            glEnable(GL_COLOR_SUM);
        }
        else
        {
            glDisable(GL_COLOR_SUM);
        }
        if (whichShader == FOGCOORD)
        {
            // Fogcoord shader needs fixed
            // functionality fog enabled
            glEnable(GL_FOG);
        }
        else
        {
            glDisable(GL_FOG);
        }
        if (whichShader == PTSIZE)
        {
            // Fogcoord shader needs fixed
            // functionality fog enabled
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);
            glEnable(GL_POINT_SMOOTH);
            glEnable(GL_BLEND);
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);
            glDisable(GL_POINT_SMOOTH);
            glDisable(GL_BLEND);
        }
        // Disable all texturing, then reenable as needed
        glActiveTexture(GL_TEXTURE3);
        glDisable(GL_TEXTURE_1D);
        glActiveTexture(GL_TEXTURE2);
        glDisable(GL_TEXTURE_1D);
        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_1D);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_TEXTURE_1D);
        if ((whichShader == TEXSPEC) || (whichShader == STRETCH))
        {
            // texture specular shaders needs 1D pow map
            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_TEXTURE_1D);
        }
        else if (whichShader == THREELIGHTS)
        {
            // 3 lights shader needs 1D pow map on all 3 units
            glActiveTexture(GL_TEXTURE3);
            glEnable(GL_TEXTURE_1D);
            glActiveTexture(GL_TEXTURE2);
            glEnable(GL_TEXTURE_1D);
            glActiveTexture(GL_TEXTURE1);
            glEnable(GL_TEXTURE_1D);
            glActiveTexture(GL_TEXTURE0);
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
        if (whichShader == STRETCH)
            squashStretch[0] += 0.01f;
        else
            cameraPos[0] += 5.0f;
        break;
    case 'X':
        if (whichShader == STRETCH)
            squashStretch[0] -= 0.01f;
        else
            cameraPos[0] -= 5.0f;
        break;
    case 'y':
        if (whichShader == STRETCH)
            squashStretch[1] += 0.01f;
        else
            cameraPos[1] += 5.0f;
        break;
    case 'Y':
        if (whichShader == STRETCH)
            squashStretch[1] -= 0.01f;
        else
            cameraPos[1] -= 5.0f;
        break;
    case 'z':
        if (whichShader == STRETCH)
            squashStretch[2] += 0.01f;
        else
            cameraPos[2] += 5.0f;
        break;
    case 'Z':
        if (whichShader == STRETCH)
            squashStretch[2] -= 0.01f;
        else
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
        if ((whichShader == FOGCOORD) || 
            (whichShader == FOG))
            density -= 0.0005f;
        else
            lightRotation -= 5.0f;
        break;
    case GLUT_KEY_RIGHT:
        if ((whichShader == FOGCOORD) || 
            (whichShader == FOG))
            density += 0.0005f;
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
}

int main(int argc, char* argv[])
{
    GLint i;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Vertex Shaders Demo");
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
        sprintf(menuEntry, "Choose vertex shader (currently \"%s\")", shaderNames[0]);
        glutAddSubMenu(menuEntry, shaderMenu);
    }
    if (lowLevelAvailable && highLevelAvailable)
    {
        if (useHighLevel)
        {
            glutAddMenuEntry("Switch to low-level vertex shader", 1);
        }
        else
        {
            glutAddMenuEntry("Switch to high-level vertex shader", 1);
        }
    }
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    glutMainLoop();

    if (glDeleteObjectARB)
    {
        for (i = 0; i < TOTAL_SHADERS; i++)
        {
            glDeleteObjectARB(progObj[i]);
            glDeleteObjectARB(vShader[i]);
        }
    }

    return 0;
}
