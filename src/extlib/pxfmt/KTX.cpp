// Put this line before #include's to eliminate warnings about fopen():
#define _CRT_SECURE_NO_WARNINGS

#include "KTX.h"
#include "pxfmt.h"


// The purpose of this function is to read a shader file, based on a filename,
// and return a pointer to malloc'd memory that contains the "char*" contents
// of the file.
static GLchar *readShaderFile(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    long len;
    char *buf;

    if (!fp) {
        printf("failed to open %s\n", filename);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (char *) malloc(len + 1);
    if (fread(buf, 1, len, fp) !=len) {
        printf("read error\n");
        exit(1);
    }
    buf[len] = '\0';

    fclose(fp);

    return (GLchar *) buf;

} // readShaderFile()



// The purpose of this function is to read 2 shader files, based on a filename,
// create a GLSL program, compile and link each shader, and attach those
// shaders to the GLSL program.
static void createCompileAndLinkProgramFromFiles(const char *vsFilename,
                                                 const char *fsFilename,
                                                 GLuint &prog)
{
    GLuint sh;
    GLchar *source, buf[100000];
    GLsizei len;
    GLint val;

    sh = glCreateShader(GL_VERTEX_SHADER);
    source = readShaderFile(vsFilename);
    glShaderSource(sh, 1, (const GLchar **) &source, NULL);
    free(source);

    glCompileShader(sh);

    glGetShaderiv(sh, GL_COMPILE_STATUS, &val);
    if (!val) {
        glGetShaderInfoLog(sh, sizeof(buf), &len, buf);
        printf("failed to compile Vertex Shader:\n%s\n", buf);
        exit(1);
    }

    prog = glCreateProgram();
    glAttachShader(prog, sh);


    sh = glCreateShader(GL_FRAGMENT_SHADER);
    source = readShaderFile(fsFilename);
    glShaderSource(sh, 1, (const GLchar **) &source, NULL);
    free(source);

    glCompileShader(sh);

    glGetShaderiv(sh, GL_COMPILE_STATUS, &val);
    if (!val) {
        glGetShaderInfoLog(sh, sizeof(buf), &len, buf);
        printf("failed to compile Fragment Shader:\n%s\n", buf);
        exit(1);
    }

    glAttachShader(prog, sh);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &val);
    if (!val) {
        glGetProgramInfoLog(prog, sizeof(buf), &len, buf);
        printf("failed to link program: %s\n", buf);
        exit(1);
    }

} // createCompileAndLinkProgramFromFiles()


#ifdef WIN32
struct timezone {int dummy;};
int gettimeofday(struct timeval* tp, void* tzp)
{
    LARGE_INTEGER t, f;

    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    tp->tv_sec = t.QuadPart/f.QuadPart;
    tp->tv_usec = (t.QuadPart-(f.QuadPart*tp->tv_sec))*1000*1000/f.QuadPart;
    return 0;
}
#endif // WIN32









/**
 * Repeatedly calls the DoDrawPixels() method, while timing how long it takes.
 */
float PixelOps::BenchmarkDrawPixels(int nFrames, bool swapBuffersBetweenDraws)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    struct timeval start, end;
    struct timezone tz;
    gettimeofday(&start, &tz); /* start timing */
    int distance = 1;
    GLint leftEdge = 0;
    GLint rightEdge = imgWidth - 1;
    int i = 0, frame = 0;

    float rate = 0.0;

    for (frame = 0 ; frame < nFrames ; i++, frame++) {
        // Draw the read-image on the right-half of the window:
        DoDrawPixels(leftEdge, 0);

        if (swapBuffersBetweenDraws) {
            glutSwapBuffers();
            glFinish();
        }

        if (i < 128) {
            leftEdge  += distance;
            rightEdge += distance;
        } else if (i <= 256) {
            leftEdge  -= distance;
            rightEdge -= distance;
            if (i == 256) {
                i = -1;
            }
        }
    }


    // Calculate the rate of frames/glDrawPixels() per second:
    if (!swapBuffersBetweenDraws) {
        glFinish();
    }
    gettimeofday(&end, &tz); /* finish timing */
    rate = frame / ((end.tv_sec-start.tv_sec) +
                    (end.tv_usec-start.tv_usec)/1000000.0);


    // Do a final buffer-swap if we weren't doing them in the loop above:
    if (!swapBuffersBetweenDraws) {
        glutSwapBuffers();
        glFinish();
    }

    return rate;
} // PixelOps::BenchmarkDrawPixels()


/**
 * Draw a square test-image on the left-half of the window.
 */
void PixelOps::DrawTestImageOnLeftSideOfWindow()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (drawTestImageProgram == 0)
    {
        createCompileAndLinkProgramFromFiles(VERTEX_SHADER_FILE,
                                             FRAGMENT_SHADER_FILE,
                                             drawTestImageProgram);
    }

    glViewport(0, 0, width, height);

    glUseProgram(drawTestImageProgram);
    getProg1UniformLocations();

    glUniform1i(uniformLocation_passThru, GL_FALSE);
    glUniform1i(uniformLocation_leftHalf, GL_TRUE);

    glBegin(GL_TRIANGLES);
    glVertex3f(-1.0f, -1.0f, 0.0f);
    glVertex3f(-1.0f,  1.0f, 0.5f);
    glVertex3f(0.0f,   1.0f, 0.25f);

    glVertex3f(-1.0f, -1.0f, 0.0f);
    glVertex3f(0.0f,   1.0f, 0.25f);
    glVertex3f(0.0f,  -1.0f, 0.5f);
    glEnd();

    glutSwapBuffers();
    glFinish();

    glUseProgram(0);
} // PixelOps::DrawTestImageOnLeftSideOfWindow()


/**
 * Common part of preparing for glDrawPixels() calls.
 *
 * Draw an image on the left-half of the window, then do a glReadPixels() in
 * order to get a copy with the desired format and type combination.
 */
bool PixelOps::CommonDrawPixelsSetup(FormatTypeCombo *pComboToUse)
{
    // Save the desired format and type info for later:
    pCombo = pComboToUse;

    // Allocate memory for the pixel data:
    imgSize = imgWidth * imgHeight * pCombo->perPixelSizeInBytes;
    pOriginalPixels = (unsigned int *) calloc(imgSize, sizeof(char));
    if (pOriginalPixels == NULL) {
        return false;
    }

    // Draw a square image on the left-hand side of the window:
    DrawTestImageOnLeftSideOfWindow();

    // Now, read the left-side image.
    glReadPixels(0, 0, imgWidth, imgHeight,
                 pCombo->format, pCombo->type, pOriginalPixels);

    // Make a copy of the image:
    pPixels = (unsigned int *) calloc(imgSize, sizeof(char));
    if (pPixels == NULL) {
        return false;
    }
    memcpy(pPixels, pOriginalPixels, imgSize);

    if ((pCombo->format == GL_LUMINANCE) ||
        (pCombo->format == GL_LUMINANCE_ALPHA)) {
        // Read the left-side image again, this time as RGBA/FLOAT:
        GLuint tmpImgSize = imgWidth * imgHeight * 16;
        pTexSubImage2DPixels =
            (unsigned int *) calloc(tmpImgSize, sizeof(char));
        if (pTexSubImage2DPixels == NULL) {
            return false;
        }
        glReadPixels(0, 0, imgWidth, imgHeight,
                     GL_RGBA, GL_FLOAT, pTexSubImage2DPixels);

        // To help with debugging, show pOriginalPixels on the right-hand side
        // of the window:
        glWindowPos2i(imgWidth, 0);
        glDrawPixels(imgWidth, imgHeight, pCombo->format, pCombo->type,
                     pOriginalPixels);
        glutSwapBuffers();
        glFinish();
    } else {
        // Simply make another copy of the image:
        pTexSubImage2DPixels = (unsigned int *) calloc(imgSize, sizeof(char));
        if (pTexSubImage2DPixels == NULL) {
            return false;
        }
        memcpy(pTexSubImage2DPixels, pOriginalPixels, imgSize);
    }

    return true;
} // PixelOps::CommonDrawPixelsSetup()


