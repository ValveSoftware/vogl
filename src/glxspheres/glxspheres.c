/* Copyright (C)2007 Sun Microsystems, Inc.
 * Copyright (C)2011, 2013 D. R. Commander
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

/* This program's goal is to reproduce, as closely as possible, the image
   output of the nVidia SphereMark demo by R. Stephen Glanville using the
   simplest available rendering method.  GLXSpheres is meant primarily to
   serve as an image pipeline benchmark for VirtualGL. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <memory.h>

#define SDL_MAIN_HANDLED 1
#include "SDL.h"
#include "SDL_syswm.h"
#include "SDL_opengl.h"

#ifdef __linux__
  #include <sys/time.h>
  #include <GL/glx.h>
  #include <GL/glu.h>
  #include <X11/Xlib.h>
#elif defined(__APPLE__)
  #include <OpenGL/glu.h>
#else
  #include <GL/glu.h>
#endif

#define min(a, b) (((a) < (b)) ? (a) : (b))

static double rrtime()
{
#if defined(__linux__) || defined(__APPLE__)
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return ((unsigned long long)(cur_time.tv_sec) * 1000000ULL + (unsigned long long)(cur_time.tv_usec)) / 1000000.0;
#else
    LARGE_INTEGER Frequency, Time;
    if (QueryPerformanceFrequency(&Frequency)!=0)
    {
        QueryPerformanceCounter(&Time);
        return (double)Time.QuadPart / (double)Frequency.QuadPart;
    }
    else
    {
        return (double)GetTickCount() * 0.001;
    }
#endif
}

#define _throw(m)                                         \
    {                                                     \
        fprintf(stderr, "ERROR (%d): %s\n", __LINE__, m); \
        goto bailout;                                     \
    }
#define _catch(f)         \
    {                     \
        if ((f) == -1)    \
            goto bailout; \
    }

#define np2(i) ((i) > 0 ? (1 << (int)(log((double)(i)) / log(2.))) : 0)

#define MAXI (220. / 255.)
#define spherered(f) (float)fabs(MAXI * (2. * f - 1.))
#define spheregreen(f) (float)fabs(MAXI * (2. * fmod(f + 2. / 3., 1.) - 1.))
#define sphereblue(f) (float)fabs(MAXI * (2. * fmod(f + 1. / 3., 1.) - 1.))

#define DEF_WIDTH 1240
#define DEF_HEIGHT 900

#define DEF_SLICES 32
#define DEF_STACKS 32

#define NSPHERES 20

#define _2PI 6.283185307180

#define DEFBENCHTIME 2.0

enum
{
    GREY = 0,
    RED,
    GREEN,
    BLUE,
    YELLOW,
    MAGENTA,
    CYAN,
    NSCHEMES
};

int usestereo = 0, useci = 0, useimm = 0, interactive = 0;
int locolor = 0, maxframes = 0, totalframes = 0;
double benchtime = DEFBENCHTIME;
int ncolors = 0, colorscheme = GREY;

int spherelist = 0, fontlistbase = 0;
GLUquadricObj *spherequad = NULL;
int slices = DEF_SLICES, stacks = DEF_STACKS;
float x = 0., y = 0., z = -3.;
float outer_angle = 0., middle_angle = 0., inner_angle = 0.;
float lonesphere_color = 0.;

int width = DEF_WIDTH, height = DEF_HEIGHT;

int setcolorscheme(SDL_Window *window, int nc, int scheme)
{
    int i;
    Uint16 red[256];
    Uint16 green[256];
    Uint16 blue[256];

    if (scheme < 0 || scheme > NSCHEMES - 1 || nc > 256 || !window)
        _throw("Invalid argument");

    for (i = 0; i < nc; i++)
    {
        red[i] = 0;
        green[i] = 0;
        blue[i] = 0;

        if (scheme == GREY || scheme == RED || scheme == YELLOW || scheme == MAGENTA)
            red[i] = (i * (256 / nc)) << 8;
        if (scheme == GREY || scheme == GREEN || scheme == YELLOW || scheme == CYAN)
            green[i] = (i * (256 / nc)) << 8;
        if (scheme == GREY || scheme == BLUE || scheme == MAGENTA || scheme == CYAN)
            blue[i] = (i * (256 / nc)) << 8;
    }

    SDL_SetWindowGammaRamp(window, red, green, blue);
    return 0;

bailout:
    return -1;
}

void reshape(int w, int h)
{
    if (w <= 0)
        w = 1;
    if (h <= 0)
        h = 1;

    width = w;
    height = h;
}

void setspherecolor(float c)
{
    if (useci)
    {
        GLfloat ndx = c * (float)(ncolors - 1);
        GLfloat mat_ndxs[] = { ndx * 0.3f, ndx * 0.8f, ndx };
        glIndexf(ndx);
        glMaterialfv(GL_FRONT, GL_COLOR_INDEXES, mat_ndxs);
    }
    else
    {
        float mat[4] = { spherered(c), spheregreen(c), sphereblue(c), 0.25f };
        glColor3f(spherered(c), spheregreen(c), sphereblue(c));
        glMaterialfv(GL_FRONT, GL_AMBIENT, mat);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, mat);
    }
}

void renderspheres(int buf)
{
    int i;
    GLfloat xaspect, yaspect;
    GLfloat stereocameraoffset = 0.;
    GLfloat neardist = 1.5, fardist = 40., zeroparallaxdist = 17.;
    glDrawBuffer(buf);

    xaspect = (GLfloat)width / (GLfloat)(min(width, height));
    yaspect = (GLfloat)height / (GLfloat)(min(width, height));

    if (buf == GL_BACK_LEFT)
        stereocameraoffset = -xaspect * zeroparallaxdist / neardist * 0.035f;
    else if (buf == GL_BACK_RIGHT)
        stereocameraoffset = xaspect * zeroparallaxdist / neardist * 0.035f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-xaspect, xaspect, -yaspect, yaspect, neardist, fardist);
    glTranslatef(-stereocameraoffset, 0., 0.);
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, width, height);

    // Begin rendering
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(20.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Lone sphere
    glPushMatrix();
    glTranslatef(0., 0., z);
    setspherecolor(lonesphere_color);
    if (useimm)
        gluSphere(spherequad, 1.3, slices, stacks);
    else
        glCallList(spherelist);
    glPopMatrix();

    // Outer ring
    glPushMatrix();
    glRotatef(outer_angle, 0., 0., 1.);
    for (i = 0; i < NSPHERES; i++)
    {
        double f = (double)i / (double)NSPHERES;
        glPushMatrix();
        glTranslatef((float)sin(_2PI * f) * 5.0f, (float)cos(_2PI * f) * 5.0f, -10.0f);
        setspherecolor((float)f);
        if (useimm)
            gluSphere(spherequad, 1.3, slices, stacks);
        else
            glCallList(spherelist);
        glPopMatrix();
    }
    glPopMatrix();

    // Middle ring
    glPushMatrix();
    glRotatef(middle_angle, 0., 0., 1.);
    for (i = 0; i < NSPHERES; i++)
    {
        double f = (double)i / (double)NSPHERES;
        glPushMatrix();
        glTranslatef((float)sin(_2PI * f) * 5.0f, (float)cos(_2PI * f) * 5.0f, -17.0f);
        setspherecolor((float)f);
        if (useimm)
            gluSphere(spherequad, 1.3, slices, stacks);
        else
            glCallList(spherelist);
        glPopMatrix();
    }
    glPopMatrix();

    // Inner ring
    glPushMatrix();
    glRotatef(inner_angle, 0., 0., 1.);
    for (i = 0; i < NSPHERES; i++)
    {
        double f = (double)i / (double)NSPHERES;
        glPushMatrix();
        glTranslatef((float)sin(_2PI * f) * 5.0f, (float)cos(_2PI * f) * 5.0f, -29.0f);
        setspherecolor((float)f);
        if (useimm)
            gluSphere(spherequad, 1.3, slices, stacks);
        else
            glCallList(spherelist);
        glPopMatrix();
    }
    glPopMatrix();

    // Move the eye back to the middle
    glMatrixMode(GL_PROJECTION);
    glTranslatef(stereocameraoffset, 0., 0.);
}

void buildfont()
{
#ifdef __linux__
    Display *dpy = XOpenDisplay(NULL);

    if (dpy)
    {
        XFontStruct *fontinfo = XLoadQueryFont(dpy, "fixed");
        if (fontinfo)
        {
            int minchar = fontinfo->min_char_or_byte2;
            int maxchar = fontinfo->max_char_or_byte2;

            fontlistbase = glGenLists(maxchar + 1);
            glXUseXFont(fontinfo->fid, minchar, maxchar - minchar + 1, fontlistbase + minchar);

            XFreeFont(dpy, fontinfo);
        }
    }

    XCloseDisplay(dpy);

#elif defined(__APPLE__)
	/* TODO */
	fprintf(stderr, "UNIMPLEMENTED buildfont\n");

