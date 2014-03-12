// BufferObject.c
// OpenGL SuperBible, Chapter 16
// Demonstrates vertex buffer objects
// Program by Benjamin Lipchak

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>

// typedef int GLintptr;
// typedef int GLsizeiptr;

#define GL_ARRAY_BUFFER                             0x8892
#define GL_WRITE_ONLY                               0x88B9
#define GL_STREAM_DRAW                              0x88E0
#define GL_STATIC_DRAW                              0x88E4



#ifdef APIENTRY
#undef APIENTRY
#endif // APIENTRY
#define APIENTRY


#ifdef RAND_MAX
#undef RAND_MAX
#endif // RAND_MAX
#define RAND_MAX                                       0x7FFF


typedef GLvoid    (APIENTRY * PFNGLBINDBUFFERPROC) (GLenum target,
                                                    GLuint buffer);
typedef GLvoid    (APIENTRY * PFNGLBUFFERDATAPROC) (GLenum target,
                                                    GLsizeiptr size,
                                                    const GLvoid *data,
                                                    GLenum usage);
typedef GLvoid    (APIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum target,
                                                       GLintptr offset,
                                                       GLsizeiptr size,
                                                       const GLvoid *data);
typedef GLvoid    (APIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei n,
                                                       const GLuint *buffers);
typedef GLvoid    (APIENTRY * PFNGLGENBUFFERSPROC) (GLsizei n,
                                                    GLuint *buffers);
typedef GLvoid *  (APIENTRY * PFNGLMAPBUFFERPROC) (GLenum target,
                                                   GLenum access);
typedef GLboolean (APIENTRY * PFNGLUNMAPBUFFERPROC) (GLenum target);

PFNGLBINDBUFFERPROC    glBindBuffer =0;
PFNGLBUFFERDATAPROC    glBufferData =0;
PFNGLBUFFERSUBDATAPROC glBufferSubData =0;
PFNGLDELETEBUFFERSPROC glDeleteBuffers =0;
PFNGLGENBUFFERSPROC    glGenBuffers =0;
PFNGLMAPBUFFERPROC     glMapBuffer =0;
PFNGLUNMAPBUFFERPROC   glUnmapBuffer =0;

GLint windowWidth = 512;                // window size
GLint windowHeight = 512;

GLuint bufferID;

GLboolean useBufferObject = GL_TRUE;
GLboolean mapBufferObject = GL_TRUE;
GLboolean animating =       GL_TRUE;

GLint numSphereVertices = 30000;
GLfloat *sphereVertexArray = NULL;

GLfloat ambientLight[] = { 0.4f, 0.4f, 0.4f, 1.0f};
GLfloat diffuseLight[] = { 0.9f, 0.9f, 0.9f, 1.0f};
GLfloat cameraPos[]    = { 400.0f, 400.0f, 400.0f, 1.0f};

// Called to regenerate points on the sphere
void RegenerateSphere(void)
{
    GLint i;
    
    if (mapBufferObject && useBufferObject)
    {
        // Delete old vertex array memory
        if (sphereVertexArray)
            free(sphereVertexArray);

        glBindBuffer(GL_ARRAY_BUFFER, bufferID);

        // Avoid pipeline flush during glMapBuffer by
        // marking data store as empty
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) *
                     numSphereVertices * 3, NULL,
                     animating ? GL_STREAM_DRAW : GL_STATIC_DRAW);

        sphereVertexArray = (GLfloat *)glMapBuffer(GL_ARRAY_BUFFER,
                                                   GL_WRITE_ONLY);
        if (!sphereVertexArray)
        {
            fprintf(stderr, "Unable to map buffer object!\n");
            sleep(2);
            exit(0);
        }
    }
    else if (!sphereVertexArray)
    {
        // We need our old vertex array memory back
        sphereVertexArray = (GLfloat *)malloc(sizeof(GLfloat) * 
                                              numSphereVertices * 3);
        if (!sphereVertexArray)
        {
            fprintf(stderr, "Unable to allocate memory for vertex arrays!\n");
            sleep(2);
            exit(0);
        }
    }

    for (i = 0; i < numSphereVertices; i++)
    {
        GLfloat r1, r2, r3, scaleFactor;

        // pick a random vector
        r1 = (GLfloat)(rand() - (RAND_MAX/2));
        r2 = (GLfloat)(rand() - (RAND_MAX/2));
        r3 = (GLfloat)(rand() - (RAND_MAX/2));

        // determine normalizing scale factor
        scaleFactor = 1.0f / sqrt(r1*r1 + r2*r2 + r3*r3);

        sphereVertexArray[(i*3)+0] = r1 * scaleFactor;
        sphereVertexArray[(i*3)+1] = r2 * scaleFactor;
        sphereVertexArray[(i*3)+2] = r3 * scaleFactor;
    }

    if (mapBufferObject && useBufferObject)
    {
        if (!glUnmapBuffer(GL_ARRAY_BUFFER))
        {
            // Some window system event has trashed our data...
            // Try, try again!
            RegenerateSphere();
        }
        sphereVertexArray = NULL;
    }
}

