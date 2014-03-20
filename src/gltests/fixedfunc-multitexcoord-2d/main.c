#include "testcommon.h"

/* Window resolution */
#define WINDOW_WIDTH 512
#define WINDOW_HEIGHT 512

/* Window title */
#define WINDOW_TITLE "Fixed function multiTexCoord 2D test"

int main( /* int argc, char* args[] */ )
{
    /* The window */
    SDL_Window* window = NULL;

    /* The event structure */
    SDL_Event event;

    /* The game loop flag */
    _Bool running = true;

    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL2 could not initialize! SDL2_Error: %s\n", SDL_GetError() );
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    //The Compatibility profile does not exist with GL 2.1
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    window = SDL_CreateWindow(
            WINDOW_TITLE,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

    if (window == NULL) {
        printf("unable to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext maincontext = SDL_GL_CreateContext(window); /* Our opengl context handle */
    assert(maincontext != NULL);
    SDL_GL_SetSwapInterval(1);

    GLint pixels[] = { 0xFF00FFFF, 0xFF00FF00, 0xFFFF0000, 0xFF000000 };

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
            0,  
            GL_RGBA,
            2, 2,
            0,  
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    static const GLfloat quadVerts[] = {
        -0.8f,  0.8f, 0.0f, 1.0f,
         0.8f,  0.8f, 0.0f, 1.0f,
        -0.8f, -0.8f, 0.0f, 1.0f,
         0.8f, -0.8f, 0.0f, 1.0f,
    };

    glEnable(GL_TEXTURE_2D);
    
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    while( running )
    {
        while( SDL_PollEvent( &event ) != 0 )
        {
            if( event.type == SDL_QUIT )
            {
                running = false;
            }
        }

        glClearColor ( 1.0, 0.0, 0.0, 1.0 );
        glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBegin(GL_TRIANGLE_STRIP);
        glMultiTexCoord2f(GL_TEXTURE0, quadVerts[0], quadVerts[1]); 
        glVertex4fv(&quadVerts[0]);
        glMultiTexCoord2f(GL_TEXTURE0, quadVerts[4], quadVerts[5]); 
        glVertex4fv(&quadVerts[4]);
        glMultiTexCoord2f(GL_TEXTURE0, quadVerts[8], quadVerts[9]); 
        glVertex4fv(&quadVerts[8]);
        glMultiTexCoord2f(GL_TEXTURE0, quadVerts[12], quadVerts[13]); 
        glVertex4fv(&quadVerts[12]);
        glEnd();

        SDL_GL_SwapWindow(window);
    }


    glDeleteTextures(1, &texture);

    SDL_GL_DeleteContext(maincontext);
    SDL_DestroyWindow( window );
    SDL_Quit();

    return 0;
}