/**
 * Draw a square test-image on the left-half of the window.
 */
void PixelOps::getProg1UniformLocations()
{
    uniformLocation_passThru = glGetUniformLocation(drawTestImageProgram,
                                                    "passThru");
    uniformLocation_leftHalf = glGetUniformLocation(drawTestImageProgram,
                                                    "leftHalf");
} // PixelOps::getProg1UniformLocations()








/**
 * Prepare for glDrawPixels() calls.
 *
 * Draw an image on the left-half of the window, then do a glReadPixels() in
 * order to get a copy with the desired format and type combination.
 */
bool RealPixelOps::PrepareForDrawPixels(FormatTypeCombo *pComboToUse)
{
    // Do the common setup:
    if (!CommonDrawPixelsSetup(pComboToUse)) {
        return false;
    }

    return true;
} // RealPixelOps::PrepareForDrawPixels()


/**
 * Actually call glDrawPixels().
 *
 * Just before hand, call glWindowPos2i().
 */
void RealPixelOps::DoDrawPixels(GLint x, GLint y)
{
    // Draw the read-image on the right-half of the window:
    glWindowPos2i(x, y);
    glDrawPixels(imgWidth, imgHeight, pCombo->format, pCombo->type, pPixels);
} // RealPixelOps::DoDrawPixels()


/**
 * Test the call to glDrawPixels().
 */
bool RealPixelOps::TestDrawPixels()
{
    // Draw the read-image on the right-half of the window:
    DoDrawPixels(imgWidth, 0);
    glutSwapBuffers();
    glFinish();

    // Now, read the image back from the window:
    glReadPixels(imgWidth, 0, imgWidth, imgHeight, pCombo->format, pCombo->type,
                 pPixels);

    // Finally, compare the read-back image with the originally-read-back image:
    if (memcmp(pOriginalPixels, pPixels, imgSize) == 0) {
        return true;
    } else {
        return false;
    }
} // RealPixelOps::TestDrawPixels()


/**
 * Do any cleanup after one or more calls to glDrawPixels().
 *
 * Just free the temporarily-allocated memory for the pixel data.
 */
void RealPixelOps::FinishDrawPixels()
{
    free(pOriginalPixels);
    free(pPixels);
    if (pTexSubImage2DPixels) {
        free(pTexSubImage2DPixels);
        pTexSubImage2DPixels = NULL;
    }
} // RealPixelOps::FinishDrawPixels()








/**
 * Prepare for glDrawPixels() calls.
 *
 * Draw an image on the left-half of the window, then do a glReadPixels() in
 * order to get a copy with the desired format and type combination.
 */
bool EmulatedPixelOps::PrepareForDrawPixels(FormatTypeCombo *pComboToUse)
{
    // When testing INTEGER formats, work-around the fact that the framebuffer
    // is non-integer by temporarily changing the format to be non-integer:
    GLuint savedFormat;
    bool didSaveFormat = false;
    switch (pComboToUse->format) {
    case GL_RED_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_RED;
        didSaveFormat = true;
        break;
    case GL_GREEN_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_GREEN;
        didSaveFormat = true;
        break;
    case GL_BLUE_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_BLUE;
        didSaveFormat = true;
        break;
    case GL_ALPHA_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_ALPHA;
        didSaveFormat = true;
        break;
    case GL_RG_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_RG;
        didSaveFormat = true;
        break;
    case GL_RGB_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_RGB;
        didSaveFormat = true;
        break;
    case GL_RGBA_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_RGBA;
        didSaveFormat = true;
        break;
    case GL_BGRA_INTEGER:
        savedFormat = pComboToUse->format;
        pComboToUse->format = GL_BGRA;
        didSaveFormat = true;
        break;
    default:
        break;
    }

    // Do the common setup:
    if (!CommonDrawPixelsSetup(pComboToUse)) {
        return false;
    }

    if (didSaveFormat) {
        pComboToUse->format = savedFormat;
    }

    return true;
} // EmulatedPixelOps::PrepareForDrawPixels()


/**
 * Actually call glDrawPixels().
 *
 * Just before hand, call glWindowPos2i().
 */
void EmulatedPixelOps::DoDrawPixels(GLint x, GLint y)
{
    // When testing INTEGER formats, work-around the fact that the framebuffer
    // is non-integer by temporarily changing the format to be non-integer:
    GLuint savedFormat;
    bool didSaveFormat = false;
    switch (pCombo->format) {
    case GL_RED_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_RED;
        didSaveFormat = true;
        break;
    case GL_GREEN_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_GREEN;
        didSaveFormat = true;
        break;
    case GL_BLUE_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_BLUE;
        didSaveFormat = true;
        break;
    case GL_ALPHA_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_ALPHA;
        didSaveFormat = true;
        break;
    case GL_RG_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_RG;
        didSaveFormat = true;
        break;
    case GL_RGB_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_RGB;
        didSaveFormat = true;
        break;
    case GL_RGBA_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_RGBA;
        didSaveFormat = true;
        break;
    case GL_BGRA_INTEGER:
        savedFormat = pCombo->format;
        pCombo->format = GL_BGRA;
        didSaveFormat = true;
        break;
    default:
        break;
    }

    // Draw the read-image on the right-half of the window:
    glWindowPos2i(x, y);
    glDrawPixels(imgWidth, imgHeight, pCombo->format, pCombo->type, pPixels);

    if (didSaveFormat) {
        pCombo->format = savedFormat;
    }
} // EmulatedPixelOps::DoDrawPixels()


/**
 * Test the call to glDrawPixels().
 */
bool EmulatedPixelOps::TestDrawPixels()
{
    pxfmt_sized_format fmt = validate_format_type_combo(pCombo->format,
                                                        pCombo->type);
    if (fmt != PXFMT_INVALID) {
        if (((pCombo->format == GL_RGBA) || (pCombo->format == GL_BGRA)) &&
            (pCombo->testSwizzleType != TestSwizzle_NONE))
        {
            pxfmt_sized_format alt_fmt =
                validate_format_type_combo(((pCombo->format == GL_RGBA) ?
                                            GL_BGRA : GL_RGBA), pCombo->type);
            if (alt_fmt == PXFMT_INVALID) {
                printf("\t INVALID SWIZZLING ATTEMPT\n");
                fflush(stdout);
                return false;
            }
            if (pxfmt_convert_pixels(pPixels, pOriginalPixels,
                                     imgWidth, imgHeight,
                                     alt_fmt, fmt, 0, 0) !=
                PXFMT_CONVERSION_SUCCESS)
            {
                printf("CONVERSION STATUS CODE ERROR!!!\n");
                fflush(stdout);
                return false;
            }
            SwizzleRGBPartOfPixels();
        }
        else
        {
            if (pxfmt_convert_pixels(pPixels, pOriginalPixels,
                                     imgWidth, imgHeight,
                                     fmt, fmt, 0, 0) !=
                PXFMT_CONVERSION_SUCCESS)
            {
                printf("CONVERSION STATUS CODE ERROR!!!\n");
                fflush(stdout);
                return false;
            }
        }
        // Put the converted image on the screen, for a visual check:
        DoDrawPixels(imgWidth, 0);
        glutSwapBuffers();
        glFinish();
    } else {
        // Draw the read-image on the right-half of the window:
        printf("The following format/type combination is NOT YET SUPPORTED\n");
        fflush(stdout);
        DoDrawPixels(imgWidth, 0);
        glutSwapBuffers();
        glFinish();

        // Now, read the image back from the window:
        glReadPixels(imgWidth, 0, imgWidth, imgHeight,
                     pCombo->format, pCombo->type,
                     pPixels);
    }

    // Finally, compare the newly-generated image with the originally-read-back
    // image:
    if (memcmp(pOriginalPixels, pPixels, imgSize) == 0) {
        return true;
    } else {
        return false;
    }
} // EmulatedPixelOps::TestDrawPixels()


