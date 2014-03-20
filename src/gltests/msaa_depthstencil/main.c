#include "testcommon.h"

/* Window resolution */
#define WINDOW_WIDTH 512
#define WINDOW_HEIGHT 512

/* Window title */
#define WINDOW_TITLE "vao core profile test"

int main( int argc , char* args[] )
{
    /* The window */
    SDL_Window* window = NULL;

    /* The window surface */
    SDL_Surface* screen = NULL;

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

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

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

    GLint level5Pixels[] = { 0xFF00FFFF, 0xFF00FF00, 0xFFFF0000, 0xFF000000 };
    GLint level6Pixels[] = { 0xFFFFFFFF };

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 5);
    glTexImage2D(GL_TEXTURE_2D,
            5,  
            GL_RGBA,
            2, 2,
            0,  
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            level5Pixels);
    glTexImage2D(GL_TEXTURE_2D,
            6,  
            GL_RGBA,
            1, 1,
            0,  
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            level6Pixels);

    const GLchar* vshadercode = "#version 330 core\n"\
    "layout(location = 0) in vec4 a_position\n;"\
    "layout(location = 1) in vec2 a_texcoord\n;"\
    "out vec2 v_texcoord\n;"\
    "void main(void)\n"\
    "{\n"\
    "    gl_Position = a_position;\n"\
    "    v_texcoord = a_texcoord;\n"\
    "}";

    const GLchar* fshadercode = "#version 330 core\n"\
    "in vec2 v_texcoord;\n"\
    "uniform sampler2D sTexture;\n"\
    "out vec4 o_color;\n"\
    "void main(){\n"\
    "    vec2 tc = v_texcoord.xy*0.5 + 0.5;\n"\
    "    o_color = texture( sTexture, tc );\n"\
    "}";

    GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
    
    glShaderSource(vshader, 1, &vshadercode, NULL);
    glShaderSource(fshader, 1, &fshadercode, NULL);

    CompileShader(vshader, "Vertex");
    CompileShader(fshader, "Fragment");

    GLuint program = glCreateProgram();
    glAttachShader(program, vshader);
    glAttachShader(program, fshader);

    LinkProgram(program, "");

    glUseProgram(program);

    GLuint texLoc = glGetUniformLocation(program, "sTexture");
    GLuint matViewProjLoc = glGetUniformLocation(program, "u_matViewProjection");
    GLuint posLoc = 0;
    GLuint texcoordLoc = 1;

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);

    GLuint quadVB = 0;
    glGenBuffers(1, &quadVB);
    glBindBuffer(GL_ARRAY_BUFFER, quadVB);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, 0);
    glEnableVertexAttribArray(1);

    glBindVertexArray(vao);

    glDepthMask(GL_TRUE);

    glEnable(GL_DEPTH_TEST);

    glDepthFunc(GL_ALWAYS);

    GLuint renderbuffers[2];
    glGenRenderbuffers(2, renderbuffers);

    int num_samples = 1;

    if (argc > 1)
        num_samples = atoi(args[1]);

    printf("Using %u multisample(s)\n", num_samples);

    if (num_samples > 1)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[0]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT);

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[1]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, num_samples, GL_DEPTH_STENCIL, WINDOW_WIDTH, WINDOW_HEIGHT);
    }
    else
    {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT);

        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffers[0]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffers[1]);

    GLenum bufs[1];
    glDrawBuffers(1, bufs);

    GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Failed creating FBO!\n");
        exit(EXIT_FAILURE);
    }

    float f = 0.0f;
    typedef unsigned int uint;
    uint s = 0;

    while( running )
    {
        while( SDL_PollEvent( &event ) != 0 )
        {
            if( event.type == SDL_QUIT )
            {
                running = false;
            }
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);

        glClearColor( 1.0, 0.0, 0.0, 1.0 );
        glClearDepth(f);
        glClearStencil(s & 0xFF);

        f += .125f;
        if (f > 1.0f)
            f = 0;
        s += 1;

        glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

        for (uint i = 0; i < 512; i++)
        {
            glStencilFunc(GL_ALWAYS, i & 0xFF, 0xFF);

            float nx = ((float)(i) / WINDOW_WIDTH) * 2.0f - 1.0f;
            float px = ((float)(i + 1) / WINDOW_WIDTH) * 2.0f - 1.0f;

            const GLfloat quadVerts[] = {
                 nx,  0.8f, 0.33f, 1.0f,
                 px,  0.8f, 0.33f, 1.0f,
                 nx, -0.8f, 0.33f, 1.0f,
                 px, -0.8f, 0.33f, 1.0f,
            };

            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        glBlitFramebuffer(0,
            0,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            0,
            0,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            GL_COLOR_BUFFER_BIT,
            GL_LINEAR);
        
        SDL_GL_SwapWindow(window);
    }

    glDeleteRenderbuffers(2, renderbuffers);

    glDeleteProgram(program);
    glDeleteShader(vshader);
    glDeleteShader(fshader);
    glDeleteTextures(1, &texture);

    glDeleteBuffers(1, &quadVB);
    glDeleteVertexArrays(1, &vao);

    SDL_GL_DeleteContext(maincontext);
    SDL_DestroyWindow( window );
    SDL_Quit();

    return 0;
}
