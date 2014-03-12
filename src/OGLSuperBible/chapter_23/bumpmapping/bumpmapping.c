// BumpMapping.c
// OpenGL SuperBible, Chapter 23
// Demonstrates shader-based bump mapping
// Program by Benjamin Lipchak

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#ifdef APIENTRY
#undef APIENTRY
#endif // APIENTRY
#define APIENTRY

#define GL_TEXTURE0                                 0x84C0
#define GL_TEXTURE1                                 0x84C1
#define GL_TEXTURE2                                 0x84C2
#define GL_TEXTURE3                                 0x84C3

#define GL_GENERATE_MIPMAP                          0x8191

#define GL_VERTEX_PROGRAM_ARB                       0x8620
#define GL_FRAGMENT_PROGRAM_ARB                     0x8804
#define GL_PROGRAM_ERROR_POSITION_ARB               0x864B
#define GL_PROGRAM_ERROR_STRING_ARB                 0x8874
#define GL_PROGRAM_FORMAT_ASCII_ARB                 0x8875

#define GL_OBJECT_COMPILE_STATUS_ARB                0x8B81
#define GL_OBJECT_LINK_STATUS_ARB                   0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB               0x8B83
#define GL_VERTEX_SHADER_ARB                        0x8B31
#define GL_FRAGMENT_SHADER_ARB                      0x8B30

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
//typedef GLvoid      (APIENTRY *PFNGLUNIFORM3FVARBPROC)(GLint location,
//                                                       GLsizei count,
//                                                       GLfloat *value);
typedef GLint       (APIENTRY *PFNGLGETUNIFORMLOCATIONARBPROC)(GLhandleARB programObj,
                                                               const GLcharARB *name);