/**
 * Repeatedly draws, while timing how long it takes.  This version calls the
 * generic method when we're supposed to swap buffers between draws, but only
 * calls the pxfmt_convert_pixels() function when swapBuffersBetweenDraws is
 * false.
 */
float EmulatedPixelOps::BenchmarkDrawPixels(int nFrames, bool swapBuffersBetweenDraws)
{
    if (swapBuffersBetweenDraws) {
        return PixelOps::BenchmarkDrawPixels(nFrames, swapBuffersBetweenDraws);
    }

    pxfmt_sized_format fmt = validate_format_type_combo(pCombo->format,
                                                        pCombo->type);
    if (fmt == PXFMT_INVALID) {
        printf("\t INVALID PXFMT!!!\n");
        fflush(stdout);
        return 0.0f;
    }

    // FIXME--FIGURE OUT A BETTER VALUE TO OVERRIDE nFrames TO:
    nFrames = 20;

    struct timeval start, end;
    struct timezone tz;
    gettimeofday(&start, &tz); /* start timing */
    int distance = 1;
    GLint leftEdge = 0;
    GLint rightEdge = imgWidth - 1;
    int i = 0, frame = 0;

    float rate = 0.0;

    for (frame = 0 ; frame < nFrames ; i++, frame++) {
        pxfmt_convert_pixels(pPixels, pOriginalPixels,
                             imgWidth, imgHeight, fmt, fmt, 0, 0);
    }

    gettimeofday(&end, &tz); /* finish timing */
    rate = frame / ((end.tv_sec-start.tv_sec) +
                    (end.tv_usec-start.tv_usec)/1000000.0);

    return rate;
} // PixelOps::BenchmarkDrawPixels()


/**
 * Do any cleanup after one or more calls to glDrawPixels().
 *
 * Just free the temporarily-allocated memory for the pixel data.
 */
void EmulatedPixelOps::FinishDrawPixels()
{
    free(pOriginalPixels);
    free(pPixels);
    if (pTexSubImage2DPixels) {
        free(pTexSubImage2DPixels);
        pTexSubImage2DPixels = NULL;
    }
} // EmulatedPixelOps::FinishDrawPixels()


/**
 * Swizzle the RGB components between RGBA<-->BGRA values that have the same
 * OpenGL "type".
 */
template <typename T>
void SwizzleRGBComponents(T *pPixels)
{
    struct Components {
        T red;
        T green;
        T blue;
        T alpha;
    };
    Components *pComponents = (Components *) pPixels;
    T tmpComponent;
    GLint width = imgWidth;
    GLint height = imgHeight;

    for (GLint y = 0 ; y < height ; y++) {
        for (GLint x = 0 ; x < width ; x++) {
            tmpComponent = pComponents->red;
            pComponents->red = pComponents->blue;
            pComponents->blue = tmpComponent;
            pComponents++;
        }
    }

} // RealPixelOps::FinishDrawPixels()


/**
 * Swizzle the RGB components between RGBA<-->BGRA values that have the same
 * OpenGL "type".
 */
void EmulatedPixelOps::SwizzleRGBPartOfPixels()
{
printf("\t SWIZZLING RGBA<-->BGRA with %d bytes-per-component\n",
       pCombo->testSwizzleType);
fflush(stdout);
    switch (pCombo->testSwizzleType) {
    case TestSwizzle_BYTE:
        SwizzleRGBComponents<unsigned char>((unsigned char*) pOriginalPixels);
        break;
    case TestSwizzle_SHORT:
        SwizzleRGBComponents<unsigned short>((unsigned short*) pOriginalPixels);
        break;
    case TestSwizzle_INT:
        SwizzleRGBComponents<unsigned int>((unsigned int*) pOriginalPixels);
        break;
    default:
        break;
    }
} // RealPixelOps::FinishDrawPixels()






















FormatTypeCombo *pCombo;

FormatTypeCombo combos[] = {

// The combinations surrounded by #ifdef SLOW_ON_NVIDIA are REALLY SLOW on
// Ian's NVIDIA card (i.e. ~2 seconds/frame).  In other words, the NVIDIA card
// is so slow, the program seems hung.
#define SLOW_ON_NVIDIA

    /*
     * GL_RED source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_RED,  "GL_RED",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RED,  "GL_RED",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RED,  "GL_RED",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RED,  "GL_RED",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RED,  "GL_RED",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RED,  "GL_RED",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
    {GL_RED,  "GL_RED",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 2,
     TestSwizzle_NONE},
    {GL_RED,  "GL_RED",  GL_FLOAT, "GL_FLOAT", 4,
     TestSwizzle_NONE},


    /*
     * GL_RG source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_RG,  "GL_RG",  GL_BYTE, "GL_BYTE", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RG,  "GL_RG",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RG,  "GL_RG",  GL_SHORT, "GL_SHORT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RG,  "GL_RG",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 4,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RG,  "GL_RG",  GL_INT, "GL_INT", 8,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RG,  "GL_RG",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 8,
     TestSwizzle_NONE},
    {GL_RG,  "GL_RG",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 4,
     TestSwizzle_NONE},
    {GL_RG,  "GL_RG",  GL_FLOAT, "GL_FLOAT", 8,
     TestSwizzle_NONE},


    /*
     * GL_RGB source
     */
    {GL_RGB, "GL_RGB", GL_BYTE, "GL_BYTE", 3,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 3,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RGB, "GL_RGB", GL_SHORT, "GL_SHORT", 6,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RGB, "GL_RGB", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 6,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RGB, "GL_RGB", GL_INT, "GL_INT", 12,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RGB, "GL_RGB", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 12,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB", GL_HALF_FLOAT, "GL_HALF_FLOAT", 6,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB", GL_FLOAT, "GL_FLOAT", 12,
     TestSwizzle_NONE},

    // Packed-integer format/type combinations:
    {GL_RGB, "GL_RGB", GL_UNSIGNED_BYTE_3_3_2, "GL_UNSIGNED_BYTE_3_3_2", 1,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB", GL_UNSIGNED_BYTE_2_3_3_REV, "GL_UNSIGNED_BYTE_2_3_3_REV", 1,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB", GL_UNSIGNED_SHORT_5_6_5, "GL_UNSIGNED_SHORT_5_6_5", 2,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB", GL_UNSIGNED_SHORT_5_6_5_REV, "GL_UNSIGNED_SHORT_5_6_5_REV", 2,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB",
     GL_UNSIGNED_INT_10F_11F_11F_REV, "GL_UNSIGNED_INT_10F_11F_11F_REV", 4,
     TestSwizzle_NONE},
    {GL_RGB, "GL_RGB", GL_UNSIGNED_INT_5_9_9_9_REV, "GL_UNSIGNED_INT_5_9_9_9_REV", 4,
     TestSwizzle_NONE},


    /*
     * GL_BGR source
     */
    {GL_BGR, "GL_BGR", GL_BYTE, "GL_BYTE", 3,
     TestSwizzle_NONE},
    {GL_BGR, "GL_BGR", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 3,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BGR, "GL_BGR", GL_SHORT, "GL_SHORT", 6,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BGR, "GL_BGR", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 6,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BGR, "GL_BGR", GL_INT, "GL_INT", 12,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BGR, "GL_BGR", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 12,
     TestSwizzle_NONE},
    {GL_BGR, "GL_BGR", GL_HALF_FLOAT, "GL_HALF_FLOAT", 6,
     TestSwizzle_NONE},
    {GL_BGR, "GL_BGR", GL_FLOAT, "GL_FLOAT", 12,
     TestSwizzle_NONE},


    /*
     * GL_RGBA source
     */
    {GL_RGBA, "GL_RGBA", GL_BYTE, "GL_BYTE", 4,
     TestSwizzle_BYTE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 4,
     TestSwizzle_BYTE},
#ifdef SLOW_ON_NVIDIA
    {GL_RGBA, "GL_RGBA", GL_SHORT, "GL_SHORT", 8,
     TestSwizzle_SHORT},
#endif // SLOW_ON_NVIDIA
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 8,
     TestSwizzle_SHORT},
