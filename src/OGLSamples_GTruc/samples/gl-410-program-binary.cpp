//**********************************
// OpenGL Program binary
// 02/08/2010 - 02/08/2010
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
	char const * SAMPLE_NAME("OpenGL Program binary");
	char const * VERT_SHADER_SOURCE("gl-410/binary.vert");
	char const * GEOM_SHADER_SOURCE("gl-410/binary.geom");
	char const * FRAG_SHADER_SOURCE("gl-410/binary.frag");
	char const * VERT_PROGRAM_BINARY("gl-410/binary.vert.bin");
	char const * GEOM_PROGRAM_BINARY("gl-410/binary.geom.bin");
	char const * FRAG_PROGRAM_BINARY("gl-410/binary.frag.bin");
	char const * TEXTURE_DIFFUSE_DXT5( "kueken1-dxt5.dds");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(1);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(4);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f,-1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 0.0f))
	};

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLuint);
	GLuint const ElementData[ElementCount] =
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
			VERT,
			GEOM,
			FRAG,
			MAX
		};
	}//namespace program

	GLuint PipelineName(0);
	GLuint ProgramName[program::MAX] = {0, 0, 0};
	GLuint BufferName[buffer::MAX] = {0, 0};
	GLuint VertexArrayName(0);
	GLint UniformMVP(0);
	GLint UniformDiffuse(0);
	GLuint Texture2DName(0);

}//namespace

bool initDebugOutput()
{
	bool Validated(true);

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return Validated;
}

bool saveProgram(GLuint ProgramName, std::string const & String)
{
	GLint Size(0);
	GLenum Format(0);

	glGetProgramiv(ProgramName, GL_PROGRAM_BINARY_LENGTH, &Size);
	std::vector<glm::byte> Data(Size);
	glGetProgramBinary(ProgramName, Size, NULL, &Format, &Data[0]);
	glf::saveBinary(String, Format, Data, Size);

	return glf::checkError("saveProgram");
}

bool initProgram()
{
	bool Validated = true;
	GLint Success = 0;

	glGenProgramPipelines(1, &PipelineName);

	// Vertex program
	ProgramName[program::VERT] = glCreateProgram();
	glProgramParameteri(ProgramName[program::VERT], GL_PROGRAM_SEPARABLE, GL_TRUE);
	glProgramParameteri(ProgramName[program::VERT], GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	{
		GLenum Format = 0;
		GLint Size = 0;
		std::vector<glm::byte> Data;
		if(glf::loadBinary(glf::DATA_DIRECTORY + VERT_PROGRAM_BINARY, Format, Data, Size))
		{
			glProgramBinary(ProgramName[program::VERT], Format, &Data[0], Size);
			glGetProgramiv(ProgramName[program::VERT], GL_LINK_STATUS, &Success);
		}
	}

	// Create program
	if(Validated && !Success)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE);

		glAttachShader(ProgramName[program::VERT], VertShaderName);
		glDeleteShader(VertShaderName);
		glLinkProgram(ProgramName[program::VERT]);
		Validated = Validated && glf::checkProgram(ProgramName[program::VERT]);
	}

	// Geometry program
	ProgramName[program::GEOM] = glCreateProgram();
	glProgramParameteri(ProgramName[program::GEOM], GL_PROGRAM_SEPARABLE, GL_TRUE);
	glProgramParameteri(ProgramName[program::GEOM], GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	{
		GLenum Format = 0;
		GLint Size = 0;
		std::vector<glm::byte> Data;
		if(glf::loadBinary(glf::DATA_DIRECTORY + GEOM_PROGRAM_BINARY, Format, Data, Size))
		{
			glProgramBinary(ProgramName[program::GEOM], Format, &Data[0], Size);
			glGetProgramiv(ProgramName[program::GEOM], GL_LINK_STATUS, &Success);
		}
	}

	// Create program
	if(Validated && !Success)
	{
		GLuint GeomShaderName = glf::createShader(GL_GEOMETRY_SHADER, glf::DATA_DIRECTORY + GEOM_SHADER_SOURCE);

		glAttachShader(ProgramName[program::GEOM], GeomShaderName);
		glDeleteShader(GeomShaderName);
		glLinkProgram(ProgramName[program::GEOM]);
		Validated = Validated && glf::checkProgram(ProgramName[program::GEOM]);
	}

	// Fragment program
	ProgramName[program::FRAG] = glCreateProgram();
	glProgramParameteri(ProgramName[program::FRAG], GL_PROGRAM_SEPARABLE, GL_TRUE);
	glProgramParameteri(ProgramName[program::FRAG], GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

	{
		GLenum Format = 0;
		GLint Size = 0;
		std::vector<glm::byte> Data;
		if(glf::loadBinary(glf::DATA_DIRECTORY + FRAG_PROGRAM_BINARY, Format, Data, Size))
		{
			glProgramBinary(ProgramName[program::FRAG], Format, &Data[0], Size);
			glGetProgramiv(ProgramName[program::FRAG], GL_LINK_STATUS, &Success);
		}
	}

	// Create program
	if(Validated && !Success)
	{
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE);

		glAttachShader(ProgramName[program::FRAG], FragShaderName);
		glDeleteShader(FragShaderName);
		glLinkProgram(ProgramName[program::FRAG]);
		Validated = Validated && glf::checkProgram(ProgramName[program::FRAG]);
	}

	// Pipeline program
	if(Validated)
	{
		glUseProgramStages(PipelineName, GL_VERTEX_SHADER_BIT, ProgramName[program::VERT]);
		glUseProgramStages(PipelineName, GL_GEOMETRY_SHADER_BIT, ProgramName[program::GEOM]);
		glUseProgramStages(PipelineName, GL_FRAGMENT_SHADER_BIT, ProgramName[program::FRAG]);
		Validated = Validated && glf::checkError("initProgram - stage");
	}

	// Get variables locations
	if(Validated)
	{
		UniformMVP = glGetUniformLocation(ProgramName[program::VERT], "MVP");
		UniformDiffuse = glGetUniformLocation(ProgramName[program::FRAG], "Diffuse");
	}

	return Validated && glf::checkError("initProgram");
}