typedef GLvoid (APIENTRY *PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef GLvoid (APIENTRY *PFNGLMULTITEXCOORD2FPROC) (GLenum texture, GLfloat s, GLfloat t);
typedef GLvoid (APIENTRY *PFNGLMULTITEXCOORD3FPROC) (GLenum texture, GLfloat s, GLfloat t, GLfloat r);

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

#define BUMPMAP           0
#define SHOWBUMP          1
#define TOTAL_SHADER_SETS 2

#define BOX               0
#define CYLINDER          1
#define TORUS             2
#define TOTAL_SHAPES      3

#define RIVETS            0
#define PYRAMIDS          1
#define TOTAL_BUMPMAPS    2

GLuint v_ids[TOTAL_SHADER_SETS];        // low-level shader object names
GLuint f_ids[TOTAL_SHADER_SETS];
GLhandleARB fShader[TOTAL_SHADER_SETS]; // high-level shader object handles
GLhandleARB vShader[TOTAL_SHADER_SETS]; 
GLhandleARB progObj[TOTAL_SHADER_SETS]; 
GLboolean needsValidation[TOTAL_SHADER_SETS];
char *shaderNames[TOTAL_SHADER_SETS] = {"bumpmap", "showbump"};
char *shapeNames[TOTAL_SHAPES] = {"box", "cylinder", "torus"};
char *bumpmapNames[TOTAL_BUMPMAPS] = {"rivets", "pyramids"};

GLint whichShader = BUMPMAP;            // current shader
GLint whichShape = BOX;                 // current shape
GLint whichBumpmap = RIVETS;            // current bumpmap

#define LIGHTPOS0_SLOT  0               // locations for low-level program local parameters

GLint windowWidth = 512;                // window size
GLint windowHeight = 512;

GLint mainMenu, shapeMenu, shaderMenu, bumpmapMenu;

GLint maxTexSize;                       // maximum allowed size for 1D/2D texture

GLfloat cameraPos[] = { 0.0f, 125.0f, -200.0f, 1.0f};
GLfloat lightPos0[3];

GLfloat lightRotation = 90.0f;

#define MAX_INFO_LOG_SIZE 2048

// 2D raised rivets and divets
GLvoid CreateRivetMap()
{
    GLfloat texels[128 * 128 * 4];
    GLint texSize = (maxTexSize > 128) ? 128 : maxTexSize;
    GLfloat x, y, radius, nx, ny, nz, scale;
    GLint u, v;

    for (v = 0; v < texSize; v++)
    {
        y = ((GLfloat)v - (((GLfloat)texSize - 1.0f) * 0.5f)) / (((GLfloat)texSize - 1.0f) * 0.5f);

        for (u = 0; u < texSize; u++)
        {
            x = ((GLfloat)u - (((GLfloat)texSize - 1.0f) * 0.5f)) / (((GLfloat)texSize - 1.0f) * 0.5f);
            radius = (x*x) + (y*y);

            if (radius <= 0.64f)
            {
                nx = x * 1.25f;
                ny = y * 1.25f;
                nz = sqrt(0.64 - radius) * 1.25f;
                scale = 1.0f / sqrt((nx*nx) + (ny*ny) + (nz*nz));

                texels[(v*texSize*4)+(u*4)+0] = (nx * scale * 0.5f) + 0.5f;
                texels[(v*texSize*4)+(u*4)+1] = (ny * scale * 0.5f) + 0.5f;
                texels[(v*texSize*4)+(u*4)+2] = (nz * scale * 0.5f) + 0.5f;
                texels[(v*texSize*4)+(u*4)+3] = 1.0f;
            }
            else
            {
                texels[(v*texSize*4)+(u*4)+0] = 0.0f;
                texels[(v*texSize*4)+(u*4)+1] = 0.0f;
                texels[(v*texSize*4)+(u*4)+2] = 1.0f;
                texels[(v*texSize*4)+(u*4)+3] = 1.0f;
            }
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texels);
}

// 2D raised pyramids
GLvoid CreatePyramidMap()
{
    GLfloat texels[128 * 128 * 4];
    GLint texSize = (maxTexSize > 128) ? 128 : maxTexSize;
    GLfloat x, y, nx, ny, nz, scale;
    GLint u, v;

    for (v = 0; v < texSize; v++)
    {
        y = ((GLfloat)v - (((GLfloat)texSize - 1.0f) * 0.5f)) / (((GLfloat)texSize - 1.0f) * 0.5f);

        for (u = 0; u < texSize; u++)
        {
            x = ((GLfloat)u - (((GLfloat)texSize - 1.0f) * 0.5f)) / (((GLfloat)texSize - 1.0f) * 0.5f);

            if ((fabs(x)+fabs(y)) <= 0.8f)
            {
                if ((x >= 0) && (y >= 0))
                {
                    nx = 0.75f;
                    ny = 0.75f;
                }
                else if ((x < 0) && (y >= 0))
                {
                    nx = -0.75f;
                    ny = 0.75f;
                }
                else if ((x >= 0) && (y < 0))
                {
                    nx = 0.75f;
                    ny = -0.75f;
                }
                else
                {
                    nx = -0.75f;
                    ny = -0.75f;
                }
                nz = 1.0f;
                scale = 1.0f / sqrt((nx*nx) + (ny*ny) + (nz*nz));

                texels[(v*texSize*4)+(u*4)+0] = (nx * scale * 0.5f) + 0.5f;
                texels[(v*texSize*4)+(u*4)+1] = (ny * scale * 0.5f) + 0.5f;
                texels[(v*texSize*4)+(u*4)+2] = (nz * scale * 0.5f) + 0.5f;
                texels[(v*texSize*4)+(u*4)+3] = 1.0f;
            }
            else
            {
                texels[(v*texSize*4)+(u*4)+0] = 0.0f;
                texels[(v*texSize*4)+(u*4)+1] = 0.0f;
                texels[(v*texSize*4)+(u*4)+2] = 1.0f;
                texels[(v*texSize*4)+(u*4)+3] = 1.0f;
            }
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texels);
}

// Compile shaders
void PrepareShader(GLint shaderNum)
{
    char fullFileName[255];
    GLubyte *shString;

    // Create low-level shader objects and specify shader text
    if (lowLevelAvailable)
    {
        GLint errorPos;

        sprintf(fullFileName, ".\\shaders\\%s.vp", shaderNames[shaderNum]);
        shString = LoadShaderText(fullFileName);
        if (!shString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, v_ids[shaderNum]);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen((char *)shString), shString);
        free(shString);
        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
        if (errorPos != -1)
        {
            fprintf(stderr, "Error in low-level vertex shader #%d at position %d!\n", shaderNum, errorPos);
            fprintf(stderr, "Error string: %s\n", glGetString(GL_PROGRAM_ERROR_STRING_ARB));
            usleep(5000);
            exit(0);
        }

        sprintf(fullFileName, ".\\shaders\\%s.fp", shaderNames[shaderNum]);
        shString = LoadShaderText(fullFileName);
        if (!shString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, f_ids[shaderNum]);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen((char *)shString), shString);
        free(shString);
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
        GLcharARB *shStringPtr[1];
        GLint success;

        sprintf(fullFileName, ".\\shaders\\%s.vs", shaderNames[shaderNum]);
        shString = LoadShaderText(fullFileName);
        if (!shString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        vShader[shaderNum] = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        shStringPtr[0] = (GLcharARB *) shString;
        glShaderSourceARB(vShader[shaderNum], 1, (const GLcharARB **)shStringPtr, NULL);
        free(shString);

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

        sprintf(fullFileName, ".\\shaders\\%s.fs", shaderNames[shaderNum]);
        shString = LoadShaderText(fullFileName);
        if (!shString)
        {
            fprintf(stderr, "Unable to load \"%s\"\n", fullFileName);
            usleep(5000);
            exit(0);
        }
        fShader[shaderNum] = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
        shStringPtr[0] = (GLcharARB *)shString;
        glShaderSourceARB(fShader[shaderNum], 1, (const GLcharARB **)shStringPtr, NULL);
        free(shString);

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
        glAttachObjectARB(progObj[shaderNum], vShader[shaderNum]);
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

void DrawCylinder(GLfloat radius, GLfloat height, GLint slices, GLfloat xTexScale, GLfloat yTexScale)
{
    GLfloat angleInc = (2.0f * 3.14159265f) / (GLfloat)slices;
    GLint i;

    glBegin(GL_QUAD_STRIP);

    for (i = 0; i <= slices; i++)
    {
        glMultiTexCoord2f(GL_TEXTURE0, xTexScale * i / (GLfloat)slices, 0.0f);
        glMultiTexCoord3f(GL_TEXTURE1, cos((angleInc * i) + (3.14159265f * 0.5f)), 0.0f, sin((angleInc * i) + (3.14159265f * 0.5f)));  // tangent
        glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 1.0f, 0.0f);                           // binormal
        glMultiTexCoord3f(GL_TEXTURE3, cos(angleInc * i), 0.0f, sin(angleInc * i)); // normal
        glVertex3f(radius * cos(angleInc * i), -height*0.5f, radius * sin(angleInc * i));
        glMultiTexCoord2f(GL_TEXTURE0, xTexScale * i / (GLfloat)slices, yTexScale);
        glVertex3f(radius * cos(angleInc * i), height*0.5f, radius * sin(angleInc * i));
    }

    glEnd();

    glBegin(GL_TRIANGLE_FAN);

    glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
    glMultiTexCoord3f(GL_TEXTURE1, 1.0f, 0.0f, 0.0f); // tangent
    glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 0.0f, 1.0f); // binormal
    glMultiTexCoord3f(GL_TEXTURE3, 0.0f, 1.0f, 0.0f); // normal
    glVertex3f(0.0f, height*0.5f, 0.0f);

    for (i = 0; i <= slices; i++)
    {
        glMultiTexCoord2f(GL_TEXTURE0, xTexScale * cos(angleInc * i) * 0.159155f, 
                                       xTexScale * sin(angleInc * i) * 0.159155f);
        glVertex3f(radius * cos(angleInc * -i), height*0.5f, radius * sin(angleInc * -i));
    }

    glEnd();

    glBegin(GL_TRIANGLE_FAN);

    glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
    glMultiTexCoord3f(GL_TEXTURE1, -1.0f, 0.0f, 0.0f); // tangent
    glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 0.0f, -1.0f); // binormal
    glMultiTexCoord3f(GL_TEXTURE3, 0.0f, -1.0f, 0.0f); // normal
    glVertex3f(0.0f, -height*0.5f, 0.0f);

    for (i = 0; i <= slices; i++)
    {
        glMultiTexCoord2f(GL_TEXTURE0, xTexScale * cos(angleInc * i) * 0.159155f, 
                                       xTexScale * sin(angleInc * i) * 0.159155f);
        glVertex3f(radius * cos(angleInc * i), -height*0.5f, radius * sin(angleInc * i));
    }

    glEnd();
}

void DrawTorus(GLfloat innerRadius, GLfloat ringRadius, GLint rings, GLint slices, GLfloat xTexScale, GLfloat yTexScale)
{
    GLfloat sliceAngleInc = (2.0f * 3.14159265f) / (GLfloat)slices;
    GLfloat ringAngleInc = (2.0f * 3.14159265f) / (GLfloat)rings;
    GLint i, j;

    for (i = 0; i <= rings; i++)
    {
        glBegin(GL_QUAD_STRIP);

        for (j = 0; j <= slices; j++)
        {
            glMultiTexCoord2f(GL_TEXTURE0, xTexScale * (i+1) / (GLfloat)rings, yTexScale * j / (GLfloat)slices);
            glMultiTexCoord3f(GL_TEXTURE1, cos((ringAngleInc * (i+1)) + (3.14159265f * 0.5f)), 0.0f, sin((ringAngleInc * (i+1)) + (3.14159265f * 0.5f)));  // tangent
            glMultiTexCoord3f(GL_TEXTURE2, 0.0f, sin((sliceAngleInc * j) + (3.14159265f * 0.5f)), 0.0f);  // binormal
            glMultiTexCoord3f(GL_TEXTURE3, cos(ringAngleInc * (i+1)), sin(sliceAngleInc * j), sin(ringAngleInc * (i+1))); // normal
            glVertex3f((innerRadius + ringRadius + (ringRadius * cos(sliceAngleInc * j))) * cos(ringAngleInc * (i+1)), (ringRadius * sin(sliceAngleInc * j)), (innerRadius + ringRadius + (ringRadius * cos(sliceAngleInc * j))) * sin(ringAngleInc * (i+1)));
            glMultiTexCoord2f(GL_TEXTURE0, xTexScale * i / (GLfloat)rings, yTexScale * j / (GLfloat)slices);
            glMultiTexCoord3f(GL_TEXTURE1, cos((ringAngleInc * i) + (3.14159265f * 0.5f)), 0.0f, sin((ringAngleInc * i) + (3.14159265f * 0.5f)));  // tangent
            glMultiTexCoord3f(GL_TEXTURE2, 0.0f, sin((sliceAngleInc * j) + (3.14159265f * 0.5f)), 0.0f);  // binormal
            glMultiTexCoord3f(GL_TEXTURE3, cos(ringAngleInc * i), sin(sliceAngleInc * j), sin(ringAngleInc * i)); // normal
            glVertex3f((innerRadius + ringRadius + (ringRadius * cos(sliceAngleInc * j))) * cos(ringAngleInc * i), (ringRadius * sin(sliceAngleInc * j)), (innerRadius + ringRadius + (ringRadius * cos(sliceAngleInc * j))) * sin(ringAngleInc * i));
        }

        glEnd();
    }
}

void DrawBox(GLfloat size, GLfloat texScale)
{
    size *= 0.5f;

    glBegin(GL_QUADS);
        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
        glMultiTexCoord3f(GL_TEXTURE1, 1.0f, 0.0f, 0.0f);  // tangent
        glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 1.0f, 0.0f);  // binormal
        glMultiTexCoord3f(GL_TEXTURE3, 0.0f, 0.0f, -1.0f); // normal
        glVertex3f(-size, -size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, texScale);
        glVertex3f(-size, size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, texScale);
        glVertex3f(size, size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, 0.0f);
        glVertex3f(size, -size, -size);

        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
        glMultiTexCoord3f(GL_TEXTURE1, -1.0f, 0.0f, 0.0f);// tangent
        glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 1.0f, 0.0f); // binormal
        glMultiTexCoord3f(GL_TEXTURE3, 0.0f, 0.0f, 1.0f); // normal
        glVertex3f(size, -size, size);
        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, texScale);
        glVertex3f(size, size, size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, texScale);
        glVertex3f(-size, size, size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, 0.0f);
        glVertex3f(-size, -size, size);

        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
        glMultiTexCoord3f(GL_TEXTURE1, 0.0f, 0.0f, 1.0f); // tangent
        glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 1.0f, 0.0f); // binormal
        glMultiTexCoord3f(GL_TEXTURE3, 1.0f, 0.0f, 0.0f); // normal
        glVertex3f(size, -size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, texScale);
        glVertex3f(size, size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, texScale);
        glVertex3f(size, size, size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, 0.0f);
        glVertex3f(size, -size, size);

        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
        glMultiTexCoord3f(GL_TEXTURE1, 0.0f, 0.0f, -1.0f); // tangent
        glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 1.0f, 0.0f);  // binormal
        glMultiTexCoord3f(GL_TEXTURE3, -1.0f, 0.0f, 0.0f); // normal
        glVertex3f(-size, -size, size);
        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, texScale);
        glVertex3f(-size, size, size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, texScale);
        glVertex3f(-size, size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, 0.0f);
        glVertex3f(-size, -size, -size);

        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
        glMultiTexCoord3f(GL_TEXTURE1, 1.0f, 0.0f, 0.0f); // tangent
        glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 0.0f, 1.0f); // binormal
        glMultiTexCoord3f(GL_TEXTURE3, 0.0f, 1.0f, 0.0f); // normal
        glVertex3f(-size, size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, texScale);
        glVertex3f(-size, size, size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, texScale);
        glVertex3f(size, size, size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, 0.0f);
        glVertex3f(size, size, -size);

        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, 0.0f);
        glMultiTexCoord3f(GL_TEXTURE1, 1.0f, 0.0f, 0.0f);  // tangent
        glMultiTexCoord3f(GL_TEXTURE2, 0.0f, 0.0f, -1.0f); // binormal
        glMultiTexCoord3f(GL_TEXTURE3, 0.0f, -1.0f, 0.0f); // normal
        glVertex3f(-size, -size, size);
        glMultiTexCoord2f(GL_TEXTURE0, 0.0f, texScale);
        glVertex3f(-size, -size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, texScale);
        glVertex3f(size, -size, -size);
        glMultiTexCoord2f(GL_TEXTURE0, texScale, 0.0f);
        glVertex3f(size, -size, size);
    glEnd();
}