#ifdef SLOW_ON_NVIDIA
    {GL_RGBA, "GL_RGBA", GL_INT, "GL_INT", 16,
     TestSwizzle_INT},
#endif // SLOW_ON_NVIDIA
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 16,
     TestSwizzle_INT},
    {GL_RGBA, "GL_RGBA", GL_HALF_FLOAT, "GL_HALF_FLOAT", 8,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_FLOAT, "GL_FLOAT", 16,
     TestSwizzle_INT},

    // Packed-integer format/type combinations:
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_SHORT_4_4_4_4, "GL_UNSIGNED_SHORT_4_4_4_4", 2,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_SHORT_4_4_4_4_REV,
     "GL_UNSIGNED_SHORT_4_4_4_4_REV", 2,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_SHORT_5_5_5_1, "GL_UNSIGNED_SHORT_5_5_5_1", 2,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_SHORT_1_5_5_5_REV,
     "GL_UNSIGNED_SHORT_1_5_5_5_REV", 2,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_INT_8_8_8_8, "GL_UNSIGNED_INT_8_8_8_8", 4,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_INT_8_8_8_8_REV,
     "GL_UNSIGNED_INT_8_8_8_8_REV", 4,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_INT_10_10_10_2,
     "GL_UNSIGNED_INT_10_10_10_2", 4,
     TestSwizzle_NONE},
    {GL_RGBA, "GL_RGBA", GL_UNSIGNED_INT_2_10_10_10_REV,
     "GL_UNSIGNED_INT_2_10_10_10_REV", 4,
     TestSwizzle_NONE},

    /*
     * GL_BGRA source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_BGRA, "GL_BGRA", GL_BYTE, "GL_BYTE", 4,
     TestSwizzle_BYTE},
#endif // SLOW_ON_NVIDIA
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 4,
     TestSwizzle_BYTE},
#ifdef SLOW_ON_NVIDIA
    {GL_BGRA, "GL_BGRA", GL_SHORT, "GL_SHORT", 8,
     TestSwizzle_SHORT},
#endif // SLOW_ON_NVIDIA
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 8,
     TestSwizzle_SHORT},
#ifdef SLOW_ON_NVIDIA
    {GL_BGRA, "GL_BGRA", GL_INT, "GL_INT", 16,
     TestSwizzle_INT},
#endif // SLOW_ON_NVIDIA
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 16,
     TestSwizzle_INT},
    {GL_BGRA, "GL_BGRA", GL_HALF_FLOAT, "GL_HALF_FLOAT", 8,
     TestSwizzle_SHORT},
    {GL_BGRA, "GL_BGRA", GL_FLOAT, "GL_FLOAT", 16,
     TestSwizzle_INT},

    // Packed-integer format/type combinations:
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_SHORT_4_4_4_4, "GL_UNSIGNED_SHORT_4_4_4_4", 2,
     TestSwizzle_NONE},
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_SHORT_4_4_4_4_REV,
     "GL_UNSIGNED_SHORT_4_4_4_4_REV", 2,
     TestSwizzle_NONE},
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_SHORT_5_5_5_1, "GL_UNSIGNED_SHORT_5_5_5_1", 2,
     TestSwizzle_NONE},
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_SHORT_1_5_5_5_REV,
     "GL_UNSIGNED_SHORT_1_5_5_5_REV", 2,
     TestSwizzle_NONE},
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_INT_8_8_8_8, "GL_UNSIGNED_INT_8_8_8_8", 4,
     TestSwizzle_NONE},
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_INT_8_8_8_8_REV,
     "GL_UNSIGNED_INT_8_8_8_8_REV", 4,
     TestSwizzle_NONE},
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_INT_10_10_10_2,
     "GL_UNSIGNED_INT_10_10_10_2", 4,
     TestSwizzle_NONE},
    {GL_BGRA, "GL_BGRA", GL_UNSIGNED_INT_2_10_10_10_REV,
     "GL_UNSIGNED_INT_2_10_10_10_REV", 4,
     TestSwizzle_NONE},


#define SUPPORT_GREEN
#ifdef SUPPORT_GREEN
    /*
     * GL_GREEN source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_GREEN,  "GL_GREEN",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_GREEN,  "GL_GREEN",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_GREEN,  "GL_GREEN",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_GREEN,  "GL_GREEN",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_GREEN,  "GL_GREEN",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_GREEN,  "GL_GREEN",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
    {GL_GREEN,  "GL_GREEN",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 2,
     TestSwizzle_NONE},
    {GL_GREEN,  "GL_GREEN",  GL_FLOAT, "GL_FLOAT", 4,
     TestSwizzle_NONE},
#endif // SUPPORT_GREEN


#define SUPPORT_BLUE
#ifdef SUPPORT_BLUE
    /*
     * GL_BLUE source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_BLUE,  "GL_BLUE",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BLUE,  "GL_BLUE",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BLUE,  "GL_BLUE",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BLUE,  "GL_BLUE",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BLUE,  "GL_BLUE",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BLUE,  "GL_BLUE",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
    {GL_BLUE,  "GL_BLUE",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 2,
     TestSwizzle_NONE},
    {GL_BLUE,  "GL_BLUE",  GL_FLOAT, "GL_FLOAT", 4,
     TestSwizzle_NONE},
#endif // SUPPORT_BLUE


#define SUPPORT_ALPHA
#ifdef SUPPORT_ALPHA
    /*
     * GL_ALPHA source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_ALPHA,  "GL_ALPHA",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_ALPHA,  "GL_ALPHA",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_ALPHA,  "GL_ALPHA",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_ALPHA,  "GL_ALPHA",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_ALPHA,  "GL_ALPHA",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_ALPHA,  "GL_ALPHA",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
    {GL_ALPHA,  "GL_ALPHA",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 2,
     TestSwizzle_NONE},
    {GL_ALPHA,  "GL_ALPHA",  GL_FLOAT, "GL_FLOAT", 4,
     TestSwizzle_NONE},
#endif // SUPPORT_ALPHA


#define SUPPORT_LUMINANCE
#ifdef SUPPORT_LUMINANCE
    /*
     * GL_LUMINANCE source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 2,
     TestSwizzle_NONE},
    {GL_LUMINANCE,  "GL_LUMINANCE",  GL_FLOAT, "GL_FLOAT", 4,
     TestSwizzle_NONE},


    /*
     * GL_LUMINANCE_ALPHA source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_BYTE, "GL_BYTE", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_SHORT, "GL_SHORT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 4,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_INT, "GL_INT", 8,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 8,
     TestSwizzle_NONE},
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 4,
     TestSwizzle_NONE},
    {GL_LUMINANCE_ALPHA,  "GL_LUMINANCE_ALPHA",  GL_FLOAT, "GL_FLOAT", 8,
     TestSwizzle_NONE},
#endif // SUPPORT_LUMINANCE


    /*
     * GL_RED_INTEGER source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_RED_INTEGER,  "GL_RED_INTEGER",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RED_INTEGER,  "GL_RED_INTEGER",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RED_INTEGER,  "GL_RED_INTEGER",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RED_INTEGER,  "GL_RED_INTEGER",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RED_INTEGER,  "GL_RED_INTEGER",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RED_INTEGER,  "GL_RED_INTEGER",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},


    /*
     * GL_RG_INTEGER source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_RG_INTEGER,  "GL_RG_INTEGER",  GL_BYTE, "GL_BYTE", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RG_INTEGER,  "GL_RG_INTEGER",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RG_INTEGER,  "GL_RG_INTEGER",  GL_SHORT, "GL_SHORT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RG_INTEGER,  "GL_RG_INTEGER",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 4,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RG_INTEGER,  "GL_RG_INTEGER",  GL_INT, "GL_INT", 8,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RG_INTEGER,  "GL_RG_INTEGER",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 8,
     TestSwizzle_NONE},


    /*
     * GL_RGB_INTEGER source
     */
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_BYTE, "GL_BYTE", 3,
     TestSwizzle_NONE},
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 3,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_SHORT, "GL_SHORT", 6,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 6,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_INT, "GL_INT", 12,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 12,
     TestSwizzle_NONE},

    // Packed-integer format/type combinations:
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_UNSIGNED_BYTE_3_3_2, "GL_UNSIGNED_BYTE_3_3_2", 1,
     TestSwizzle_NONE},
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_UNSIGNED_BYTE_2_3_3_REV, "GL_UNSIGNED_BYTE_2_3_3_REV", 1,
     TestSwizzle_NONE},
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_UNSIGNED_SHORT_5_6_5, "GL_UNSIGNED_SHORT_5_6_5", 2,
     TestSwizzle_NONE},
    {GL_RGB_INTEGER, "GL_RGB_INTEGER", GL_UNSIGNED_SHORT_5_6_5_REV, "GL_UNSIGNED_SHORT_5_6_5_REV", 2,
     TestSwizzle_NONE},


    /*
     * GL_BGR_INTEGER source
     */
    {GL_BGR_INTEGER, "GL_BGR_INTEGER", GL_BYTE, "GL_BYTE", 3,
     TestSwizzle_NONE},
    {GL_BGR_INTEGER, "GL_BGR_INTEGER", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 3,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BGR_INTEGER, "GL_BGR_INTEGER", GL_SHORT, "GL_SHORT", 6,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BGR_INTEGER, "GL_BGR_INTEGER", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 6,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BGR_INTEGER, "GL_BGR_INTEGER", GL_INT, "GL_INT", 12,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BGR_INTEGER, "GL_BGR_INTEGER", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 12,
     TestSwizzle_NONE},


    /*
     * GL_RGBA_INTEGER source
     */
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_BYTE, "GL_BYTE", 4,
     TestSwizzle_BYTE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 4,
     TestSwizzle_BYTE},