// Switch between buffer objects and plain old vertex arrays
void SetRenderingMethod(void)
{
    if (useBufferObject)
    {
        glBindBuffer(GL_ARRAY_BUFFER, bufferID);
        // No stride, no offset
        glNormalPointer(GL_FLOAT, 0, 0);
        glVertexPointer(3, GL_FLOAT, 0, 0);

        if (!mapBufferObject)
        {
            if (animating)
            {
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLfloat) * 
                                numSphereVertices * 3, sphereVertexArray);
            }
            else
            {
                // If not animating, this gets called once
                // to establish new static buffer object
                glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 
                             numSphereVertices * 3, sphereVertexArray, 
                             GL_STATIC_DRAW);
            }
        }
    }
    else
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glNormalPointer(GL_FLOAT, 0, sphereVertexArray);
        glVertexPointer(3, GL_FLOAT, 0, sphereVertexArray);
    }
}

// Called to draw scene objects
void DrawModels(void)
{
    GLint r, g, b;

    // Draw 27 spheres in a color cube
    for (r = 0; r < 3; r++)
    {
        for (g = 0; g < 3; g++)
        {
            for (b = 0; b < 3; b++)
            {
                glColor3f(r * 0.5f, g * 0.5f, b * 0.5f);

                glPushMatrix();
                glTranslatef(100.0f * r - 100.0f, 
                             100.0f * g - 100.0f, 
                             100.0f * b - 100.0f);
                glScalef(50.0f, 50.0f, 50.0f);
                glDrawArrays(GL_POINTS, 0, numSphereVertices);
                glPopMatrix();
            }
        }
    }
}

// Called to draw scene
void RenderScene(void)
{
    struct timeb timeBuffer;
    static int frameCounter = 0;
    static long lastTime = 0;

    // Get initial time
    if (frameCounter == 0)
    {
        ftime(&timeBuffer);
        lastTime = (timeBuffer.time * 1000) + timeBuffer.millitm;
    }

    frameCounter++;
    if (frameCounter == 100)
    {
        long thisTime;

        frameCounter = 0;
        ftime(&timeBuffer);
        thisTime = (timeBuffer.time * 1000) + timeBuffer.millitm;
        fprintf(stdout, "FPS: %f\n", 100.0f * 1000.0f / (thisTime - lastTime));
        lastTime = thisTime;
    }

    // Track camera angle
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 1.0f, 10.0f, 10000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(cameraPos[0], cameraPos[1], cameraPos[2], 
              0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (animating)
    {
        RegenerateSphere();
        SetRenderingMethod();
    }

    // Draw objects in the scene
    DrawModels();

    // Flush drawing commands
    glutSwapBuffers();

    glutPostRedisplay();
}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
{
    const GLubyte *version;
    GLboolean glVersion15 = GL_FALSE;

    fprintf(stdout, "Buffer Object Demo\n\n");

    // Make sure required functionality is available!
    version = glGetString(GL_VERSION);
    if ((version[0] == '1') && (version[1] == '.') &&
        (version[2] >= '5') && (version[2] <= '9'))
    {
        glVersion15 = GL_TRUE;
    }

    if (!glVersion15 && !gltIsExtSupported("GL_ARB_vertex_buffer_object"))
    {
        fprintf(stderr, "Neither OpenGL 1.5 nor GL_ARB_vertex_buffer_object"
                        " extension is available!\n");
        sleep(2);
        exit(0);
    }

    // Load the function pointers
    if (glVersion15)
    {
        glBindBuffer = gltGetExtensionPointer("glBindBuffer");
        glBufferData = gltGetExtensionPointer("glBufferData");
        glBufferSubData = gltGetExtensionPointer("glBufferSubData");
        glDeleteBuffers = gltGetExtensionPointer("glDeleteBuffers");
        glGenBuffers = gltGetExtensionPointer("glGenBuffers");
        glMapBuffer = gltGetExtensionPointer("glMapBuffer");
        glUnmapBuffer = gltGetExtensionPointer("glUnmapBuffer");
    }
    else
    {
        glBindBuffer = gltGetExtensionPointer("glBindBufferARB");
        glBufferData = gltGetExtensionPointer("glBufferDataARB");
        glBufferSubData = gltGetExtensionPointer("glBufferSubDataARB");
        glDeleteBuffers = gltGetExtensionPointer("glDeleteBuffersARB");
        glGenBuffers = gltGetExtensionPointer("glGenBuffersARB");
        glMapBuffer = gltGetExtensionPointer("glMapBufferARB");
        glUnmapBuffer = gltGetExtensionPointer("glUnmapBufferARB");
    }

    if (!glBindBuffer || !glBufferData || !glBufferSubData || 
        !glDeleteBuffers || !glGenBuffers || !glMapBuffer || !glUnmapBuffer)
    {
        fprintf(stderr, "Not all entrypoints were available!\n");
        sleep(2);
        exit(0);
    }

    sphereVertexArray = (GLfloat *)malloc(sizeof(GLfloat) * 
                                          numSphereVertices * 3);
    if (!sphereVertexArray)
    {
        fprintf(stderr, "Unable to allocate system memory for vertex arrays!");
        sleep(2);
        exit(0);
    }

    fprintf(stdout, "Controls:\n");
    fprintf(stdout, "\tRight-click for menu\n\n");
    fprintf(stdout, "\tx/X\t\tMove +/- in x direction\n");
    fprintf(stdout, "\ty/Y\t\tMove +/- in y direction\n");
    fprintf(stdout, "\tz/Z\t\tMove +/- in z direction\n\n");
    fprintf(stdout, "\tq\t\tExit demo\n\n");
    
    // Generate a buffer object
    glGenBuffers(1, &bufferID);
    // Create the data store 
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 
                 numSphereVertices * 3, NULL, 
                 GL_STATIC_DRAW);

    // Set up vertex arrays
    RegenerateSphere();
    SetRenderingMethod();
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);

    // Black background
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f );

    // Hidden surface removal
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Set up some lighting state that never changes
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
}