#else
    HDC hdc = wglGetCurrentDC();

    SelectObject(hdc, GetStockObject(SYSTEM_FONT));
    wglUseFontBitmaps(hdc, 0, 255, fontlistbase);
#endif
}

int display(SDL_Window *window, int advance)
{
    static int first = 1;
    static double start = 0., elapsed = 0., mpixels = 0.;
    static unsigned long frames = 0;
    static char temps[256];
    GLfloat xaspect, yaspect;

    if (first)
    {
        buildfont();

        GLfloat id4[] = { 1., 1., 1., 1. };
        GLfloat light0_amb[] = { 0.3f, 0.3f, 0.3f, 1.0f };
        GLfloat light0_dif[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        GLfloat light0_pos[] = { 1., 1., 1., 0. };

        spherelist = glGenLists(1);
        if (!(spherequad = gluNewQuadric()))
            _throw("Could not allocate GLU quadric object");
        glNewList(spherelist, GL_COMPILE);
        gluSphere(spherequad, 1.3, slices, stacks);
        glEndList();

        if (!locolor)
        {
            if (useci)
            {
                glMaterialf(GL_FRONT, GL_SHININESS, 50.);
            }
            else
            {
                glMaterialfv(GL_FRONT, GL_AMBIENT, id4);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, id4);
                glMaterialfv(GL_FRONT, GL_SPECULAR, id4);
                glMaterialf(GL_FRONT, GL_SHININESS, 50.);

                glLightfv(GL_LIGHT0, GL_AMBIENT, light0_amb);
                glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_dif);
                glLightfv(GL_LIGHT0, GL_SPECULAR, id4);
            }
            glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
            glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 180.);
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
        }

        if (locolor)
            glShadeModel(GL_FLAT);
        else
            glShadeModel(GL_SMOOTH);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        SDL_snprintf(temps, 255, "Measuring performance ...");

        first = 0;
    }

    if (advance)
    {
        z -= 0.5;
        if (z < -29.)
        {
            if (useci)
            {
                colorscheme = (colorscheme + 1) % NSCHEMES;
                _catch(setcolorscheme(window, ncolors, colorscheme));
            }

            z = -3.5;
        }

        outer_angle += 0.1f;
        if (outer_angle > 360.0f)
            outer_angle -= 360.0f;
        middle_angle -= 0.37f;
        if (middle_angle < -360.0f)
            middle_angle += 360.0f;
        inner_angle += 0.63f;
        if (inner_angle > 360.0f)
            inner_angle -= 360.0f;
        lonesphere_color += 0.005f;
        if (lonesphere_color > 1.0f)
            lonesphere_color -= 1.0f;
    }

    if (usestereo)
    {
        renderspheres(GL_BACK_LEFT);
        renderspheres(GL_BACK_RIGHT);
    }
    else
    {
        renderspheres(GL_BACK);
    }

    glDrawBuffer(GL_BACK);
    glPushAttrib(GL_CURRENT_BIT);
    glPushAttrib(GL_LIST_BIT);
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);

    if (useci)
        glIndexf(254.);
    else
        glColor3f(1., 1., 1.);

    xaspect = (GLfloat)width / (GLfloat)(min(width, height));
    yaspect = (GLfloat)height / (GLfloat)(min(width, height));
    glRasterPos3f(-0.95f * xaspect, -0.95f * yaspect, -1.5f);

    glListBase(fontlistbase);
    glCallLists((GLsizei)strlen(temps), GL_UNSIGNED_BYTE, temps);
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();

    SDL_GL_SwapWindow(window);

    if (start > 0.)
    {
        elapsed += rrtime() - start;
        frames++;
        totalframes++;
        mpixels += (double)width * (double)height / 1000000.;
        if (elapsed > benchtime || (maxframes && totalframes > maxframes))
        {
            SDL_snprintf(temps, 255, "%f frames/sec - %f Mpixels/sec",
                     (double)frames / elapsed, mpixels / elapsed);
            printf("%s\n", temps);
            elapsed = mpixels = 0.;
            frames = 0;
        }
    }
    if (maxframes && totalframes > maxframes)
        goto bailout;

    start = rrtime();

    return 0;