#ifdef SLOW_ON_NVIDIA
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_SHORT, "GL_SHORT", 8,
     TestSwizzle_SHORT},
#endif // SLOW_ON_NVIDIA
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 8,
     TestSwizzle_SHORT},
#ifdef SLOW_ON_NVIDIA
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_INT, "GL_INT", 16,
     TestSwizzle_INT},
#endif // SLOW_ON_NVIDIA
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 16,
     TestSwizzle_INT},

    // Packed-integer format/type combinations:
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_SHORT_4_4_4_4, "GL_UNSIGNED_SHORT_4_4_4_4", 2,
     TestSwizzle_NONE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_SHORT_4_4_4_4_REV,
     "GL_UNSIGNED_SHORT_4_4_4_4_REV", 2,
     TestSwizzle_NONE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_SHORT_5_5_5_1, "GL_UNSIGNED_SHORT_5_5_5_1", 2,
     TestSwizzle_NONE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_SHORT_1_5_5_5_REV,
     "GL_UNSIGNED_SHORT_1_5_5_5_REV", 2,
     TestSwizzle_NONE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_INT_8_8_8_8, "GL_UNSIGNED_INT_8_8_8_8", 4,
     TestSwizzle_NONE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_INT_8_8_8_8_REV,
     "GL_UNSIGNED_INT_8_8_8_8_REV", 4,
     TestSwizzle_NONE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_INT_10_10_10_2,
     "GL_UNSIGNED_INT_10_10_10_2", 4,
     TestSwizzle_NONE},
    {GL_RGBA_INTEGER, "GL_RGBA_INTEGER", GL_UNSIGNED_INT_2_10_10_10_REV,
     "GL_UNSIGNED_INT_2_10_10_10_REV", 4,
     TestSwizzle_NONE},

    /*
     * GL_BGRA_INTEGER source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_BYTE, "GL_BYTE", 4,
     TestSwizzle_BYTE},
#endif // SLOW_ON_NVIDIA
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 4,
     TestSwizzle_BYTE},
#ifdef SLOW_ON_NVIDIA
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_SHORT, "GL_SHORT", 8,
     TestSwizzle_SHORT},
#endif // SLOW_ON_NVIDIA
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 8,
     TestSwizzle_SHORT},
#ifdef SLOW_ON_NVIDIA
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_INT, "GL_INT", 16,
     TestSwizzle_INT},
#endif // SLOW_ON_NVIDIA
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 16,
     TestSwizzle_INT},

    // Packed-integer format/type combinations:
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_SHORT_4_4_4_4, "GL_UNSIGNED_SHORT_4_4_4_4", 2,
     TestSwizzle_NONE},
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_SHORT_4_4_4_4_REV,
     "GL_UNSIGNED_SHORT_4_4_4_4_REV", 2,
     TestSwizzle_NONE},
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_SHORT_5_5_5_1, "GL_UNSIGNED_SHORT_5_5_5_1", 2,
     TestSwizzle_NONE},
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_SHORT_1_5_5_5_REV,
     "GL_UNSIGNED_SHORT_1_5_5_5_REV", 2,
     TestSwizzle_NONE},
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_INT_8_8_8_8, "GL_UNSIGNED_INT_8_8_8_8", 4,
     TestSwizzle_NONE},
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_INT_8_8_8_8_REV,
     "GL_UNSIGNED_INT_8_8_8_8_REV", 4,
     TestSwizzle_NONE},
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_INT_10_10_10_2,
     "GL_UNSIGNED_INT_10_10_10_2", 4,
     TestSwizzle_NONE},
    {GL_BGRA_INTEGER, "GL_BGRA_INTEGER", GL_UNSIGNED_INT_2_10_10_10_REV,
     "GL_UNSIGNED_INT_2_10_10_10_REV", 4,
     TestSwizzle_NONE},


#define SUPPORT_GREEN
#ifdef SUPPORT_GREEN
    /*
     * GL_GREEN_INTEGER source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_GREEN_INTEGER,  "GL_GREEN_INTEGER",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_GREEN_INTEGER,  "GL_GREEN_INTEGER",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_GREEN_INTEGER,  "GL_GREEN_INTEGER",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_GREEN_INTEGER,  "GL_GREEN_INTEGER",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_GREEN_INTEGER,  "GL_GREEN_INTEGER",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_GREEN_INTEGER,  "GL_GREEN_INTEGER",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
#endif // SUPPORT_GREEN


#define SUPPORT_BLUE
#ifdef SUPPORT_BLUE
    /*
     * GL_BLUE_INTEGER source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_BLUE_INTEGER,  "GL_BLUE_INTEGER",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BLUE_INTEGER,  "GL_BLUE_INTEGER",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BLUE_INTEGER,  "GL_BLUE_INTEGER",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BLUE_INTEGER,  "GL_BLUE_INTEGER",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_BLUE_INTEGER,  "GL_BLUE_INTEGER",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_BLUE_INTEGER,  "GL_BLUE_INTEGER",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
#endif // SUPPORT_BLUE


#define SUPPORT_ALPHA
#ifdef SUPPORT_ALPHA
    /*
     * GL_ALPHA_INTEGER source
     */
