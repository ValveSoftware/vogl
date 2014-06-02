/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

// File: vogltest.cpp
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "vogl_core.h"
#include "vogl_colorized_console.h"
#include "vogl_command_line_params.h"

#define SDL_MAIN_HANDLED 1
#include "SDL.h"
#include "SDL_opengl.h"

// External function which has a bunch of voglcore tests.
int run_voglcore_tests(int argc, char *argv[]);

#define PI 3.14159265

#define CALL_GL(name) name

typedef struct RenderContextRec
{
    int nWinWidth;
    int nWinHeight;
    int nMousePosX;
    int nMousePosY;

} RenderContext;

void SetupGLState(RenderContext *rcx)
{
    float aspectRatio = rcx->nWinHeight ? (float)rcx->nWinWidth / (float)rcx->nWinHeight : 1.0f;
    float fYTop = 0.05f;
    float fYBottom = -fYTop;
    float fXLeft = fYTop * aspectRatio;
    float fXRight = fYBottom * aspectRatio;

    CALL_GL(glViewport)(0, 0, rcx->nWinWidth, rcx->nWinHeight);
    CALL_GL(glScissor)(0, 0, rcx->nWinWidth, rcx->nWinHeight);

    CALL_GL(glClearColor)(0.0f, 1.0f, 1.0f, 1.0f);
    CALL_GL(glClear)(GL_COLOR_BUFFER_BIT);

    // Clear matrix stack
    CALL_GL(glMatrixMode)(GL_PROJECTION);
    CALL_GL(glLoadIdentity)();
    CALL_GL(glMatrixMode)(GL_MODELVIEW);
    CALL_GL(glLoadIdentity)();

    // Set the frustrum
    CALL_GL(glFrustum)(fXLeft, fXRight, fYBottom, fYTop, 0.1f, 100.f);
}

void DrawCircle()
{
    float fx = 0.0;
    float fy = 0.0;

    int nDegrees = 0;

    // Use a triangle fan with 36 tris for the circle
    CALL_GL(glBegin)(GL_TRIANGLE_FAN);

    CALL_GL(glVertex2f)(0.0, 0.0);
    for (nDegrees = 0; nDegrees < 360; nDegrees += 10)
    {
        fx = sin((float)nDegrees * PI / 180);
        fy = cos((float)nDegrees * PI / 180);
        CALL_GL(glVertex2f)(fx, fy);
    }
    CALL_GL(glVertex2f)(0.0f, 1.0f);

    CALL_GL(glEnd)();
}