// Called to draw scene objects
void DrawModels(void)
{
    if (useHighLevel)
    {
        GLint uniformLoc = glGetUniformLocationARB(progObj[whichShader], "sampler0");
        if (uniformLoc != -1)
        {
            glUniform1iARB(uniformLoc, 0);
        }
        uniformLoc = glGetUniformLocationARB(progObj[whichShader], "lightPos0");
        if (uniformLoc != -1)
        {
            glUniform3fvARB(uniformLoc, 1, lightPos0);
        }
    }
    else
    {
        glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, LIGHTPOS0_SLOT, lightPos0);
    }

    // Draw box to demonstrate low-geometry, high-detail
    glColor3f(0.8f, 0.5f, 0.3f);
    switch (whichShape)
    {
    case CYLINDER:
        DrawCylinder(25.0f, 70.0f, 25, 20.0f, 10.0f);
        break;
    case TORUS:
        DrawTorus(20.0f, 15.0f, 50, 25, 40.0f, 12.0f);
        break;
    default:
        DrawBox(70.0f, 10.0f);
        break;
    }
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
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    
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

    fprintf(stdout, "Bump Mapping Demo\n\n");

    // Make sure required functionality is available!
    if (gltIsExtSupported("GL_ARB_fragment_shader") &&
        gltIsExtSupported("GL_ARB_vertex_shader") &&
        gltIsExtSupported("GL_ARB_shader_objects") &&
        gltIsExtSupported("GL_ARB_shading_language_100"))
    {
        highLevelAvailable = GL_TRUE;
    }
    if (gltIsExtSupported("GL_ARB_fragment_program") &&
        gltIsExtSupported("GL_ARB_vertex_program"))
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

    // Make sure we have multitexture!
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
    fprintf(stdout, "\tR/L arrows\t+/- rotate lights\n\n");
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
    glEnable(GL_CULL_FACE);

    // Texture state
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0 + 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    CreateRivetMap();
    glBindTexture(GL_TEXTURE_2D, 1 + 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    CreatePyramidMap();
    glBindTexture(GL_TEXTURE_2D, 0 + 1);

    lightPos0[0] = 300.0f * cos(-lightRotation * 3.14159265f / 180.0f);
    lightPos0[1] = 0.0f;
    lightPos0[2] = 300.0f * sin(-lightRotation * 3.14159265f / 180.0f);

    if (lowLevelAvailable)
    {
        glGenProgramsARB(TOTAL_SHADER_SETS, v_ids);
        glGenProgramsARB(TOTAL_SHADER_SETS, f_ids);
        // Low-level will always be enabled, but high-level 
        // will take precedence if they're both enabled
        glEnable(GL_VERTEX_PROGRAM_ARB);
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
    }

    // Load and compile low- and high-level shaders
    for (i = 0; i < TOTAL_SHADER_SETS; i++)
    {
        PrepareShader(i);
    }

    // Install first shader
    if (lowLevelAvailable)
    {
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, v_ids[0]);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, f_ids[0]);
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
            glutChangeToMenuEntry(4, "Switch to low-level shaders", 1);
            glUseProgramObjectARB(progObj[whichShader]);
        }
        else
        {
            glutChangeToMenuEntry(4, "Switch to high-level shaders", 1);
            glUseProgramObjectARB(0);
        }
        break;

    default:
        if (value >= (TOTAL_SHAPES+TOTAL_SHADER_SETS+2))
        {
            char menuEntry[128];
            whichBumpmap = value - TOTAL_SHAPES - TOTAL_SHADER_SETS - 2;
            sprintf(menuEntry, "Choose bumpmap (currently \"%s\")", bumpmapNames[whichBumpmap]);
            glutSetMenu(mainMenu);
            glutChangeToSubMenu(3, menuEntry, bumpmapMenu);
            glBindTexture(GL_TEXTURE_2D, whichBumpmap + 1);
        }
        else if (value >= (TOTAL_SHAPES+2))
        {
            char menuEntry[128];
            whichShader = value - TOTAL_SHAPES - 2;
            sprintf(menuEntry, "Choose shader (currently \"%s\")", shaderNames[whichShader]);
            glutSetMenu(mainMenu);
            glutChangeToSubMenu(2, menuEntry, shaderMenu);
            if (lowLevelAvailable)
            {
                glBindProgramARB(GL_VERTEX_PROGRAM_ARB, v_ids[whichShader]);
                glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, f_ids[whichShader]);
            }
            if (useHighLevel)
            {
                glUseProgramObjectARB(progObj[whichShader]);
            }
        }
        else
        {
            char menuEntry[128];
            whichShape = value - 2;
            sprintf(menuEntry, "Choose shape (currently \"%s\")", shapeNames[whichShape]);
            glutSetMenu(mainMenu);
            glutChangeToSubMenu(1, menuEntry, shapeMenu);
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
        lightRotation -= 5.0f;
        break;
    case GLUT_KEY_RIGHT:
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

    lightPos0[0] = 300.0f * cos(-lightRotation * 3.14159265f / 180.0f);
    lightPos0[1] = 0.0f;
    lightPos0[2] = 300.0f * sin(-lightRotation * 3.14159265f / 180.0f);

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
    glutCreateWindow("Bump Mapping Demo");
    glutReshapeFunc(ChangeSize);
    glutKeyboardFunc(KeyPressFunc);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);

    SetupRC();

    // Create the menus
    shapeMenu = glutCreateMenu(ProcessMenu);
    for (i = 0; i < TOTAL_SHAPES; i++)
    {
        char menuEntry[128];
        sprintf(menuEntry, "\"%s\"", shapeNames[i]);
        glutAddMenuEntry(menuEntry, 2+i);
    }

    shaderMenu = glutCreateMenu(ProcessMenu);
    for (i = 0; i < TOTAL_SHADER_SETS; i++)
    {
        char menuEntry[128];
        sprintf(menuEntry, "\"%s\"", shaderNames[i]);
        glutAddMenuEntry(menuEntry, 2+TOTAL_SHAPES+i);
    }

    bumpmapMenu = glutCreateMenu(ProcessMenu);
    for (i = 0; i < TOTAL_BUMPMAPS; i++)
    {
        char menuEntry[128];
        sprintf(menuEntry, "\"%s\"", bumpmapNames[i]);
        glutAddMenuEntry(menuEntry, 2+TOTAL_SHAPES+TOTAL_SHADER_SETS+i);
    }

    mainMenu = glutCreateMenu(ProcessMenu);
    {
        char menuEntry[128];
        sprintf(menuEntry, "Choose shape (currently \"%s\")", shapeNames[0]);
        glutAddSubMenu(menuEntry, shapeMenu);
        sprintf(menuEntry, "Choose shader (currently \"%s\")", shaderNames[0]);
        glutAddSubMenu(menuEntry, shaderMenu);
        sprintf(menuEntry, "Choose bumpmap (currently \"%s\")", bumpmapNames[0]);
        glutAddSubMenu(menuEntry, bumpmapMenu);
    }
    if (lowLevelAvailable && highLevelAvailable)
    {
        if (useHighLevel)
        {
            glutAddMenuEntry("Switch to low-level shaders", 1);
        }
        else
        {
            glutAddMenuEntry("Switch to high-level shaders", 1);
        }
    }
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    glutMainLoop();

    if (glDeleteObjectARB)
    {
        for (i = 0; i < TOTAL_SHADER_SETS; i++)
        {
            glDeleteObjectARB(progObj[i]);
            glDeleteObjectARB(vShader[i]);
            glDeleteObjectARB(fShader[i]);
        }
    }

    return 0;
}