#ifdef SLOW_ON_NVIDIA
    {GL_ALPHA_INTEGER,  "GL_ALPHA_INTEGER",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_ALPHA_INTEGER,  "GL_ALPHA_INTEGER",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_ALPHA_INTEGER,  "GL_ALPHA_INTEGER",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_ALPHA_INTEGER,  "GL_ALPHA_INTEGER",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_ALPHA_INTEGER,  "GL_ALPHA_INTEGER",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_ALPHA_INTEGER,  "GL_ALPHA_INTEGER",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
#endif // SUPPORT_ALPHA


    /*
     * GL_DEPTH_COMPONENT
     */
#ifdef SLOW_ON_NVIDIA
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 2,
     TestSwizzle_NONE},
    {GL_DEPTH_COMPONENT,  "GL_DEPTH_COMPONENT",  GL_FLOAT, "GL_FLOAT", 4,
     TestSwizzle_NONE},


    /*
     * GL_STENCIL_INDEX
     */
#ifdef SLOW_ON_NVIDIA
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_BYTE, "GL_BYTE", 1,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE", 1,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_SHORT, "GL_SHORT", 2,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT", 2,
     TestSwizzle_NONE},
#ifdef SLOW_ON_NVIDIA
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_INT, "GL_INT", 4,
     TestSwizzle_NONE},
#endif // SLOW_ON_NVIDIA
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_UNSIGNED_INT, "GL_UNSIGNED_INT", 4,
     TestSwizzle_NONE},
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_HALF_FLOAT, "GL_HALF_FLOAT", 2,
     TestSwizzle_NONE},
    {GL_STENCIL_INDEX,  "GL_STENCIL_INDEX",  GL_FLOAT, "GL_FLOAT", 4,
     TestSwizzle_NONE},


    /*
     * GL_DEPTH_STENCIL
     */
    {GL_DEPTH_STENCIL, "GL_DEPTH_STENCIL",
     GL_UNSIGNED_INT_24_8, "GL_UNSIGNED_INT_24_8", 4,
     TestSwizzle_NONE},
    {GL_DEPTH_STENCIL, "GL_DEPTH_STENCIL",
     GL_FLOAT_32_UNSIGNED_INT_24_8_REV, "GL_FLOAT_32_UNSIGNED_INT_24_8_REV", 8,
     TestSwizzle_NONE},
};










/*****************************************************************************\
Description:
    Initialize the static instance of this singleton class.
\*****************************************************************************/
IGFXgpuPxAllPackedIntVals* IGFXgpuPxAllPackedIntVals::mpInstance = NULL;


/*****************************************************************************\
Description:
    Return (after potentially instantiating) a pointer to the instance of this
    singleton class.
\*****************************************************************************/
IGFXgpuPxAllPackedIntVals* IGFXgpuPxAllPackedIntVals::GetInstance()
{
    if (!mpInstance)
    {
        mpInstance = new IGFXgpuPxAllPackedIntVals;
    }
    return mpInstance;
}


/*****************************************************************************\
Description:
    Return a pointer to the values for a given format/type combination.
\*****************************************************************************/
IGFXgpuPxPackedIntVals *IGFXgpuPxAllPackedIntVals::GetPackedIntVals(
    GLuint index)
{
    return &mpPackedIntVals[index];
}