bailout:
    if (spherequad)
    {
        gluDeleteQuadric(spherequad);
        spherequad = NULL;
    }
    return -1;
}

int event_loop(SDL_Window *window)
{
    while (1)
    {
        SDL_Event event;
        int advance = !interactive;
        int dodisplay = advance;

        while (SDL_PollEvent(&event))
        {
            switch(event.type)
            {
            case SDL_QUIT:
                return 0;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                case SDLK_q:
                    return 0;
                }
                break;

            case SDL_MOUSEMOTION:
                // Advance when moving the mouse if the mouse button is down.
                if (!event.motion.state)
                    break;
                // Fall through.
            case SDL_MOUSEBUTTONDOWN:
                dodisplay = 1;
                advance = 1;
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                    reshape(event.window.data1, event.window.data2);
                    break;
                }
            }
        }

        if (dodisplay)
        {
            if (display(window, advance) == -1)
                return -1;
        }
    }
}

void go_fullscreen(SDL_Window *window, int index)
{
    Uint32 flags;
    struct SDL_Rect rect = { 0, 0, 0, 0 }; 

    if (index < 0)
        index = 0;
    else if (index > SDL_GetNumVideoDisplays())
        index = SDL_GetNumVideoDisplays();

    SDL_GetDisplayBounds(index, &rect);
    width = rect.w;
    height = rect.h;

    flags = SDL_GetWindowFlags(window);
    if (flags & SDL_WINDOW_FULLSCREEN)
    {
        SDL_SetWindowFullscreen(window, SDL_FALSE);
        SDL_Delay(15);
    }    

    SDL_SetWindowPosition(window, rect.x, rect.y);
    SDL_SetWindowFullscreen(window, SDL_TRUE);
}

