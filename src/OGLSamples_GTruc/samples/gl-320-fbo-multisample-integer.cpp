//**********************************
// OpenGL Framebuffer Multisample Integer
// 28/01/2013 - 23/02/2013
//**********************************
// Christophe Riccio
// ogl-samples@g-truc.net
//**********************************
// G-Truc Creation
// www.g-truc.net
//**********************************

#include <glf/glf.hpp>

namespace
{
	char const * SAMPLE_NAME("OpenGL Framebuffer Multisample Integer");	
	char const * VERT_SHADER_SOURCE1("gl-320/fbo-multisample-integer.vert");
	char const * FRAG_SHADER_SOURCE1("gl-320/fbo-multisample-integer.frag");
	char const * VERT_SHADER_SOURCE2("gl-320/texture-integer.vert");
	char const * FRAG_SHADER_SOURCE2("gl-320/texture-integer.frag");
	char const * TEXTURE_DIFFUSE("kueken3-bgr8.dds");
	glm::ivec2 const FRAMEBUFFER_SIZE(160, 120);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(3);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	// With DDS textures, v texture coordinate are reversed, from top to bottom
	GLsizei const VertexCount(6);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-4.0f,-3.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 4.0f,-3.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 4.0f, 3.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2( 4.0f, 3.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-4.0f, 3.0f), glm::vec2(0.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-4.0f,-3.0f), glm::vec2(0.0f, 1.0f))
	};

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			COLORBUFFER,
			MULTISAMPLE,
			MAX
		};
	}//namespace texture

	namespace program
	{
		enum type
		{
			RENDER,
			SPLASH,
			MAX
		};
	}//namespace buffer

	namespace framebuffer
	{
		enum type
		{
			RENDER,
			RESOLVE,
			MAX
		};
	}//namespace framebuffer

	GLuint VertexArrayName(0);
	GLuint BufferName(0);
	std::vector<GLuint> TextureName(texture::MAX);
	std::vector<GLuint> FramebufferName(framebuffer::MAX);
	std::vector<GLuint> ProgramName(program::MAX);
	std::vector<GLuint> UniformMVP(program::MAX);
	std::vector<GLuint> UniformDiffuse(program::MAX);
}//namespace

bool initDebugOutput()
{
#	ifdef GL_ARB_debug_output
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		glDebugMessageCallbackARB(&glf::debugOutput, NULL);
#	endif

	return true;
}

bool initProgram()
{
	bool Validated = true;
	
	glf::compiler Compiler;

	// Create program
	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE1, "--version 150 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE1, "--version 150 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[program::RENDER] = glCreateProgram();
		glAttachShader(ProgramName[program::RENDER], VertShaderName);
		glAttachShader(ProgramName[program::RENDER], FragShaderName);
		glBindAttribLocation(ProgramName[program::RENDER], glf::semantic::attr::POSITION, "Position");
		glBindAttribLocation(ProgramName[program::RENDER], glf::semantic::attr::TEXCOORD, "Texcoord");
		glBindFragDataLocation(ProgramName[program::RENDER], glf::semantic::frag::COLOR, "Color");
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		glLinkProgram(ProgramName[program::RENDER]);
		Validated = Validated && glf::checkProgram(ProgramName[program::RENDER]);
	}

	if(Validated)
	{
		UniformMVP[program::RENDER] = glGetUniformLocation(ProgramName[program::RENDER], "MVP");
		UniformDiffuse[program::RENDER] = glGetUniformLocation(ProgramName[program::RENDER], "Diffuse");
	}

	// Create program
	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE2, "--version 150 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE2, "--version 150 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[program::SPLASH] = glCreateProgram();
		glAttachShader(ProgramName[program::SPLASH], VertShaderName);
		glAttachShader(ProgramName[program::SPLASH], FragShaderName);
		glBindAttribLocation(ProgramName[program::SPLASH], glf::semantic::attr::POSITION, "Position");
		glBindAttribLocation(ProgramName[program::SPLASH], glf::semantic::attr::TEXCOORD, "Texcoord");
		glBindFragDataLocation(ProgramName[program::SPLASH], glf::semantic::frag::COLOR, "Color");
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		glLinkProgram(ProgramName[program::SPLASH]);
		Validated = Validated && glf::checkProgram(ProgramName[program::SPLASH]);
	}

	if(Validated)
	{
		UniformMVP[program::SPLASH] = glGetUniformLocation(ProgramName[program::SPLASH], "MVP");
		UniformDiffuse[program::SPLASH] = glGetUniformLocation(ProgramName[program::SPLASH], "Diffuse");
	}

	return Validated && glf::checkError("initProgram");
}

