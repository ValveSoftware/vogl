//**********************************
// OpenGL Framebuffer Multisample
// 27/01/2012 - 27/01/2012
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
	char const * SAMPLE_NAME = "OpenGL Framebuffer Explicit Multisample";	
	char const * TEXTURE_DIFFUSE("kueken3-bgr8.dds");
	glm::ivec2 const FRAMEBUFFER_SIZE(320, 240);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(3);
	int const SAMPLE_MINOR_VERSION(3);

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

	namespace program
	{
		enum type
		{
			THROUGH,
			RESOLVE_BOX,
			RESOLVE_NEAR,
			MAX
		};
	}//namespace program

	namespace renderbuffer
	{
		enum type
		{
			DEPTH,
			COLOR,
			MAX
		};
	}//namespace renderbuffer

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			COLOR,
			MAX
		};
	}//namespace texture

	char const * VERT_SHADER_SOURCE("gl-330/multisample-explicit-texture-nv.vert");
	char const * FRAG_SHADER_SOURCE[program::MAX] = 
	{
		"gl-330/multisample-explicit-texture-nv.frag",
		"gl-330/multisample-explicit-box-nv.frag",
		"gl-330/multisample-explicit-near-nv.frag",
	};

	GLuint VertexArrayName(0);
	GLuint ProgramName[program::MAX];
	GLuint BufferName(0);
	GLuint SamplerName(0);
	GLuint TextureName[texture::MAX] = {0, 0};
	GLuint RenderbufferName[renderbuffer::MAX] = {0, 0};
	GLuint FramebufferName(0);
	GLint UniformMVP[program::MAX];
	GLint UniformDiffuse[program::MAX];
}//namespace

bool initDebugOutput()
{
	bool Validated(true);

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return Validated;
}

bool initSampler()
{
	glGenSamplers(1, &SamplerName);
	glSamplerParameteri(SamplerName, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(SamplerName, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glSamplerParameterfv(SamplerName, GL_TEXTURE_BORDER_COLOR, &glm::vec4(0.0f)[0]);
	glSamplerParameterf(SamplerName, GL_TEXTURE_MIN_LOD, -1000.f);
	glSamplerParameterf(SamplerName, GL_TEXTURE_MAX_LOD, 1000.f);
	glSamplerParameterf(SamplerName, GL_TEXTURE_LOD_BIAS, 0.0f);
	glSamplerParameteri(SamplerName, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glSamplerParameteri(SamplerName, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	return glf::checkError("initSampler");
}

bool initProgram()
{
	bool Validated = true;
	
	// Create program
	for(int i = 0; i < program::MAX; ++i)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE[i]);

		ProgramName[i] = glCreateProgram();
		glAttachShader(ProgramName[i], VertShaderName);
		glAttachShader(ProgramName[i], FragShaderName);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		glLinkProgram(ProgramName[i]);
		Validated = Validated && glf::checkProgram(ProgramName[i]);

		UniformMVP[i] = glGetUniformLocation(ProgramName[i], "MVP");
		UniformDiffuse[i] = glGetUniformLocation(ProgramName[i], "Diffuse");
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
	glGenTextures(texture::MAX, TextureName);

	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Required AMD bug
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // Required AMD bug
	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glTexImage2D(
			GL_TEXTURE_2D, 
			GLint(Level), 
			GL_RGB8, 
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			0,
			GL_BGR, 
			GL_UNSIGNED_BYTE, 
			Texture[Level].data());
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindTexture(GL_TEXTURE_RENDERBUFFER_NV, TextureName[texture::COLOR]);
	glTexRenderbufferNV(GL_TEXTURE_RENDERBUFFER_NV, RenderbufferName[renderbuffer::COLOR]);
	glBindTexture(GL_TEXTURE_RENDERBUFFER_NV, 0);

	return glf::checkError("initTexture");
}

bool initRenderbuffer()
{
	glGenRenderbuffers(renderbuffer::MAX, RenderbufferName);
	glBindRenderbuffer(GL_RENDERBUFFER, RenderbufferName[renderbuffer::COLOR]);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);
	glBindRenderbuffer(GL_RENDERBUFFER, RenderbufferName[renderbuffer::DEPTH]);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	return glf::checkError("initRenderbuffer");
}

bool initFramebuffer()
{
	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, RenderbufferName[renderbuffer::COLOR]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, RenderbufferName[renderbuffer::DEPTH]);
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
	Validated = Validated && glf::checkExtension("GL_NV_explicit_multisample");

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initSampler();
	if(Validated)
		Validated = initRenderbuffer();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();

	return Validated && glf::checkError("begin");
}

bool end()
{
	glDeleteBuffers(1, &BufferName);
	for(int i = 0; i < program::MAX; ++i)
		glDeleteProgram(ProgramName[i]);
	glDeleteSamplers(1, &SamplerName);
	glDeleteTextures(renderbuffer::MAX, RenderbufferName);
	glDeleteTextures(texture::MAX, TextureName);
	glDeleteFramebuffers(1, &FramebufferName);
	glDeleteVertexArrays(1, &VertexArrayName);

	return glf::checkError("end");
}

void renderFBO()
{
	//glm::mat4 Perspective = glm::perspective(45.0f, float(FRAMEBUFFER_SIZE.x) / FRAMEBUFFER_SIZE.y, 0.1f, 100.0f);
	//glm::mat4 ViewFlip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f,-1.0f, 1.0f));
	//glm::mat4 ViewTranslate = glm::translate(ViewFlip, glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
	//glm::mat4 View = glm::rotate(ViewTranslate,-15.f, glm::vec3(0.f, 0.f, 1.f));
	//glm::mat4 Model = glm::mat4(1.0f);
	//glm::mat4 MVP = Perspective * View * Model;

	glm::mat4 Perspective = glm::perspective(45.0f, float(FRAMEBUFFER_SIZE.x) / FRAMEBUFFER_SIZE.y, 0.1f, 100.0f);
	glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0 - 4.0));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Perspective * View * Model;

	glEnable(GL_DEPTH_TEST);

	glUseProgram(ProgramName[program::THROUGH]);
	glUniform1i(UniformDiffuse[program::THROUGH], 0);
	glUniformMatrix4fv(UniformMVP[program::THROUGH], 1, GL_FALSE, &MVP[0][0]);

	glViewport(0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH, 0, &Depth);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glBindSampler(0, SamplerName);
	glBindVertexArray(VertexArrayName);

	glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 5);

	glDisable(GL_DEPTH_TEST);

	glf::checkError("renderFBO");
}

