#include <stdio.h>
#include "gltools.h"
#include <string.h>
#include <unistd.h>

// Load shader from disk into a null-terminated string
GLubyte *LoadShaderText(const char *fileName)
{
    GLubyte *shaderText = NULL;
    GLint shaderLength = 0;
    FILE *fp;

    fp = fopen(fileName, "r");

#ifdef __linux__
    if (fp == NULL)
    {
        char buf[1024];

        // Get name of exe.
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf));
        if (len > 0)
        {
            // Get basename of exe.
            buf[len] = 0;
            char *bname = strrchr(buf, '/');

            // These things are being passed in as "shaders\blah.vs" - so search for both \ and /.
            char *fname = strrchr(fileName, '/');
            if (!fname)
                fname = strrchr(fileName, '\\');
            if (bname && fname) 
            {
                char filename[1024];

                snprintf(filename, sizeof(filename), "%s_shaders/%s", bname + 1, fname + 1);
                filename[sizeof(filename) - 1] = 0;

                fp = fopen(filename, "r");
                if (!fp)
                    fprintf(stderr, "ERROR: LoadShaderText(%s) failed.\n", filename);
            }
        }
    }
#endif

    if (fp == NULL)
    {
        fprintf(stderr, "ERROR: LoadShaderText(%s) failed.\n", fileName);
        return NULL;
    }

    while (fgetc(fp) != EOF)
    {
        shaderLength++;
    }
    rewind(fp);
    shaderText = (GLubyte *)malloc(shaderLength+1);
    if (shaderText != NULL)
    {
        fread(shaderText, 1, shaderLength, fp);
    }
    shaderText[shaderLength] = '\0';
    fclose(fp);

    return shaderText;
}