bool initBuffer()
{
	glGenBuffers(1, &BufferName);
	glBindBuffer(GL_ARRAY_BUFFER, BufferName);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return glf::checkError("initBuffer");;
}

bool initTexture()
{
	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));

	glGenTextures(texture::MAX, &TextureName[0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glTexImage2D(
			GL_TEXTURE_2D, 
			GLint(Level), 
			GL_RGBA8UI, 
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			0,
			GL_BGR_INTEGER, GL_UNSIGNED_BYTE,
			Texture[Level].data());
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	GLint MaxSampleMaskWords(0);
	GLint MaxColorTextureSamples(0);
	GLint MaxDepthTextureSamples(0);
	GLint MaxIntegerSamples(0);

	glGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &MaxSampleMaskWords); 
	glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &MaxColorTextureSamples); 
	glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &MaxDepthTextureSamples); 
	glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &MaxIntegerSamples); 

	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, TextureName[texture::MULTISAMPLE]);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MaxIntegerSamples, GL_RGBA8UI, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, GL_TRUE);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	return glf::checkError("initTexture");
}

bool initFramebuffer()
{
	glGenFramebuffers(framebuffer::MAX, &FramebufferName[0]);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::RENDER]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::MULTISAMPLE], 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::RESOLVE]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::COLORBUFFER], 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return glf::checkError("initFramebuffer");
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);
	glBindVertexArray(0);

	return glf::checkError("initVertexArray");
}

bool begin()
{
	bool Validated = true;
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initProgram();
	glf::checkError("initProgram Apple workaround");
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();

	return Validated && glf::checkError("begin");
}

bool end()
{
	glDeleteBuffers(1, &BufferName);
	glDeleteProgram(ProgramName[program::RENDER]);
	glDeleteProgram(ProgramName[program::SPLASH]);
	glDeleteTextures(texture::MAX, &TextureName[0]);
	glDeleteFramebuffers(framebuffer::MAX, &FramebufferName[0]);
	glDeleteVertexArrays(1, &VertexArrayName);

	return glf::checkError("end");
}

void renderFBO(GLuint Framebuffer)
{
	glm::mat4 Perspective = glm::perspective(45.0f, float(FRAMEBUFFER_SIZE.x) / FRAMEBUFFER_SIZE.y, 0.1f, 100.0f);
	glm::mat4 ViewFlip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f,-1.0f, 1.0f));
	glm::mat4 ViewTranslate = glm::translate(ViewFlip, glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
	glm::mat4 View = glm::rotate(ViewTranslate,-15.f, glm::vec3(0.f, 0.f, 1.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Perspective * View * Model;

	glUseProgram(ProgramName[program::RENDER]);
	glUniform1i(UniformDiffuse[program::RENDER], 0);
	glUniformMatrix4fv(UniformMVP[program::RENDER], 1, GL_FALSE, &MVP[0][0]);

	glViewport(0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);
	glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
	glClearBufferuiv(GL_COLOR, 0, &glm::uvec4(0, 128, 255, 255)[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glBindVertexArray(VertexArrayName);

	glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);

	glf::checkError("renderFBO");
}

void renderFB(GLuint Texture2DName)
{
	glm::mat4 Perspective = glm::perspective(45.0f, float(Window.Size.x) / Window.Size.y, 0.1f, 100.0f);
	glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Perspective * View * Model;

	glUseProgram(ProgramName[program::SPLASH]);
	glUniform1i(UniformDiffuse[program::SPLASH], 0);
	glUniformMatrix4fv(UniformMVP[program::SPLASH], 1, GL_FALSE, &MVP[0][0]);

	glViewport(0, 0, Window.Size.x, Window.Size.y);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Texture2DName);
	glBindVertexArray(VertexArrayName);

	glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);
		
	glf::checkError("renderFB");
}

void display()
{
	// Clear the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	// Pass 1
	// Render the scene in a multisampled framebuffer
	glEnable(GL_MULTISAMPLE);
	renderFBO(FramebufferName[framebuffer::RENDER]);
	glDisable(GL_MULTISAMPLE);

	// Resolved multisampling
	glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferName[framebuffer::RENDER]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferName[framebuffer::RESOLVE]);
	glBlitFramebuffer(
		0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 
		0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Pass 2
	// Render the colorbuffer from the multisampled framebuffer
	renderFB(TextureName[texture::COLORBUFFER]);

	glf::checkError("display");
	glf::swapBuffers();
}

int main(int argc, char* argv[])
{
	if(glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT,
		::SAMPLE_MAJOR_VERSION, ::SAMPLE_MINOR_VERSION))
		return 0;
	return 1;
}