void resolveMultisampling()
{
	////glm::mat4 Projection(glm::ortho(-8.0f, 8.0f, -6.0f, 6.0f));
	//glm::mat4 Perspective = glm::perspective(45.0f, float(Window.Size.x) / Window.Size.y, 0.1f, 100.0f);
	//glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
	//glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
	//glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
	//glm::mat4 Model = glm::mat4(1.0f);
	//glm::mat4 MVP = Perspective * View * Model;

	glm::mat4 Perspective = glm::perspective(45.0f, float(Window.Size.x) / Window.Size.y, 0.1f, 100.0f);
	glm::mat4 ViewFlip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f,-1.0f, 1.0f));
	glm::mat4 ViewTranslate = glm::translate(ViewFlip, glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
	glm::mat4 View = glm::rotate(ViewTranslate,-15.f, glm::vec3(0.f, 0.f, 1.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Perspective * View * Model;

	glViewport(0, 0, Window.Size.x, Window.Size.y);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RENDERBUFFER_NV, TextureName[texture::COLOR]);
	glBindSampler(0, SamplerName);

	glBindVertexArray(VertexArrayName);

	glEnable(GL_SCISSOR_TEST);

	// Box
	{
		glScissor(1, 1, Window.Size.x  / 2 - 2, Window.Size.y - 2);
		glUseProgram(ProgramName[program::RESOLVE_BOX]);
		glUniform1i(UniformDiffuse[program::RESOLVE_BOX], 0);
		glUniformMatrix4fv(UniformMVP[program::RESOLVE_BOX], 1, GL_FALSE, &MVP[0][0]);
		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 5);
	}

	// Near
	{
		glScissor(Window.Size.x / 2 + 1, 1, Window.Size.x / 2 - 2, Window.Size.y - 2);
		glUseProgram(ProgramName[program::RESOLVE_NEAR]);
		glUniform1i(UniformDiffuse[program::RESOLVE_NEAR], 0);
		glUniformMatrix4fv(UniformMVP[program::RESOLVE_NEAR], 1, GL_FALSE, &MVP[0][0]);
		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 5);
	}

	glDisable(GL_SCISSOR_TEST);

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
	renderFBO();
	glDisable(GL_MULTISAMPLE);

	// Pass 2
	// Resolved and render the colorbuffer from the multisampled framebuffer
	resolveMultisampling();

	glf::checkError("display");
	glf::swapBuffers();
}

int main(int argc, char* argv[])
{
	if(glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT, ::SAMPLE_MAJOR_VERSION, 
		::SAMPLE_MINOR_VERSION))
		return 0;
	return 1;
}
