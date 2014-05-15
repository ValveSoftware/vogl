

#ifdef OLD_CODE
#else  // OLD_CODE
#endif // OLD_CODE

#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include <string>
#include <string.h>
#include <map>


// FIXME - REMOVE THIS MACRO, AND ITS USE
#define TEMPORARILY_DISABLE_SIZE_CHECK


#pragma comment( lib, "glew32.lib" )


#define FORMAT_STRLEN 20
#define TYPE_STRLEN 40



#ifdef WIN32
#else  // WIN32
#endif // WIN32

#ifdef WIN32
#include <windows.h>
#else  // WIN32
#include <sys/time.h>
#endif // WIN32


// Window height:
#define HEIGHT 768
static GLint width = (HEIGHT * 2), height = HEIGHT;
static GLint comboNumberToRun = -1;


#define OPT_COMPILE_TOO 4
#define OPT_CREATE_TEXTURE_TOO 3
#define OPT_COPY_DATA_TOO 2
#define OPT_ONLY_RENDER 1
static GLint loopOptimizationLevel = OPT_COPY_DATA_TOO;


enum TestOrBenchmarkType
{
    DoATest = 0,
    DoABenchmark = 1,
};
static TestOrBenchmarkType testOrBenchmark = DoATest;


enum CommandType
{
    DrawPixelsCommand = 0,
};
static CommandType command = DrawPixelsCommand;


enum ShaderType
{
    RealCommands = 0,
    Emulated = 1,
};
static ShaderType unpackShaderType = RealCommands;


#define INITIAL_NUM_FRAMES_TO_DRAW 2
#define MINIMUM_FRAME_RATE 16
#define MORE_FRAMES_TO_DRAW 256
#define NUM_FRAMES_ROUNDING (MORE_FRAMES_TO_DRAW - 1)

GLuint imgWidth;  // Width of the image in pixels
GLuint imgHeight; // Height of the image in pixels

#define VERTEX_SHADER_FILE "twoHalves.vert"
#define FRAGMENT_SHADER_FILE "twoHalves.frag"





enum TestSwizzleType
{
    TestSwizzle_NONE = 0,
    TestSwizzle_BYTE = 1,
    TestSwizzle_SHORT = 2,
    TestSwizzle_INT = 4,
};

struct FormatTypeCombo
{
    /** OpenGL API-level "format" of the "user data" (e.g. GL_RGBA). */
    GLuint format;
    /** Human-readable version of "format". */
    char formatStr[FORMAT_STRLEN];
    /** OpenGL API-level "type" of the "user data" (e.g. GL_FLOAT). */
    GLuint type;
    /** Human-readable version of "type". */
    char typeStr[TYPE_STRLEN];
    /**
     * This is used to allocate memory for glReadPixels() data for this
     * format/type combination.
     */
    GLuint perPixelSizeInBytes;

    /** What C++ data type to use for swizzling between RGBA/BGRA for testing */
    TestSwizzleType testSwizzleType;
};





// NOTE: The following 2 classes were cloned from the PixelOps driver:
struct IGFXgpuPxPackedIntVals {
    GLboolean fourComponents; // GL_TRUE == 4 components, else 3

    GLuint redShiftAmt;       // Amount to shift for the red value
    GLuint greenShiftAmt;     // Amount to shift for the green value
    GLuint blueShiftAmt;      // Amount to shift for the blue value
    GLuint alphaShiftAmt;     // Amount to shift for the alpha value

    GLuint redMask;           // Value to mask by for the red value
    GLuint greenMask;         // Value to mask by for the green value
    GLuint blueMask;          // Value to mask by for the blue value
    GLuint alphaMask;         // Value to mask by for the alpha value

    GLfloat redSize;          // Value to multiply/divide by for the red value
    GLfloat greenSize;        // Value to multiply/divide by for the green value
    GLfloat blueSize;         // Value to multiply/divide by for the blue value
    GLfloat alphaSize;        // Value to multiply/divide by for the alpha value
};
class IGFXgpuPxAllPackedIntVals
{
public:
    static IGFXgpuPxAllPackedIntVals* GetInstance();
    IGFXgpuPxPackedIntVals *GetPackedIntVals(GLuint index);

private:
    IGFXgpuPxAllPackedIntVals();
    IGFXgpuPxAllPackedIntVals(IGFXgpuPxAllPackedIntVals const&);
    IGFXgpuPxAllPackedIntVals& operator=(IGFXgpuPxAllPackedIntVals const&) {};

private:
    static IGFXgpuPxAllPackedIntVals* mpInstance; // Singleton instance
    IGFXgpuPxPackedIntVals *mpPackedIntVals; // One struct per combination
};


