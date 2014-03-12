// ShadowMap.c
// OpenGL SuperBible, Chapter 18
// Demonstrates shadow mapping
// Program by Benjamin Lipchak

#include "../../common/openglsb.h"	// System and OpenGL Stuff
#include "../../common/gltools.h"	// OpenGL toolkit
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#define GL_DEPTH_TEXTURE_MODE             0x884B
#define GL_TEXTURE_COMPARE_MODE           0x884C
#define GL_COMPARE_R_TO_TEXTURE           0x884E
#define GL_TEXTURE_COMPARE_FAIL_VALUE_ARB 0x80BF

GLboolean ambientShadowAvailable = GL_FALSE;
GLboolean controlCamera = GL_TRUE;      // xyz keys will control lightpos
GLboolean noShadows = GL_FALSE;         // normal lighting
GLboolean showShadowMap = GL_FALSE;     // show the shadowmap texture

GLfloat factor = 4.0f;                  // for polygon offset

GLint windowWidth = 512;                // window size
GLint windowHeight = 512;

GLint shadowSize = 512;                 // set based on window size
GLuint shadowTextureID;

GLfloat ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f};
GLfloat diffuseLight[] = { 0.7f, 0.7f, 0.7f, 1.0f};
GLfloat noLight[]      = { 0.0f, 0.0f, 0.0f, 1.0f};
GLfloat lightPos[]     = { 100.0f, 300.0f, 100.0f, 1.0f};
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

// Called to regenerate the shadow map
void RegenerateShadowMap(void)
{
    GLfloat lightToSceneDistance, nearPlane, fieldOfView;
    GLfloat lightModelview[16], lightProjection[16];

    // Save the depth precision for where it's useful
    lightToSceneDistance = sqrt(lightPos[0] * lightPos[0] + 
                                lightPos[1] * lightPos[1] + 
                                lightPos[2] * lightPos[2]);
    nearPlane = lightToSceneDistance - 150.0f;
    if (nearPlane < 50.0f)
        nearPlane = 50.0f;
    // Keep the scene filling the depth texture
    fieldOfView = 17000.0f / lightToSceneDistance;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fieldOfView, 1.0f, nearPlane, nearPlane + 300.0f);
    glGetFloatv(GL_PROJECTION_MATRIX, lightProjection);
    // Switch to light's point of view
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(lightPos[0], lightPos[1], lightPos[2], 
              0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    glGetFloatv(GL_MODELVIEW_MATRIX, lightModelview);
    glViewport(0, 0, shadowSize, shadowSize);

    // Clear the window with current clearing color
    glClear(GL_DEPTH_BUFFER_BIT);

    // All we care about here is resulting depth values
    glShadeModel(GL_FLAT);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_NORMALIZE);
    glColorMask(0, 0, 0, 0);

    // Overcome imprecision
    glEnable(GL_POLYGON_OFFSET_FILL);

    // Draw objects in the scene
    DrawModels();

    // Copy depth values into depth texture
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
                     0, 0, shadowSize, shadowSize, 0);

    // Restore normal drawing state
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glColorMask(1, 1, 1, 1);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Set up texture matrix for shadow map projection
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.5f, 0.5f, 0.5f);
    glScalef(0.5f, 0.5f, 0.5f);
    glMultMatrixf(lightProjection);
    glMultMatrixf(lightModelview);
}

