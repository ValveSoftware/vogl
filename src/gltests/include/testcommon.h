#include <stdio.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#include <assert.h>

void CompileShader(GLuint shader, char* shaderName)
{
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == 0)
    {   
        GLint len = 0;
        char* msg = NULL;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        msg = (char*) malloc(len);
        glGetShaderInfoLog(shader, len, &len, msg);
        printf("%s shader compilation failed: \n%s\n", shaderName, msg);
        free(msg);
    }    
}

void LinkProgram(GLuint program, char* programName)
{
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == 0)
    {
        GLint len = 0;
        char* msg = NULL;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        msg = (char*) malloc(len);
        glGetProgramInfoLog(program, len, &len, msg);
        printf("%s program failed to link: \n%s\n", programName, msg);
        free(msg);
    } 
}