bool initTexture()
{
	glGenTextures(1, &Texture2DName);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Texture2DName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE_DXT5));
	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glCompressedTexImage2D(
			GL_TEXTURE_2D,
			GLint(Level),
			GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			0, 
			GLsizei(Texture[Level].size()), 
			Texture[Level].data());
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	return glf::checkError("initTexture");
}

bool initBuffer()
{
	// Generate a buffer object
	glGenBuffers(buffer::MAX, BufferName);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return glf::checkError("initArrayBuffer");
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);

    glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
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
	bool Validated = glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	GLint NumProgramBinaryFormats(0);
	glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &NumProgramBinaryFormats);

	std::vector<GLint> ProgramBinaryFormats(static_cast<std::size_t>(NumProgramBinaryFormats));
	glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, &ProgramBinaryFormats[0]);

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

	return Validated && glf::checkError("begin");
}

bool end()
{
	saveProgram(ProgramName[program::VERT], glf::DATA_DIRECTORY + VERT_PROGRAM_BINARY);
	saveProgram(ProgramName[program::GEOM], glf::DATA_DIRECTORY + GEOM_PROGRAM_BINARY);
	saveProgram(ProgramName[program::FRAG], glf::DATA_DIRECTORY + FRAG_PROGRAM_BINARY);

	// Delete objects
	glDeleteBuffers(buffer::MAX, BufferName);
	glDeleteVertexArrays(1, &VertexArrayName);
	glDeleteTextures(1, &Texture2DName);
	for(std::size_t i = 0; i < program::MAX; ++i)
		glDeleteProgram(ProgramName[i]);
	glDeleteProgramPipelines(1, &PipelineName);

	return glf::checkError("end");
}

void display()
{
	// Compute the MVP (Model View Projection matrix)
	glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
	glm::mat4 ViewTranslateZ = glm::translate(glm::mat4(1.0), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslateZ, float(Window.RotationCurrent.y), glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 ViewRotateY = glm::rotate(ViewRotateX, float(Window.RotationCurrent.x), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 View = ViewRotateY;
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	glProgramUniformMatrix4fv(ProgramName[program::VERT], UniformMVP, 1, GL_FALSE, &MVP[0][0]);
	glProgramUniform1i(ProgramName[program::FRAG], UniformDiffuse, 0);

	glViewportIndexedfv(0, &glm::vec4(0, 0, Window.Size.x, Window.Size.y)[0]);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f)[0]);

	glBindProgramPipeline(PipelineName);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Texture2DName);

	glBindVertexArray(VertexArrayName);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]); //!\ Need to be called after glBindVertexArray
	glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_INT, NULL, 1, 0);

	glf::checkError("display");
	glf::swapBuffers();
}

int main(int argc, char* argv[])
{
	return glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT, 
		::SAMPLE_MAJOR_VERSION, 
		::SAMPLE_MINOR_VERSION);
}
