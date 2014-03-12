//**********************************
// OpenGL Layered rendering
// 28/03/2012 - 28/03/2012
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
	char const * SAMPLE_NAME("OpenGL Layered rendering");
	char const * VERT_SHADER_SOURCE1("gl-420/layer-amd.vert");
	char const * FRAG_SHADER_SOURCE1("gl-420/layer-amd.frag");
	char const * VERT_SHADER_SOURCE2("gl-420/viewport-amd.vert");
	char const * FRAG_SHADER_SOURCE2("gl-420/viewport-amd.frag");
	glm::ivec2 const FRAMEBUFFER_SIZE(640, 480);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(3);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2( 3.0f,-1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f, 3.0f), glm::vec2(0.0f, 1.0f))
	};

	enum buffer_type
	{
		BUFFER_VERTEX,
		BUFFER_ELEMENT,
		BUFFER_MAX
	};

	enum pipeline 
	{
		LAYERING,
		VIEWPORT,
		PIPELINE_MAX
	};

	GLuint FramebufferName(0);
	GLuint VertexArrayName[PIPELINE_MAX] = {0, 0};

	GLuint ProgramName[PIPELINE_MAX] = {0, 0};
	GLuint PipelineName[PIPELINE_MAX] = {0, 0};
	GLint UniformMVP[PIPELINE_MAX] = {0, 0};
	GLuint SamplerName(0);

	GLuint BufferName[BUFFER_MAX] = {0, 0};
	GLuint TextureColorbufferName(0);

}//namespace

bool initDebugOutput()
{
	bool Validated(true);

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return Validated;
}

bool initProgram()
{
	bool Validated = true;

	glf::compiler Compiler;

	glGenProgramPipelines(GLsizei(PIPELINE_MAX), PipelineName);

	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE1, 
			"--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE1,
			"--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[LAYERING] = glCreateProgram();
		glProgramParameteri(ProgramName[LAYERING], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[LAYERING], VertShaderName);
		glAttachShader(ProgramName[LAYERING], FragShaderName);
		glLinkProgram(ProgramName[LAYERING]);

		Validated = Validated && glf::checkProgram(ProgramName[LAYERING]);
	}

	if(Validated)
		glUseProgramStages(PipelineName[LAYERING], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[LAYERING]);

	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE2, 
			"--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE2,
			"--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[VIEWPORT] = glCreateProgram();
		glProgramParameteri(ProgramName[VIEWPORT], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[VIEWPORT], VertShaderName);
		glAttachShader(ProgramName[VIEWPORT], FragShaderName);
		glLinkProgram(ProgramName[VIEWPORT]);

		Validated = Validated && glf::checkProgram(ProgramName[VIEWPORT]);
	}

	if(Validated)
		glUseProgramStages(PipelineName[VIEWPORT], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[VIEWPORT]);

	if(Validated)
	{
		for(std::size_t i = 0; i < PIPELINE_MAX; ++i)
			UniformMVP[i] = glGetUniformLocation(ProgramName[i], "MVP");
	}

	return Validated;
}

bool initBuffer()
{
	glGenBuffers(BUFFER_MAX, BufferName);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[BUFFER_VERTEX]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}

bool initTexture()
{
	glGenTextures(1, &TextureColorbufferName);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, TextureColorbufferName);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1000);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

	glTexImage3D(
		GL_TEXTURE_2D_ARRAY, 
		0, 
		GL_RGB, 
		GLsizei(FRAMEBUFFER_SIZE.x), 
		GLsizei(FRAMEBUFFER_SIZE.y), 
		GLsizei(4), //depth
		0,  
		GL_RGB, 
		GL_UNSIGNED_BYTE, 
		NULL);

	return true;
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

	return true;
}

bool initFramebuffer()
{
	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureColorbufferName, 0);

	if(glf::checkFramebuffer(FramebufferName))
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

bool initVertexArray()
{
	glGenVertexArrays(PIPELINE_MAX, VertexArrayName);

	glBindVertexArray(VertexArrayName[VIEWPORT]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[BUFFER_VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);
	glBindVertexArray(0);

	glBindVertexArray(VertexArrayName[LAYERING]);
	glBindVertexArray(0);

	return true;
}

bool begin()
{
	bool Validated = glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();
	if(Validated)
		Validated = initSampler();
	
	return Validated;
}

bool end()
{
	glDeleteVertexArrays(PIPELINE_MAX, VertexArrayName);
	glDeleteBuffers(BUFFER_MAX, BufferName);
	glDeleteTextures(1, &TextureColorbufferName);
	glDeleteFramebuffers(1, &FramebufferName);
	glDeleteProgram(ProgramName[VIEWPORT]);
	glDeleteProgram(ProgramName[LAYERING]);
	glDeleteSamplers(1, &SamplerName);

	return true;
}

void display()
{
	glm::mat4 Projection = glm::ortho(-1.0f, 1.0f, 1.0f,-1.0f, 1.0f, -1.0f);
	glm::mat4 View = glm::mat4(1.0f);
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	glProgramUniformMatrix4fv(ProgramName[LAYERING], UniformMVP[LAYERING], 1, GL_FALSE, &MVP[0][0]);
	glProgramUniformMatrix4fv(ProgramName[VIEWPORT], UniformMVP[VIEWPORT], 1, GL_FALSE, &MVP[0][0]);

	// Pass 1
	{
		glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewportIndexedfv(0, &glm::vec4(0, 0, FRAMEBUFFER_SIZE)[0]);

		glBindProgramPipeline(PipelineName[LAYERING]);
		glBindVertexArray(VertexArrayName[LAYERING]);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 4, 0);
	}

	// Pass 2
	{
		GLint Border = 2;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewportIndexedfv(0, &glm::vec4(Border, Border, Window.Size / 2 - 2 * Border)[0]);
		glViewportIndexedfv(1, &glm::vec4((Window.Size.x >> 1) + Border, Border, Window.Size / 2 - 2 * Border)[0]);
		glViewportIndexedfv(2, &glm::vec4((Window.Size.x >> 1) + Border, (Window.Size.y >> 1) + 1, Window.Size / 2 - 2 * Border)[0]);
		glViewportIndexedfv(3, &glm::vec4(Border, (Window.Size.y >> 1) + Border, Window.Size / 2 - 2 * Border)[0]);

		glBindProgramPipeline(PipelineName[VIEWPORT]);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, TextureColorbufferName);
		glBindSampler(0, SamplerName);

		glBindVertexArray(VertexArrayName[VIEWPORT]);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 4, 0);
	}

	glf::swapBuffers();
}

int main(int argc, char* argv[])
{
	return glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT, ::SAMPLE_MAJOR_VERSION, 
		::SAMPLE_MINOR_VERSION);
}