void Draw(RenderContext *rcx)
{
    float fRightX = 0.0;
    float fRightY = 0.0;
    float fLeftX = 0.0;
    float fLeftY = 0.0;

    int nLtEyePosX = (rcx->nWinWidth) / 2 - (0.3 * ((rcx->nWinWidth) / 2));
    int nLtEyePosY = (rcx->nWinHeight) / 2;
    int nRtEyePosX = (rcx->nWinWidth) / 2 + (0.3 * ((rcx->nWinWidth) / 2));
    int nRtEyePosY = (rcx->nWinHeight) / 2;

    double fLtVecMag = 100;
    double fRtVecMag = 100;

    // Use the eyeball width as the minimum
    double fMinLength = (0.1 * ((rcx->nWinWidth) / 2));

    // Calculate the length of the vector from the eyeball
    // to the mouse pointer
    fLtVecMag = sqrt(pow((double)(rcx->nMousePosX - nLtEyePosX), 2.0) +
                     pow((double)(rcx->nMousePosY - nLtEyePosY), 2.0));

    fRtVecMag = sqrt(pow((double)(rcx->nMousePosX - nRtEyePosX), 2.0) +
                     pow((double)(rcx->nMousePosY - nRtEyePosY), 2.0));

    // Clamp the minimum magnatude
    if (fRtVecMag < fMinLength)
        fRtVecMag = fMinLength;
    if (fLtVecMag < fMinLength)
        fLtVecMag = fMinLength;

    // Normalize the vector components
    fLeftX = (float)(rcx->nMousePosX - nLtEyePosX) / fLtVecMag;
    fLeftY = -1.0 * (float)(rcx->nMousePosY - nLtEyePosY) / fLtVecMag;
    fRightX = (float)(rcx->nMousePosX - nRtEyePosX) / fRtVecMag;
    fRightY = -1.0 * ((float)(rcx->nMousePosY - nRtEyePosY) / fRtVecMag);

    CALL_GL(glClear)(GL_COLOR_BUFFER_BIT);

    // Clear matrix stack
    CALL_GL(glMatrixMode)(GL_PROJECTION);
    CALL_GL(glLoadIdentity)();

    CALL_GL(glMatrixMode)(GL_MODELVIEW);
    CALL_GL(glLoadIdentity)();

    // Draw left eyeball
    CALL_GL(glColor3f)(1.0, 1.0, 1.0);
    CALL_GL(glScalef)(0.20, 0.20, 1.0);
    CALL_GL(glTranslatef)(-1.5, 0.0, 0.0);
    DrawCircle();

    // Draw black
    CALL_GL(glColor3f)(0.0, 0.0, 0.0);
    CALL_GL(glScalef)(0.40, 0.40, 1.0);
    CALL_GL(glTranslatef)(fLeftX, fLeftY, 0.0);
    DrawCircle();

    // Draw right eyeball
    CALL_GL(glColor3f)(1.0, 1.0, 1.0);
    CALL_GL(glLoadIdentity)();
    CALL_GL(glScalef)(0.20, 0.20, 1.0);
    CALL_GL(glTranslatef)(1.5, 0.0, 0.0);
    DrawCircle();

    // Draw black
    CALL_GL(glColor3f)(0.0, 0.0, 0.0);
    CALL_GL(glScalef)(0.40, 0.40, 1.0);
    CALL_GL(glTranslatef)(fRightX, fRightY, 0.0);
    DrawCircle();

    // Clear matrix stack
    CALL_GL(glMatrixMode)(GL_MODELVIEW);
    CALL_GL(glLoadIdentity)();

    // Draw Nose
    CALL_GL(glColor3f)(0.5, 0.0, 0.7);
    CALL_GL(glScalef)(0.20, 0.20, 1.0);
    CALL_GL(glTranslatef)(0.0, -1.5, 0.0);

#if 0
	CALL_GL(glBegin)(GL_TRIANGLES);
	CALL_GL(glVertex2f)(0.0, 1.0);
	CALL_GL(glVertex2f)(-0.5, -1.0);
	CALL_GL(glVertex2f)(0.5, -1.0);
	CALL_GL(glEnd)();
#endif

    float verts[3][2] =
    {
        { 0.0, 1.0 },
        { -0.5, -1.0 },
        { 0.5, -1.0 }
    };

    CALL_GL(glVertexPointer)(2, GL_FLOAT, sizeof(float) * 2, verts);
    CALL_GL(glEnableClientState)(GL_VERTEX_ARRAY);

    CALL_GL(glDrawArrays)(GL_TRIANGLES, 0, 3);

    CALL_GL(glDisableClientState)(GL_VERTEX_ARRAY);
    CALL_GL(glVertexPointer)(4, GL_FLOAT, 0, NULL);
}

int main(int argc, char *argv[])
{
    int ret = EXIT_FAILURE;

    vogl_core_init();

    if (argc > 1)
    {
        // If we were passed any args, send them to voglcore test function.
        return run_voglcore_tests(argc, argv);
    }

    vogl::colorized_console::init();
    vogl::colorized_console::set_exception_callback();

    vogl::console::set_tool_prefix("(vogltest) ");

    int done = 0;
    RenderContext rcx;
    SDL_Window *window;

    rcx.nWinWidth = 400;
    rcx.nWinHeight = 200;
    rcx.nMousePosX = 0;
    rcx.nMousePosY = 0;

    // Initialize SDL for video output.
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        vogl_error_printf("Unable to initialize SDL: %s\n", SDL_GetError());
        goto err;
    }

    // Create our OpenGL window.
    window = SDL_CreateWindow("vogltest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, rcx.nWinWidth, rcx.nWinHeight,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE );
    if (!window)
    {
        vogl_error_printf("Unable to create OpenGL window: %s\n", SDL_GetError());
        goto err;
    }

    if (!SDL_GL_CreateContext(window))
    {
        vogl_error_printf("Unable to create OpenGL context: %s\n", SDL_GetError());
        goto err;
    }

    SetupGLState(&rcx);

    while (!done)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            switch(event.type)
            {
            case SDL_QUIT:
                done = 1;
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                case SDLK_q:
                    done = 1;
                    break;
                }

            case SDL_MOUSEMOTION:
                rcx.nMousePosX = event.motion.x;
                rcx.nMousePosY = event.motion.y;
                Draw(&rcx);
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                    rcx.nWinWidth = event.window.data1;
                    rcx.nWinHeight = event.window.data2;
                    SetupGLState(&rcx);
                    break;
                }
            }
        }

        Draw(&rcx);
        SDL_GL_SwapWindow(window);
    }

    vogl::colorized_console::deinit();

    ret = EXIT_SUCCESS;

err:
    SDL_Quit();
    return ret;
}
