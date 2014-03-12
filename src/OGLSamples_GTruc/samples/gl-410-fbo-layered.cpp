//**********************************
// OpenGL Layered rendering
// 19/08/2010 - 04/03/2012
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
	char const * VERT_SHADER_SOURCE1("gl-410/layer.vert");
	char const * GEOM_SHADER_SOURCE1("gl-410/layer.geom");
	char const * FRAG_SHADER_SOURCE1("gl-410/layer.frag");
	char const * VERT_SHADER_SOURCE2("gl-410/viewport.vert");
	char const * GEOM_SHADER_SOURCE2("gl-410/viewport.geom");
	char const * FRAG_SHADER_SOURCE2("gl-410/viewport.frag");
	glm::ivec2 const FRAMEBUFFER_SIZE(640, 480);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(1);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(4);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f,-1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 1.0f))
	};

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0
	};

	enum buffer_type
	{
		BUFFER_VERTEX,
		BUFFER_ELEMENT,
		BUFFER_MAX
	};

	enum program 
	{
		LAYERING,
		VIEWPORT,
		PROGRAM_MAX
	};

	GLuint FramebufferName(0);
	GLuint VertexArrayName[PROGRAM_MAX] = {0, 0};

	GLuint ProgramName[PROGRAM_MAX] = {0, 0};
	GLint UniformMVP[PROGRAM_MAX] = {0, 0};
	GLint UniformDiffuse(0);
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

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE1);
		GLuint GeomShaderName = glf::createShader(GL_GEOMETRY_SHADER, glf::DATA_DIRECTORY + GEOM_SHADER_SOURCE1);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE1);

		ProgramName[LAYERING] = glCreateProgram();
		glAttachShader(ProgramName[LAYERING], VertShaderName);
		glAttachShader(ProgramName[LAYERING], GeomShaderName);
		glAttachShader(ProgramName[LAYERING], FragShaderName);
		glDeleteShader(VertShaderName);
		glDeleteShader(GeomShaderName);
		glDeleteShader(FragShaderName);
		glLinkProgram(ProgramName[LAYERING]);
		Validated = glf::checkProgram(ProgramName[LAYERING]);
	}

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE2);
		GLuint GeomShaderName = glf::createShader(GL_GEOMETRY_SHADER, glf::DATA_DIRECTORY + GEOM_SHADER_SOURCE2);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE2);

		ProgramName[VIEWPORT] = glCreateProgram();
		glAttachShader(ProgramName[VIEWPORT], VertShaderName);
		glAttachShader(ProgramName[VIEWPORT], GeomShaderName);
		glAttachShader(ProgramName[VIEWPORT], FragShaderName);
		glDeleteShader(VertShaderName);
		glDeleteShader(GeomShaderName);
		glDeleteShader(FragShaderName);
		glLinkProgram(ProgramName[VIEWPORT]);
		Validated = glf::checkProgram(ProgramName[VIEWPORT]);
	}

	if(Validated)
	{
		for(std::size_t i = 0; i < PROGRAM_MAX; ++i)
			UniformMVP[i] = glGetUniformLocation(ProgramName[i], "MVP");

		UniformDiffuse = glGetUniformLocation(ProgramName[VIEWPORT], "Diffuse");
	}

	return Validated && glf::checkError("initProgram");
}

bool initBuffer()
{
	glGenBuffers(BUFFER_MAX, BufferName);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[BUFFER_ELEMENT]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[BUFFER_VERTEX]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return glf::checkError("initBuffer");
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

	return glf::checkError("initTexture");
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

bool initFramebuffer()
{
	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureColorbufferName, 0);

	if(glf::checkFramebuffer(FramebufferName))
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return glf::checkError("initFramebuffer");
}

bool initVertexArray()
{
	glGenVertexArrays(PROGRAM_MAX, VertexArrayName);

	glBindVertexArray(VertexArrayName[VIEWPORT]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[BUFFER_VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[BUFFER_ELEMENT]);
	glBindVertexArray(0);

	glBindVertexArray(VertexArrayName[LAYERING]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[BUFFER_VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[BUFFER_ELEMENT]);
	glBindVertexArray(0);

	return glf::checkError("initVertexArray");
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
	
	return Validated && glf::checkError("begin");
}

bool end()
{
	glDeleteVertexArrays(PROGRAM_MAX, VertexArrayName);
	glDeleteBuffers(BUFFER_MAX, BufferName);
	glDeleteTextures(1, &TextureColorbufferName);
	glDeleteFramebuffers(1, &FramebufferName);
	glDeleteProgram(ProgramName[VIEWPORT]);
	glDeleteProgram(ProgramName[LAYERING]);
	glDeleteSamplers(1, &SamplerName);

	return glf::checkError("end");
}

void display()
{
	glm::mat4 Projection = glm::ortho(-1.0f, 1.0f, 1.0f,-1.0f, 1.0f, -1.0f);
	glm::mat4 View = glm::mat4(1.0f);
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	glProgramUniformMatrix4fv(ProgramName[LAYERING], UniformMVP[LAYERING], 1, GL_FALSE, &MVP[0][0]);
	glProgramUniformMatrix4fv(ProgramName[VIEWPORT], UniformMVP[VIEWPORT], 1, GL_FALSE, &MVP[0][0]);
	glProgramUniform1i(ProgramName[VIEWPORT], UniformDiffuse, 0);

	// Pass 1
	{
		glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewportIndexedfv(0, &glm::vec4(0, 0, FRAMEBUFFER_SIZE)[0]);

		glUseProgram(ProgramName[LAYERING]);

		glBindVertexArray(VertexArrayName[LAYERING]);
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, NULL, 1, 0);
	}

	// Pass 2
	{
		GLint Border = 2;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewportIndexedfv(0, &glm::vec4(Border, Border, Window.Size / 2 - 2 * Border)[0]);
		glViewportIndexedfv(1, &glm::vec4((Window.Size.x >> 1) + Border, Border, Window.Size / 2 - 2 * Border)[0]);
		glViewportIndexedfv(2, &glm::vec4((Window.Size.x >> 1) + Border, (Window.Size.y >> 1) + 1, Window.Size / 2 - 2 * Border)[0]);
		glViewportIndexedfv(3, &glm::vec4(Border, (Window.Size.y >> 1) + Border, Window.Size / 2 - 2 * Border)[0]);

		glUseProgram(ProgramName[VIEWPORT]);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, TextureColorbufferName);
		glBindSampler(0, SamplerName);

		glBindVertexArray(VertexArrayName[VIEWPORT]);
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, NULL, 1, 0);
	}

	glf::checkError("display");
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