void ProcessMenu(int value)
{
    switch(value)
    {
    case 1:
        useBufferObject = !useBufferObject;
        RegenerateSphere();
        SetRenderingMethod();
        if (useBufferObject)
        {
            glutChangeToMenuEntry(1, "Toggle buffer object usage (currently ON)", 1);
        }
        else
        {
            glutChangeToMenuEntry(1, "Toggle buffer object usage (currently OFF)", 1);
        }
        break;

    case 2:
        mapBufferObject = !mapBufferObject;
        RegenerateSphere();
        SetRenderingMethod();
        if (mapBufferObject)
        {
            glutChangeToMenuEntry(2, "Toggle mapped buffer object (currently ON)", 2);
        }
        else
        {
            glutChangeToMenuEntry(2, "Toggle mapped buffer object (currently OFF)", 2);
        }
        break;

    case 3:
        animating = !animating;
        if (animating)
        {
            glutChangeToMenuEntry(3, "Toggle animation (currently ON)", 3);
            // Establish streaming buffer object
            // Data will be loaded with subsequent calls to glBufferSubData
            glBindBuffer(GL_ARRAY_BUFFER, bufferID);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 
                         numSphereVertices * 3, NULL, GL_STREAM_DRAW);
        } 
        else
        {
            glutChangeToMenuEntry(3, "Toggle animation (currently OFF)", 3);
            // Establish static buffer object
            // Data will be loaded with subsequent calls to glBufferSubData
            glBindBuffer(GL_ARRAY_BUFFER, bufferID);
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 
                         numSphereVertices * 3, NULL, GL_STATIC_DRAW);
            RegenerateSphere();
            SetRenderingMethod();
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
        cameraPos[0] += 5.0f;
        break;
    case GLUT_KEY_RIGHT:
        cameraPos[0] -= 5.0f;
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

    glViewport(0, 0, windowWidth, windowHeight);
}

int main(int argc, char* argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Buffer Object Demo");
    glutReshapeFunc(ChangeSize);
    glutKeyboardFunc(KeyPressFunc);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);

    // Create the Menu
    glutCreateMenu(ProcessMenu);
    glutAddMenuEntry("Toggle buffer object usage (currently ON)", 1);
    glutAddMenuEntry("Toggle mapped buffer object (currently ON)", 2);
    glutAddMenuEntry("Toggle animation (currently ON)", 3);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    SetupRC();

    glutMainLoop();

    if (sphereVertexArray)
        free(sphereVertexArray);
    if (glDeleteBuffers)
        glDeleteBuffers(1, &bufferID);

    return 0;
}
