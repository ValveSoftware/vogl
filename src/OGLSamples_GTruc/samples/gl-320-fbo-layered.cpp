//**********************************
// OpenGL Layered rendering
// 19/08/2010 - 22/02/2013
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
	char const * VERT_SHADER_SOURCE1("gl-320/fbo-layered.vert");
	char const * GEOM_SHADER_SOURCE1("gl-320/fbo-layered.geom");
	char const * FRAG_SHADER_SOURCE1("gl-320/fbo-layered.frag");
	char const * VERT_SHADER_SOURCE2("gl-320/fbo-layered-rtt-array.vert");
	char const * FRAG_SHADER_SOURCE2("gl-320/fbo-layered-rtt-array.frag");
	glm::ivec2 const FRAMEBUFFER_SIZE(320, 240);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(3);
	int const SAMPLE_MINOR_VERSION(2);

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

	namespace buffer
	{
		enum type
		{
			VERTEX,
			ELEMENT,
			MAX
		};
	}//namespace buffer

	namespace program
	{
		enum type
		{
			LAYERING,
			SPLASH,
			MAX
		};
	}//namespace program

	GLuint FramebufferName(0);
	std::vector<GLuint> VertexArrayName(program::MAX);
	std::vector<GLuint> ProgramName(program::MAX);
	std::vector<GLuint> UniformMVP(program::MAX);
	GLint UniformDiffuse(0);
	GLint UniformLayer(0);

	std::vector<GLuint> BufferName(buffer::MAX);
	GLuint TextureColorbufferName(0);
	glm::ivec4 Viewport[4];
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

	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE1, "--version 150 --profile core");
		GLuint GeomShaderName = Compiler.create(GL_GEOMETRY_SHADER, glf::DATA_DIRECTORY + GEOM_SHADER_SOURCE1, "--version 150 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE1, "--version 150 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[program::LAYERING] = glCreateProgram();
		glAttachShader(ProgramName[program::LAYERING], VertShaderName);
		glAttachShader(ProgramName[program::LAYERING], GeomShaderName);
		glAttachShader(ProgramName[program::LAYERING], FragShaderName);
		glBindAttribLocation(ProgramName[program::LAYERING], glf::semantic::attr::POSITION, "Position");
		glBindFragDataLocation(ProgramName[program::LAYERING], glf::semantic::frag::COLOR, "FragColor");
		glDeleteShader(VertShaderName);
		glDeleteShader(GeomShaderName);
		glDeleteShader(FragShaderName);
		glLinkProgram(ProgramName[program::LAYERING]);
		Validated = Validated && glf::checkProgram(ProgramName[program::LAYERING]);
	}

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
		for(std::size_t i = 0; i < program::MAX; ++i)
			UniformMVP[i] = glGetUniformLocation(ProgramName[i], "MVP");

		UniformDiffuse = glGetUniformLocation(ProgramName[program::SPLASH], "Diffuse");
		UniformLayer = glGetUniformLocation(ProgramName[program::SPLASH], "Layer");
	}

	return Validated && glf::checkError("initProgram");
}

bool initBuffer()
{
	glGenBuffers(buffer::MAX, &BufferName[0]);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
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
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage3D(
		GL_TEXTURE_2D_ARRAY,
		0,
		GL_RGB8,
		GLsizei(FRAMEBUFFER_SIZE.x),
		GLsizei(FRAMEBUFFER_SIZE.y),
		GLsizei(4), //depth
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		NULL);

	return glf::checkError("initTexture");
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
	glGenVertexArrays(program::MAX, &VertexArrayName[0]);

	glBindVertexArray(VertexArrayName[program::SPLASH]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBindVertexArray(0);

	glBindVertexArray(VertexArrayName[program::LAYERING]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBindVertexArray(0);

	return glf::checkError("initVertexArray");
}

bool begin()
{
	int Border = 2;
	Viewport[0] = glm::ivec4(Border, Border, FRAMEBUFFER_SIZE - 2 * Border);
	Viewport[1] = glm::ivec4((Window.Size.x >> 1) + Border, 1, FRAMEBUFFER_SIZE - 2 * Border);
	Viewport[2] = glm::ivec4((Window.Size.x >> 1) + Border, (Window.Size.y >> 1) + Border, FRAMEBUFFER_SIZE - 2 * Border);
	Viewport[3] = glm::ivec4(Border, (Window.Size.y >> 1) + Border, FRAMEBUFFER_SIZE - 2 * Border);

	bool Validated(true);
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
	for(std::size_t i = 0; i < program::MAX; ++i)
		glDeleteProgram(ProgramName[i]);
	glDeleteVertexArrays(program::MAX, &VertexArrayName[0]);
	glDeleteBuffers(buffer::MAX, &BufferName[0]);
	glDeleteTextures(1, &TextureColorbufferName);
	glDeleteFramebuffers(1, &FramebufferName);

	return glf::checkError("end");
}

void display()
{
	glm::mat4 Projection = glm::ortho(-1.0f, 1.0f, 1.0f,-1.0f, 1.0f, -1.0f);
	glm::mat4 View = glm::mat4(1.0f);
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	// Pass 1
	{
		glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
		glViewport(0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);

		glUseProgram(ProgramName[program::LAYERING]);
		glUniformMatrix4fv(UniformMVP[program::LAYERING], 1, GL_FALSE, &MVP[0][0]);

		glBindVertexArray(VertexArrayName[program::LAYERING]);
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, NULL, 1, 0);
	}

	// Pass 2
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glUseProgram(ProgramName[program::SPLASH]);
		glUniformMatrix4fv(UniformMVP[program::SPLASH], 1, GL_FALSE, &MVP[0][0]);
		glUniform1i(UniformDiffuse, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, TextureColorbufferName);

		glBindVertexArray(VertexArrayName[program::SPLASH]);

		for(int i = 0; i < 4; ++i)
		{
			glUniform1i(UniformLayer, i);
			glViewport(Viewport[i].x, Viewport[i].y, Viewport[i].z, Viewport[i].w);
			glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, NULL, 1, 0);
		}
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