void usage(char **argv)
{
    printf("\nUSAGE: %s [options]\n\n", argv[0]);
    printf("Options:\n");
    printf("-c = Use color index rendering (default is RGB)\n");
    printf("-fs = Full-screen mode\n");
    printf("-i = Interactive mode.  Frames advance in response to mouse movement\n");
    printf("-l = Use fewer than 24 colors (to force non-JPEG encoding in TurboVNC)\n");
    printf("-m = Use immediate mode rendering (default is display list)\n");
    printf("-p <p> = Use (approximately) <p> polygons to render scene\n");
    printf("-s = Use stereographic rendering initially\n");
    printf("     (this can be switched on and off in the application)\n");
    printf("-32 = Use 32-bit visual (default is 24-bit)\n");
    printf("-f <n> = max frames to render\n");
    printf("-bt <t> = print benchmark results every <t> seconds (default=%.1f)\n",
           DEFBENCHTIME);
    printf("-w <wxh> = specify window width and height\n");
    printf("-sc <s> = Create window on X screen # <s>\n");
    printf("\n");
}

int main(int argc, char **argv)
{
    int i;
    int ret = -1;
    int screen = -1;
    int usealpha = 0;
    int fullscreen = 0;
    SDL_SysWMinfo info;
    SDL_Window *window = NULL;
    SDL_GLContext *ctx = NULL;

    for (i = 0; i < argc; i++)
    {
        if (!SDL_strncasecmp(argv[i], "-h", 2) || !SDL_strncasecmp(argv[i], "-?", 2))
        {
            usage(argv);
            exit(0);
        }

        if (!SDL_strncasecmp(argv[i], "-c", 2))
            useci = 1;
        else if (!SDL_strncasecmp(argv[i], "-i", 2))
            interactive = 1;
        else if (!SDL_strncasecmp(argv[i], "-l", 2))
            locolor = 1;
        else if (!SDL_strncasecmp(argv[i], "-m", 2))
            useimm = 1;
        else if (!SDL_strncasecmp(argv[i], "-32", 3))
            usealpha = 1;
        else if (!SDL_strncasecmp(argv[i], "-w", 2) && i < argc - 1)
        {
            int w = 0, h = 0;

            if (sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0)
            {
                width = w;
                height = h;
                printf("Window dimensions: %d x %d\n", width, height);
            }
        }
        else if (!SDL_strncasecmp(argv[i], "-p", 2) && i < argc - 1)
        {
            int npolys = atoi(argv[++i]);
            if (npolys > 0)
            {
                slices = stacks = (int)(sqrt((double)npolys / ((double)(3 * NSPHERES + 1))));
                if (slices < 1)
                    slices = stacks = 1;
            }
        }
        else if (!SDL_strncasecmp(argv[i], "-fs", 3))
        {
            fullscreen = 1;
        }
        else if (!SDL_strncasecmp(argv[i], "-f", 2) && i < argc - 1)
        {
            int mf = atoi(argv[++i]);
            if (mf > 0)
            {
                maxframes = mf;
                printf("Number of frames to render: %d\n", maxframes);
            }
        }
        else if (!SDL_strncasecmp(argv[i], "-bt", 3) && i < argc - 1)
        {
            double temp = atof(argv[++i]);
            if (temp > 0.0)
                benchtime = temp;
        }
        else if (!SDL_strncasecmp(argv[i], "-sc", 3) && i < argc - 1)
        {
            int sc = atoi(argv[++i]);
            if (sc > 0)
            {
                screen = sc;
                printf("Rendering to screen %d\n", screen);
            }
        }
        else if (!SDL_strncasecmp(argv[i], "-s", 2))
        {
            usestereo = 1;
        }
    }

    if (useci)
    {
        SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, usealpha ? 8 : 0); 
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    }
    else
    {
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, usealpha ? 8 : 0); 
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_STEREO, usestereo); 
    }

    // Initialize SDL for video output.
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        _throw("Unable to initialize SDL");

    // Create our OpenGL window.
    window = SDL_CreateWindow("vogltest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
        _throw("Unable to create OpenGL window");

    ctx = SDL_GL_CreateContext(window);
    if (!ctx)
        _throw("Unable to create OpenGL context");

    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) == SDL_TRUE)
    {
        const char *subsystem = "Unknown system";
        switch(info.subsystem)
        {
        case SDL_SYSWM_UNKNOWN:   break;
        case SDL_SYSWM_WINDOWS:   subsystem = "Microsoft Windows(TM)";  break;
        case SDL_SYSWM_X11:       subsystem = "X Window System";        break;
        case SDL_SYSWM_DIRECTFB:  subsystem = "DirectFB";               break;
        case SDL_SYSWM_COCOA:     subsystem = "Apple OS X";             break;
        case SDL_SYSWM_UIKIT:     subsystem = "UIKit";                  break;
        case SDL_SYSWM_WAYLAND:   subsystem = "Wayland";                break;
#if SDL_VERSION_ATLEAST(2, 0, 2)
        case SDL_SYSWM_MIR:       subsystem = "Mir";                    break;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 3)
        case SDL_SYSWM_WINRT:     subsystem = "WinRT";                  break;
#endif
        }

        printf("Running SDL version %d.%d.%d on '%s'\n", 
               (int)info.version.major,
               (int)info.version.minor,
               (int)info.version.patch,
               subsystem); 

        if (useci)
        {
#ifdef __linux__
            int n;
            XVisualInfo vinfo_template;
            XVisualInfo *vinfo;

            memset(&vinfo_template, 0, sizeof(vinfo_template));
            vinfo_template.colormap_size = 256;
            vinfo = XGetVisualInfo(info.info.x11.display, VisualColormapSizeMask, &vinfo_template, &n);
            if (vinfo && (n > 0))
            {
                ncolors = np2(vinfo->colormap_size);
            }
#else
            // TODO: Just assume 256 for non-X11 platforms?
            ncolors = 256;
#endif

            if (ncolors < 32)
                _throw("Color map is not large enough");

            _catch(setcolorscheme(window, ncolors, colorscheme));
        }
    }

    fprintf(stderr, "Polygons in scene: %d\n", (NSPHERES * 3 + 1) * slices * stacks);

    if (fullscreen)
    {
        go_fullscreen(window, screen);
    }

    ret = event_loop(window);

bailout:
    SDL_Quit();
    return ret;
}
