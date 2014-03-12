//**********************************
// OpenGL Samples Pack 
// ogl-samples.g-truc.net
//**********************************
// OpenGL Primitive Line AA
// 29/08/2012 - 29/08/2012
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
	char const * SAMPLE_NAME("OpenGL Primitive Line AA");
	char const * VERT_SHADER_SOURCE_AA("gl-420/primitive-line-aa.vert");
	char const * FRAG_SHADER_SOURCE_AA("gl-420/primitive-line-aa.frag");
	char const * VERT_SHADER_SOURCE_SPLASH("gl-420/primitive-line-splash.vert");
	char const * FRAG_SHADER_SOURCE_SPLASH("gl-420/primitive-line-splash.frag");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);
	glm::uvec2 const FRAMEBUFFER_SIZE(80, 60);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	std::vector<glm::vec2> VertexData;

	namespace buffer
	{
		enum type
		{
			VERTEX,
			TRANSFORM,
			MAX
		};
	}//namespace buffer
	
	namespace texture
	{
		enum type
		{
			MULTISAMPLE,
			COLOR,
			MAX
		};
	}//namespace texture

	namespace framebuffer
	{
		enum type
		{
			MULTISAMPLE,
			COLOR,
			MAX
		};
	}//namespace framebuffer

	namespace pipeline
	{
		enum type
		{
			MULTISAMPLE,
			SPLASH,
			MAX
		};
	}//namespace pipeline

	GLuint VertexArrayName[pipeline::MAX] = {0, 0};
	GLuint PipelineName[pipeline::MAX] = {0, 0};
	GLuint ProgramName[pipeline::MAX] = {0, 0};
	GLuint FramebufferName[framebuffer::MAX] = {0, 0};
	GLuint BufferName[buffer::MAX] = {0, 0};
	GLuint TextureName[texture::MAX] = {0, 0};
}//namespace

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(pipeline::MAX, PipelineName);

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_AA);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_AA);

		ProgramName[pipeline::MULTISAMPLE] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::MULTISAMPLE], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::MULTISAMPLE], VertShaderName);
		glAttachShader(ProgramName[pipeline::MULTISAMPLE], FragShaderName);
		glLinkProgram(ProgramName[pipeline::MULTISAMPLE]);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);
		Validated = Validated && glf::checkProgram(ProgramName[pipeline::MULTISAMPLE]);
	}

	if(Validated)
	{
		glUseProgramStages(
			PipelineName[pipeline::MULTISAMPLE], 
			GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
			ProgramName[pipeline::MULTISAMPLE]);
	}

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_SPLASH);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_SPLASH);

		ProgramName[pipeline::SPLASH] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::SPLASH], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::SPLASH], VertShaderName);
		glAttachShader(ProgramName[pipeline::SPLASH], FragShaderName);
		glLinkProgram(ProgramName[pipeline::SPLASH]);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);
		Validated = Validated && glf::checkProgram(ProgramName[pipeline::SPLASH]);
	}

	if(Validated)
	{
		glUseProgramStages(
			PipelineName[pipeline::SPLASH], 
			GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
			ProgramName[pipeline::SPLASH]);
	}

	return Validated;
}

bool initBuffer()
{
	std::size_t const Count(360);
	float const Step(360.f / float(Count));

	VertexData.resize(Count);
	for(std::size_t i = 0; i < Count; ++i)
		VertexData[i] = glm::vec2(glm::sin(glm::radians(Step * float(i))), glm::cos(glm::radians(Step * float(i))));

	glGenBuffers(buffer::MAX, BufferName);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
	glBufferData(GL_ARRAY_BUFFER, VertexData.size() * sizeof(glm::vec2), &VertexData[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLint UniformBufferOffset(0);

	glGetIntegerv(
		GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		&UniformBufferOffset);

	GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return true;
}

bool initVertexArray()
{
	glGenVertexArrays(pipeline::MAX, VertexArrayName);

	glBindVertexArray(VertexArrayName[pipeline::MULTISAMPLE]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), GLF_BUFFER_OFFSET(0));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
	glBindVertexArray(0);

	glBindVertexArray(VertexArrayName[pipeline::SPLASH]);
	glBindVertexArray(0);

	return true;
}

bool initFramebuffer()
{
	glGenFramebuffers(framebuffer::MAX, FramebufferName);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::MULTISAMPLE]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::MULTISAMPLE], 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::COLOR]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::COLOR], 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return true;
}

bool initTexture()
{
	glGenTextures(texture::MAX, TextureName);

	{
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, TextureName[texture::MULTISAMPLE]);
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 8, GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, GL_TRUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
	}

	{
		glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLOR]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	
	return true;
}

bool initState()
{
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	return true;
}

bool initDebugOutput()
{
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return true;
}

bool begin()
{
	bool Validated(true);
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initState();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();
	if(Validated)
		Validated = initVertexArray();

	return Validated;
}

bool end()
{
	glDeleteBuffers(texture::MAX, TextureName);
	glDeleteProgramPipelines(pipeline::MAX, PipelineName);
	glDeleteProgram(ProgramName[pipeline::MULTISAMPLE]);
	glDeleteProgram(ProgramName[pipeline::SPLASH]);
	glDeleteBuffers(buffer::MAX, BufferName);
	glDeleteVertexArrays(pipeline::MAX, VertexArrayName);

	return true;
}

void display()
{
	// Update of the uniform buffer
	{
		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(
			GL_UNIFORM_BUFFER, 0,	sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		glm::mat4 Projection = glm::perspectiveFov(45.f, 640.f, 480.f, 0.1f, 100.0f);
		//glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 Model = glm::mat4(1.0f);
		
		*Pointer = Projection * View * Model;

		// Make sure the uniform buffer is uploaded
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}

	//////////////////////////////
	// Render multisampled texture

	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::MULTISAMPLE]);
	//glEnable(GL_MULTISAMPLE);
	glViewportIndexedf(0, 0, 0, GLfloat(FRAMEBUFFER_SIZE.x), GLfloat(FRAMEBUFFER_SIZE.y));
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	glBindProgramPipeline(PipelineName[pipeline::MULTISAMPLE]);
	glBindVertexArray(VertexArrayName[pipeline::MULTISAMPLE]);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);

	glDrawArraysInstancedBaseInstance(GL_LINE_LOOP, 0, GLsizei(VertexData.size()), 3, 0);

	//////////////////////////
	// Resolving multisampling
	glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferName[framebuffer::MULTISAMPLE]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferName[framebuffer::COLOR]);
	glBlitFramebuffer(
		0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 
		0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//////////////////////////////////////
	// Render resolved multisample texture

	glViewportIndexedf(0, 0, 0, GLfloat(Window.Size.x), GLfloat(Window.Size.y));
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glDisable(GL_MULTISAMPLE);
	glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, TextureName[texture::MULTISAMPLE]);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLOR]);
	glBindVertexArray(VertexArrayName[pipeline::SPLASH]);
	glBindProgramPipeline(PipelineName[pipeline::SPLASH]);

	glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, 1, 0);
	
	glf::swapBuffers();
}

int main(int argc, char* argv[])
{
	return glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT, 
		::SAMPLE_MAJOR_VERSION, ::SAMPLE_MINOR_VERSION);
}