/*****************************************************************************\
Description:
    Construct this singleton class, with the values to mask, shift, and
    divide/multiply packed integer values by.  This is "private" so that only
    the GetInstance() method may instantiate the one (singleton) copy of this
    class.
\*****************************************************************************/
IGFXgpuPxAllPackedIntVals::IGFXgpuPxAllPackedIntVals()
{
    // Notice that the following values are ordered so that we can always pull
    // out the RBGA values in an RGBA order.  Also notice that there are some
    // "placeholders" values (to keep indexing into this array simple) for
    // entries that make no sense for algorithms 5 and 6 (e.g.  the
    // 10F_11F_11F and 5_9_9_9 types).
    static IGFXgpuPxPackedIntVals packedIntVals[] =
    {
        // The following entries are for GL_RGB[A][_INTEGER] formats (i.e. the
        // values for the "red" component really are for the red component):
        {GL_FALSE, 5, 2, 0, 0,          // __GL_UNSIGNED_BYTE_3_3_2
         0xE0, 0x1C, 0x03, 0x00,
         8.0, 8.0, 4.0, 1.0},
        {GL_FALSE, 11, 5, 0, 0,         // __GL_UNSIGNED_SHORT_5_6_5
         0xF800, 0x07E0, 0x001F, 0x0000,
         31.0, 63.0, 31.0, 1.0},
        {GL_TRUE, 12, 8, 4, 0,          // __GL_UNSIGNED_SHORT_4_4_4_4
         0xF000, 0x0F00, 0x00F0, 0x000F,
         15.0, 15.0, 15.0, 15.0},
        {GL_TRUE, 11, 6, 1, 0,          // __GL_UNSIGNED_SHORT_5_5_5_1
         0xF800, 0x07C0, 0x003E, 0x0001,
         31.0, 31.0, 31.0, 1.0},
        {GL_TRUE, 24, 16, 8, 0,         // __GL_UNSIGNED_INT_8_8_8_8
         0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF,
         255.0, 255.0, 255.0, 255.0},
        {GL_TRUE, 22, 12, 2, 0,         // __GL_UNSIGNED_INT_10_10_10_2
         0xFFC00000, 0x003FF000, 0x00000FFC, 0x00000003,
         1023.0, 1023.0, 1023.0, 3.0},
        {GL_FALSE, 0, 3, 6, 0,          // __GL_UNSIGNED_BYTE_2_3_3_REV
         0x07, 0x38, 0xC0, 0x00,
         8.0, 8.0, 4.0, 1.0},
        {GL_FALSE, 0, 5, 11, 0,         // __GL_UNSIGNED_SHORT_5_6_5_REV
         0x001F, 0x07E0, 0xF800, 0x0000,
         31.0, 63.0, 31.0, 1.0},
        {GL_TRUE, 0, 4, 8, 12,          // __GL_UNSIGNED_SHORT_4_4_4_4_REV
         0x000F, 0x00F0, 0x0F00, 0xF000,
         15.0, 15.0, 15.0, 15.0},
        {GL_TRUE, 0, 5, 10, 15,         // __GL_UNSIGNED_SHORT_1_5_5_5_REV
         0x001F, 0x03E0, 0x7C00, 0x8000,
         31.0, 31.0, 31.0, 1.0},
        {GL_TRUE, 0, 8, 16, 24,         // __GL_UNSIGNED_INT_8_8_8_8_REV
         0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000,
         255.0, 255.0, 255.0, 255.0},
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_INT_10F_11F_11F_REV
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_INT_5_9_9_9_REV
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_TRUE, 0, 10, 20, 30,        // __GL_UNSIGNED_INT_2_10_10_10_REV
         0x000003FF, 0x000FFC00, 0x3FF00000, 0xC0000000,
         1023.0, 1023.0, 1023.0, 3.0},

        // The following entries are for GL_BGR[A][_INTEGER] formats (i.e. the
        // values for the "red" and "blue" components are reversed):
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_BYTE_3_3_2
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_SHORT_5_6_5
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_TRUE, 4, 8, 12, 0,          // __GL_UNSIGNED_SHORT_4_4_4_4
         0x00F0, 0x0F00, 0xF000, 0x000F,
         15.0, 15.0, 15.0, 15.0},
        {GL_TRUE, 1, 6, 11, 0,          // __GL_UNSIGNED_SHORT_5_5_5_1
         0x003E, 0x07C0, 0xF800, 0x0001,
         31.0, 31.0, 31.0, 1.0},
        {GL_TRUE, 8, 16, 24, 0,         // __GL_UNSIGNED_INT_8_8_8_8
         0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF,
         255.0, 255.0, 255.0, 255.0},
        {GL_TRUE, 2, 12, 22, 0,         // __GL_UNSIGNED_INT_10_10_10_2
         0x00000FFC, 0x003FF000, 0xFFC00000, 0x00000003,
         1023.0, 1023.0, 1023.0, 3.0},
        {GL_FALSE, 0, 0, 0, 0,            // __GL_UNSIGNED_BYTE_2_3_3_REV
         0x00, 0x00, 0x00, 0x00,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_FALSE, 0, 0, 0, 0,            // __GL_UNSIGNED_SHORT_5_6_5_REV
         0x00, 0x00, 0x00, 0x00,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_TRUE, 8, 4, 0, 12,          // __GL_UNSIGNED_SHORT_4_4_4_4_REV
         0x0F00, 0x00F0, 0x000F, 0xF000,
         15.0, 15.0, 15.0, 15.0},
        {GL_TRUE, 10, 5, 0, 15,         // __GL_UNSIGNED_SHORT_1_5_5_5_REV
         0x7C00, 0x03E0, 0x001F, 0x8000,
         31.0, 31.0, 31.0, 1.0},
        {GL_TRUE, 16, 8, 0, 24,         // __GL_UNSIGNED_INT_8_8_8_8_REV
         0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000,
         255.0, 255.0, 255.0, 255.0},
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_INT_10F_11F_11F_REV
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_INT_5_9_9_9_REV
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_TRUE, 20, 10, 0, 30,        // __GL_UNSIGNED_INT_2_10_10_10_REV
         0x3FF00000, 0x000FFC00, 0x000003FF, 0xC0000000,
         1023.0, 1023.0, 1023.0, 3.0},

        // The following entries are for the ABGR format (i.e. the values for
        // the "red" and "alpha" components are reversed, and the values for
        // the "green" and "blue" components are reversed):
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_BYTE_3_3_2
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_SHORT_5_6_5
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_TRUE, 0, 4, 8, 12,          // __GL_UNSIGNED_SHORT_4_4_4_4
         0x000F, 0x00F0, 0x0F00, 0xF000,
         15.0, 15.0, 15.0, 15.0},
        {GL_TRUE, 0, 1, 6, 11,          // __GL_UNSIGNED_SHORT_5_5_5_1
         0x0001, 0x003E, 0x07C0, 0xF800,
         1.0, 31.0, 31.0, 31.0},
        {GL_TRUE, 0, 8, 16, 24,         // __GL_UNSIGNED_INT_8_8_8_8
         0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000,
         255.0, 255.0, 255.0, 255.0},
        {GL_TRUE, 0, 2, 12, 22,         // __GL_UNSIGNED_INT_10_10_10_2
         0x00000003, 0x00000FFC, 0x003FF000, 0xFFC00000,
         3.0, 1023.0, 1023.0, 1023.0},
        {GL_FALSE, 0, 0, 0, 0,            // __GL_UNSIGNED_BYTE_2_3_3_REV
         0x00, 0x00, 0x00, 0x00,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_FALSE, 0, 0, 0, 0,            // __GL_UNSIGNED_SHORT_5_6_5_REV
         0x00, 0x00, 0x00, 0x00,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_TRUE, 12, 8, 4, 0,            // __GL_UNSIGNED_SHORT_4_4_4_4_REV
         0xF000, 0x0F00, 0x00F0, 0x000F,
         15.0, 15.0, 15.0, 15.0},
        {GL_TRUE, 15, 10, 5, 0,         // __GL_UNSIGNED_SHORT_1_5_5_5_REV
         0x8000, 0x7C00, 0x03E0, 0x001F,
         1.0, 31.0, 31.0, 31.0},
        {GL_TRUE, 24, 16, 8, 0,           // __GL_UNSIGNED_INT_8_8_8_8_REV
         0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF,
         255.0, 255.0, 255.0, 255.0},
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_INT_10F_11F_11F_REV
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_FALSE, 0, 0, 0, 0,          // __GL_UNSIGNED_INT_5_9_9_9_REV
         0x00000000, 0x00000000, 0x00000000, 0x00000000,
         1.0, 1.0, 1.0, 1.0}, // placeholder
        {GL_TRUE, 30, 20, 10, 0,        // __GL_UNSIGNED_INT_2_10_10_10_REV
         0xC0000000, 0x3FF00000, 0x000FFC00, 0x000003FF,
         3.0, 1023.0, 1023.0, 1023.0},
    };

    // Point at the just-initialized array:
    mpPackedIntVals = packedIntVals;
}


/*****************************************************************************\
Description:
    The copy constructor is also "private" so that this class can't be copied.
\*****************************************************************************/
IGFXgpuPxAllPackedIntVals::IGFXgpuPxAllPackedIntVals(
    IGFXgpuPxAllPackedIntVals const&)
{
}











// This is the buffer that the auto-generated source is placed into:
char generatedShaderSrc[4096];


static int win;