// Called to draw scene
void RenderScene(void)
{
    // Track camera angle
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 1.0f, 1.0f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(cameraPos[0], cameraPos[1], cameraPos[2], 
              0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    glViewport(0, 0, windowWidth, windowHeight);
    
    // Track light position
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Clear the window with current clearing color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (showShadowMap)
    {
        // Display shadow map for educational purposes
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Show the shadowMap at its actual size relative to window
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(-1.0f, -1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(((GLfloat)shadowSize/(GLfloat)windowWidth)*2.0-1.0f, 
                       -1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(((GLfloat)shadowSize/(GLfloat)windowWidth)*2.0-1.0f, 
                       ((GLfloat)shadowSize/(GLfloat)windowHeight)*2.0-1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(-1.0f, 
                       ((GLfloat)shadowSize/(GLfloat)windowHeight)*2.0-1.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        gluPerspective(45.0f, 1.0f, 1.0f, 1000.0f);
        glMatrixMode(GL_MODELVIEW);
    }
    else if (noShadows)
    {
        // Set up some simple lighting
        glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);

        // Draw objects in the scene
        DrawModels();
    }
    else
    {
        GLfloat sPlane[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        GLfloat tPlane[4] = {0.0f, 1.0f, 0.0f, 0.0f};
        GLfloat rPlane[4] = {0.0f, 0.0f, 1.0f, 0.0f};
        GLfloat qPlane[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        if (!ambientShadowAvailable)
        {
            GLfloat lowAmbient[4] = {0.1f, 0.1f, 0.1f, 1.0f};
            GLfloat lowDiffuse[4] = {0.35f, 0.35f, 0.35f, 1.0f};

            // Because there is no support for an "ambient"
            // shadow compare fail value, we'll have to
            // draw an ambient pass first...
            glLightfv(GL_LIGHT0, GL_AMBIENT, lowAmbient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, lowDiffuse);

            // Draw objects in the scene
            DrawModels();

            // Enable alpha test so that shadowed fragments are discarded
            glAlphaFunc(GL_GREATER, 0.9f);
            glEnable(GL_ALPHA_TEST);
        }

        glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);

        // Set up shadow comparison
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, 
                        GL_COMPARE_R_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Set up the eye plane for projecting the shadow map on the scene
        glEnable(GL_TEXTURE_GEN_S);
        glEnable(GL_TEXTURE_GEN_T);
        glEnable(GL_TEXTURE_GEN_R);
        glEnable(GL_TEXTURE_GEN_Q);
        glTexGenfv(GL_S, GL_EYE_PLANE, sPlane);
        glTexGenfv(GL_T, GL_EYE_PLANE, tPlane);
        glTexGenfv(GL_R, GL_EYE_PLANE, rPlane);
        glTexGenfv(GL_Q, GL_EYE_PLANE, qPlane);

        // Draw objects in the scene
        DrawModels();

        glDisable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        glDisable(GL_TEXTURE_GEN_R);
        glDisable(GL_TEXTURE_GEN_Q);
    }
    
    if (glGetError() != GL_NO_ERROR)
        fprintf(stderr, "GL Error!\n");

    // Flush drawing commands
    glutSwapBuffers();
}

// This function does any needed initialization on the rendering
// context. 
void SetupRC()
{
    const GLubyte *version;

    fprintf(stdout, "Shadow Mapping Demo\n\n");

    // Make sure required functionality is available!
    version = glGetString(GL_VERSION);
    if (((version[0] != '1') || (version[1] != '.') || 
         (version[2] < '4') || (version[2] > '9')) &&   // 1.4+
        (!gltIsExtSupported("GL_ARB_shadow")))
    {
        fprintf(stderr, "Neither OpenGL 1.4 nor GL_ARB_shadow"
                        " extension is available!\n");
        usleep(2000);
        exit(0);
    }

    // Check for optional extension
    if (gltIsExtSupported("GL_ARB_shadow_ambient"))
    {
        ambientShadowAvailable = GL_TRUE;
    }
    else
    {
        fprintf(stderr, "GL_ARB_shadow_ambient extension not available!\n");
        fprintf(stderr, "Extra ambient rendering pass will be required.\n\n");
        usleep(2000);
    }

    fprintf(stdout, "Controls:\n");
    fprintf(stdout, "\tRight-click for menu\n\n");
    fprintf(stdout, "\tx/X\t\tMove +/- in x direction\n");
    fprintf(stdout, "\ty/Y\t\tMove +/- in y direction\n");
    fprintf(stdout, "\tz/Z\t\tMove +/- in z direction\n\n");
    fprintf(stdout, "\tf/F\t\tChange polygon offset factor +/-\n\n");
    fprintf(stdout, "\tq\t\tExit demo\n\n");
    
    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );

    // Hidden surface removal
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glPolygonOffset(factor, 0.0f);

    // Set up some lighting state that never changes
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHT0);

    // Set up some texture state that never changes
    glGenTextures(1, &shadowTextureID);
    glBindTexture(GL_TEXTURE_2D, shadowTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
    if (ambientShadowAvailable)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FAIL_VALUE_ARB, 
                        0.5f);
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);

    RegenerateShadowMap();
}

void ProcessMenu(int value)
{
    switch(value)
    {
    case 1:
        noShadows = !noShadows;
        showShadowMap = GL_FALSE;
        if (noShadows)
        {
            glutChangeToMenuEntry(1, "Toggle shadows (currently OFF)", 1);
        }
        else
        {
            glutChangeToMenuEntry(1, "Toggle shadows (currently ON)", 1);
        }
        glutChangeToMenuEntry(2, "Toggle show shadowmap (currently OFF)", 2);
        break;

    case 2:
        showShadowMap = !showShadowMap;
        noShadows = GL_FALSE;
        if (showShadowMap)
        {
            glutChangeToMenuEntry(2, "Toggle show shadowmap (currently ON)", 2);
        }
        else
        {
            glutChangeToMenuEntry(2, "Toggle show shadowmap (currently OFF)", 2);
        }
        glutChangeToMenuEntry(1, "Toggle shadows (currently ON)", 1);
        break;

    case 3:
        controlCamera = !controlCamera;
        if (controlCamera)
        {
            glutChangeToMenuEntry(3, "Switch to LIGHT control (currently CAMERA)", 3);
        }
        else
        {
            glutChangeToMenuEntry(3, "Switch to CAMERA control (currently LIGHT)", 3);
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
    case 'f':
        factor++;
        glPolygonOffset(factor, 0.0f);
        RegenerateShadowMap();
        break;
    case 'F':
        factor--;
        glPolygonOffset(factor, 0.0f);
        RegenerateShadowMap();
        break;
    case 'x':
        if (controlCamera)
            cameraPos[0] += 5.0f;
        else
            lightPos[0] += 5.0f;
        break;
    case 'X':
        if (controlCamera)
            cameraPos[0] -= 5.0f;
        else
            lightPos[0] -= 5.0f;
        break;
    case 'y':
        if (controlCamera)
            cameraPos[1] += 5.0f;
        else
            lightPos[1] += 5.0f;
        break;
    case 'Y':
        if (controlCamera)
            cameraPos[1] -= 5.0f;
        else
            lightPos[1] -= 5.0f;
        break;
    case 'z':
        if (controlCamera)
            cameraPos[2] += 5.0f;
        else
            lightPos[2] += 5.0f;
        break;
    case 'Z':
        if (controlCamera)
            cameraPos[2] -= 5.0f;
        else
            lightPos[2] -= 5.0f;
        break;
    case 'q':
    case 'Q':
    case 27 : /* ESC */
        exit(0);
    }

    // We don't need to regenerate the shadow map
    // if only the camera angle changes
    if (!controlCamera)
        RegenerateShadowMap();

    // Refresh the Window
    glutPostRedisplay();
}

void SpecialKeys(int key, int x, int y)
{
    switch (key)
    {
    case GLUT_KEY_LEFT:
        if (controlCamera)
            cameraPos[0] -= 5.0f;
        else
            lightPos[0] -= 5.0f;
        break;
    case GLUT_KEY_RIGHT:
        if (controlCamera)
            cameraPos[0] += 5.0f;
        else
            lightPos[0] += 5.0f;
        break;
    case GLUT_KEY_UP:
        if (controlCamera)
            cameraPos[2] -= 5.0f;
        else
            lightPos[2] -= 5.0f;
        break;
    case GLUT_KEY_DOWN:
        if (controlCamera)
            cameraPos[2] += 5.0f;
        else
            lightPos[2] += 5.0f;
        break;
    default:
        break;
    }

    // We don't need to regenerate the shadow map
    // if only the camera angle changes
    if (!controlCamera)
        RegenerateShadowMap();

    // Refresh the Window
    glutPostRedisplay();
}

void ChangeSize(int w, int h)
{
    GLint i;
    
    windowWidth = w;
    windowHeight = h;
    
    // Find the largest power of two that will fit in window
    if (w > h)
        shadowSize = h;
    else
        shadowSize = w;

    // Try each size until we get one that's too big
    i = 0;
    while ((1 << i) <= shadowSize)
        i++;
    shadowSize = (1 << (i-1));
}

int main(int argc, char* argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Shadow Mapping Demo");
    glutReshapeFunc(ChangeSize);
    glutKeyboardFunc(KeyPressFunc);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);

    // Create the Menu
    glutCreateMenu(ProcessMenu);
    glutAddMenuEntry("Toggle shadows (currently ON)", 1);
    glutAddMenuEntry("Toggle show shadowmap (currently OFF)", 2);
    glutAddMenuEntry("Switch to LIGHT control (currently CAMERA)", 3);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    SetupRC();

    glutMainLoop();
    return 0;
}