// For algorithm 7, we need the maximum values for 32-bit [unsigned] int's,
// expressed as a floating-point number:
#define MAX_UINT32_AS_FLOAT 4294967295.0
#define MAX_INT32_AS_FLOAT  2147483647.0



/**
 * Abstract base class for performing, testing, and benchmarking various
 * PixelOps functions (e.g. glDrawPixels()) either with real OpenGL commands,
 * or via emulation.
 */
class PixelOps
{
public:
    PixelOps(GLint windowWidth, GLint windowHeight,
             GLint imageWidth, GLint imageHeight)
    {
        winWidth = windowWidth;
        winHeight = windowHeight;
        imgWidth = imageWidth;
        imgHeight = imageHeight;
        pCombo = NULL;
        imgSize = 0;
        pOriginalPixels = NULL;
        pPixels = NULL;
        pTexSubImage2DPixels = NULL;
        drawTestImageProgram = 0;
        uniformLocation_passThru = 0;
        uniformLocation_leftHalf = 0;
        txImgTexture = 0;
    }
    virtual ~PixelOps() {}

    virtual bool PrepareForDrawPixels(FormatTypeCombo *pComboToUse) = 0;
    virtual void DoDrawPixels(GLint x, GLint y) = 0;
    virtual bool TestDrawPixels() = 0;
    virtual float BenchmarkDrawPixels(int nFrames, bool swapBuffersBetweenDraws);
    virtual void FinishDrawPixels() = 0;

protected:
    void DrawTestImageOnLeftSideOfWindow();
    bool CommonDrawPixelsSetup(FormatTypeCombo *pComboToUse);

protected:
    GLint winWidth;
    GLint winHeight;
    GLint imgWidth;
    GLint imgHeight;
    FormatTypeCombo *pCombo;
    GLuint imgSize; // Size of the image in bytes
    GLuint *pOriginalPixels;
    GLuint *pPixels;
    GLuint *pTexSubImage2DPixels; // What to give to glTexSubImage2D()
    GLuint txImgTexture; // Texture used as the destination of glTexImage2D()

private:
    void getProg1UniformLocations();

private:
    GLuint drawTestImageProgram;
    GLuint uniformLocation_passThru;
    GLuint uniformLocation_leftHalf;
};


/**
 * Concrete class for performing PixelOps functions using the real OpenGL
 * commands.
 */
class RealPixelOps : public PixelOps
{
public:
    RealPixelOps(GLint windowWidth, GLint windowHeight,
                 GLint imageWidth, GLint imageHeight)
    : PixelOps(windowWidth, windowHeight, imageWidth, imageHeight)
    {
    }
    virtual ~RealPixelOps() {}

    virtual bool PrepareForDrawPixels(FormatTypeCombo *pComboToUse);
    virtual void DoDrawPixels(GLint x, GLint y);
    virtual bool TestDrawPixels();
    virtual void FinishDrawPixels();

private:
};


/**
 * Concrete class for performing PixelOps functions using the real OpenGL
 * commands.
 */
class EmulatedPixelOps : public PixelOps
{
public:
    EmulatedPixelOps(GLint windowWidth, GLint windowHeight,
                 GLint imageWidth, GLint imageHeight)
    : PixelOps(windowWidth, windowHeight, imageWidth, imageHeight)
    {
    }
    virtual ~EmulatedPixelOps() {}

    virtual bool PrepareForDrawPixels(FormatTypeCombo *pComboToUse);
    virtual void DoDrawPixels(GLint x, GLint y);
    virtual bool TestDrawPixels();
    virtual float BenchmarkDrawPixels(int nFrames, bool swapBuffersBetweenDraws);
    virtual void FinishDrawPixels();

private:
    void SwizzleRGBPartOfPixels();
};