// The purpose of this function is to perform drawing everytime GLUT requests
// drawing.
//
// - The first time this function is called, it will call renderReadAndDraw()
//   in order to show that the program is working for at least one format/type
//   combination.
//
// - The second time (and nth times) this function is called, it will call
//   drawAndReturnRate() in order to benchmark all of the desired format/type
//   combinations, and report the performance numbers.
//
// - Note: this function will be be called a 2nd/nth time by pressing the space
//   bar when the image window has focus.  Resizing that window will also cause
//   this function to be called.
static void display(void)
{
    PixelOps *px;
    int nCombos = sizeof(combos) / sizeof(FormatTypeCombo);
    int i = 0;

    switch ((int) unpackShaderType)
    {
    case RealCommands:
        px = new RealPixelOps(width, height, height, height);
        break;
    case Emulated:
        px = new EmulatedPixelOps(width, height, height, height);
        break;
    default:
        printf("The algorithm %d is not yet supported\n", unpackShaderType);
        fflush(stdout);
        exit(1);
    }


    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (comboNumberToRun >= 0) {
        i = comboNumberToRun;
        nCombos = comboNumberToRun + 1;
    }
    for ( ; i < nCombos ; i++) {
        float rate1, rate2;
        // Initially try a small number of frames in case it's WAY TOO SLOW:
        int nFrames = INITIAL_NUM_FRAMES_TO_DRAW;

        pCombo = &combos[i];

#if 0
        // Change the previous line to "#if 1" if wanting to benchmark with
        // byte-swapping turned on.
        glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
        glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
#endif

        //
        // PREPARE for a test or benchmark:
        //
        switch (command) {
        case DrawPixelsCommand:
            if (!px->PrepareForDrawPixels(pCombo)) {
                printf("%7s/%30s (0x%04x/0x%04x) is not supported\n",
                       pCombo->formatStr, pCombo->typeStr,
                       pCombo->format, pCombo->type);
                fflush(stdout);
                continue;
            }
            break;
        default:
            printf("ERROR: Unsupported command type!!!\n");
            fflush(stdout);
            exit(1);
        } // switch (command)



        //
        // TEST, if that's what was chosen to do:
        //
        if (testOrBenchmark == DoATest) {
            switch (command) {
            case DrawPixelsCommand:
                if (px->TestDrawPixels()) {
                    printf("%7s/%30s (0x%04x/0x%04x) passes\n",
                           pCombo->formatStr, pCombo->typeStr,
                           pCombo->format, pCombo->type);
                    fflush(stdout);
                } else {
                    printf("%7s/%30s (0x%04x/0x%04x) FAILS!!!\n",
                           pCombo->formatStr, pCombo->typeStr,
                           pCombo->format, pCombo->type);
                    fflush(stdout);
                }
                break;
            } // switch (command)
        } else {
            //
            // BENCHMARK, if that's what was chosen to do:
            //

            // Do a very-minimal amount of frames, in case it's very slow:
            switch (command) {
            case DrawPixelsCommand:
                rate1 = px->BenchmarkDrawPixels(nFrames, true);
                break;
            } // switch (command)


            if ((int) rate1 > MINIMUM_FRAME_RATE) {
                // The rate was fast enough to try drawing more frames:
                nFrames = MORE_FRAMES_TO_DRAW;

                switch (command) {
                case DrawPixelsCommand:
                    rate1 = px->BenchmarkDrawPixels(nFrames, true);

                    if ((int) rate1 > (nFrames / 2)) {
                        // Increase the number of frames to draw, so that
                        // drawAndReturnRate() runs for at least 2 seconds:
                        nFrames = (((int) rate1) * 4) & ~NUM_FRAMES_ROUNDING;

                        // Try drawing again with the increased number of
                        // frames:
                        rate1 = px->BenchmarkDrawPixels(nFrames, true);
                    }
                    break;
                } // switch (command)
            }

            // Now, draw repeatedly without buffer swaps & print results:
            switch (command) {
            case DrawPixelsCommand:
                rate2 = px->BenchmarkDrawPixels(nFrames, false);

                printf("%7s/%30s: %.3f FPS, %.3f DPS (%d frames)\n",
                       pCombo->formatStr, pCombo->typeStr, rate1, rate2,
                       nFrames);
                fflush(stdout);
                break;
            } // switch (command)
        }


        // FINISH the primitive:
        switch (command) {
        case DrawPixelsCommand:
            px->FinishDrawPixels();
            break;
        } // switch (command)
    }

    if (testOrBenchmark == DoATest) {
        exit(0);
    }
}



static void reshape(int width, int height)
{
    glViewport(0, 0, (GLint) width, (GLint) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -0.5, 0.5);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}



static void keyboard(unsigned char key, int x, int y)
{
    switch (key) {
    case 27:
        glutDestroyWindow(win);
        exit(0);
        break;
    case ' ':
        display();
        break;
    default:
        glutPostRedisplay();
        return;
    }
}



static void init(int argc, char **argv)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
}


void printUsage(char *programName)
{
    printf("Usage:\n  %s\n"
           "\t{test|benchmark}\n"
           "\t[-c <combo num>] [-l] [-o {loopOptimizationLevel}]\n"
           "\t[-s <image size>]\n"
           "\t[-t {real|emulated}]\n\n",
           programName);
}


int main(int argc, char **argv)
{
//    printf("argc = %d\n", argc);
    for (int i = 1 ; i < argc ; i++) {
//        printf("argv[%d] = \"%s\"\n", i, argv[i]);
        if (strcmp(argv[i], "test") == 0) {
            testOrBenchmark = DoATest;
        }
        else if (strcmp(argv[i], "benchmark") == 0) {
            testOrBenchmark = DoABenchmark;
        }
        else if ((strcmp(argv[i], "help") == 0) ||
                 (strcmp(argv[i], "-help") == 0) ||
                 (strcmp(argv[i], "--help") == 0)) {
            printUsage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-l") == 0) {
            int nCombos = sizeof(combos) / sizeof(FormatTypeCombo);
            FormatTypeCombo *pCombo;
            printf("Num    Format         Type\n");
            printf("---------------------------------------------\n");
            for (int j = 0 ; j < nCombos ; j++) {
                pCombo = &combos[j];
                printf("[%2d] = %7s/%30s\n", j,
                       pCombo->formatStr, pCombo->typeStr);
            }
            return 0;
        }
        else if (strcmp(argv[i], "-c") == 0) {
            if (++i < argc) {
                comboNumberToRun = atoi(argv[i]);
            } else {
                printf("Argument missing to the \"-%s\" option\n\n", argv[i]);
                printUsage(argv[0]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (++i < argc) {
                loopOptimizationLevel = atoi(argv[i]);
            } else {
                printf("Argument missing to the \"-%s\" option\n\n", argv[i]);
                printUsage(argv[0]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-s") == 0) {
            if (++i < argc) {
                height = atoi(argv[i]);
                width = height * 2;
            } else {
                printf("Argument missing to the \"-%s\" option\n\n", argv[i]);
                printUsage(argv[0]);
                return 1;
            }
        }
        else if (strcmp(argv[i], "-t") == 0) {
            if (++i < argc) {
                if (strcmp(argv[i], "real") == 0) {
                    unpackShaderType = RealCommands;
                } else if (strcmp(argv[i], "emulated") == 0) {
                    unpackShaderType = Emulated;
                } else {
                    printf("\"-%s\" is not a recognized argument\n\n", argv[i]);
                    printUsage(argv[0]);
                    return 1;
                }
            } else {
                printf("Argument missing to the \"-%s\" option\n\n", argv[i]);
                printUsage(argv[0]);
                return 1;
            }
        } else {
          printf("Unknown option: \"%s\"\n\n", argv[i]);
          printUsage(argv[0]);
          return 1;
        }
    }

    imgWidth = imgHeight = height;

    pCombo = &combos[0];


    glutInit(&argc, argv);

    glutInitWindowPosition(0, 0);
    glutInitWindowSize(width, height);

    glutInitDisplayMode(GLUT_RGBA);

    win = glutCreateWindow(argv[0]);
    if (!win) {
        printf("failed to create a window\n");
        exit(1);
    }

    if (glewInit() != GLEW_OK) {
        printf("failed to initialize GLEW\n");
        exit(1);
    }

    init(argc, argv);

    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutDisplayFunc(display);

    glutMainLoop();

    return 0;
}
