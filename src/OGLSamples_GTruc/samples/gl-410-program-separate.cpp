//**********************************
// OpenGL Separate program
// 02/08/2010 - 29/09/2011
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
	char const * SAMPLE_NAME("OpenGL Separate program");
	char const * VERTEX_SHADER_SOURCE("gl-410/separate.vert");
	char const * FRAGMENT_SHADER_SOURCE("gl-410/separate.frag");
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
			VERTEX,
			FRAGMENT,
			MAX
		};
	}//namespace program


	GLuint PipelineName(0);
	GLuint SeparateProgramName[program::MAX];
	GLint SeparateUniformMVP(0);
	GLint SeparateUniformDiffuse(0);
	
	GLuint UnifiedProgramName;
	GLint UnifiedUniformMVP(0);
	GLint UnifiedUniformDiffuse(0);

	GLuint BufferName[buffer::MAX];
	GLuint VertexArrayName;
	GLuint TextureName(0);
}//namespace

bool initDebugOutput()
{
	bool Validated(true);

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return Validated;
}

bool initUnifiedProgram()
{
	bool Validated = true;

	// Create program
	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERTEX_SHADER_SOURCE);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAGMENT_SHADER_SOURCE);

		UnifiedProgramName = glCreateProgram();
		glAttachShader(UnifiedProgramName, VertShaderName);
		glAttachShader(UnifiedProgramName, FragShaderName);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);
		glLinkProgram(UnifiedProgramName);
		Validated = Validated && glf::checkProgram(UnifiedProgramName);
	}

	if(Validated)
	{
		UnifiedUniformMVP = glGetUniformLocation(UnifiedProgramName, "MVP");
		UnifiedUniformDiffuse = glGetUniformLocation(UnifiedProgramName, "Diffuse");
	}

	return Validated && glf::checkError("initProgram");
}

bool initSeparateProgram()
{
	bool Validated = true;

	glGenProgramPipelines(1, &PipelineName);

	if(Validated)
	{
		std::string VertexSourceContent = glf::loadFile(glf::DATA_DIRECTORY + VERTEX_SHADER_SOURCE);
		char const * VertexSourcePointer = VertexSourceContent.c_str();
		SeparateProgramName[program::VERTEX] = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &VertexSourcePointer);
		Validated = glf::checkProgram(SeparateProgramName[program::VERTEX]);
	}

	if(Validated)
		glUseProgramStages(PipelineName, GL_VERTEX_SHADER_BIT, SeparateProgramName[program::VERTEX]);

	if(Validated)
	{
		std::string FragmentSourceContent = glf::loadFile(glf::DATA_DIRECTORY + FRAGMENT_SHADER_SOURCE);
		char const * FragmentSourcePointer = FragmentSourceContent.c_str();
		SeparateProgramName[program::FRAGMENT] = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &FragmentSourcePointer);
		Validated = glf::checkProgram(SeparateProgramName[program::FRAGMENT]);
	}

	if(Validated)
		glUseProgramStages(PipelineName, GL_FRAGMENT_SHADER_BIT, SeparateProgramName[program::FRAGMENT]);

	// Get variables locations
	if(Validated)
	{
		SeparateUniformMVP = glGetUniformLocation(SeparateProgramName[program::VERTEX], "MVP");
		SeparateUniformDiffuse = glGetUniformLocation(SeparateProgramName[program::FRAGMENT], "Diffuse");
	}

	return Validated && glf::checkError("initProgram");
}

bool initTexture()
{
	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE_DXT5));

	glGenTextures(1, &TextureName);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

	for(std::size_t Level(0); Level < Texture.levels(); ++Level)
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

	return glf::checkError("initTexture");
}

bool initVertexBuffer()
{
	glGenBuffers(buffer::MAX, BufferName);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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
	bool Validated = true;
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initSeparateProgram();
	if(Validated)
		Validated = initUnifiedProgram();
	if(Validated)
		Validated = initVertexBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initTexture();

	return Validated && glf::checkError("begin");
}

bool end()
{
	// Delete objects
	glDeleteBuffers(buffer::MAX, BufferName);
	glDeleteVertexArrays(1, &VertexArrayName);
	glDeleteTextures(1, &TextureName);
	glDeleteProgram(SeparateProgramName[program::VERTEX]);
	glDeleteProgram(SeparateProgramName[program::FRAGMENT]);
	glDeleteProgram(UnifiedProgramName);
	glDeleteProgramPipelines(1, &PipelineName);

	return glf::checkError("end");
}

void display()
{
	// Compute the MVP (Model View Projection matrix)
    glm::mat4 Projection = glm::perspective(45.0f, 2.0f / 3.0f, 0.1f, 100.0f);
	glm::mat4 ViewTranslateZ = glm::translate(glm::mat4(1.0), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslateZ, float(Window.RotationCurrent.y), glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 ViewRotateY = glm::rotate(ViewRotateX, float(Window.RotationCurrent.x), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 View = ViewRotateY;
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f)[0]);

	glBindTexture(GL_TEXTURE_2D, TextureName);
	glBindVertexArray(VertexArrayName);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);

	// Render with the separate programs
	glViewportIndexedfv(0, &glm::vec4(0, 0, Window.Size.x / 2, Window.Size.y)[0]);
	glProgramUniformMatrix4fv(SeparateProgramName[program::VERTEX], SeparateUniformMVP, 1, GL_FALSE, &MVP[0][0]);
	glProgramUniform1i(SeparateProgramName[program::FRAGMENT], SeparateUniformDiffuse, 0);
	glBindProgramPipeline(PipelineName);
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_INT, NULL, 1, 0);
	glBindProgramPipeline(0);

	// Render with the unified programs
	glViewportIndexedfv(0, &glm::vec4(Window.Size.x / 2, 0, Window.Size.x / 2, Window.Size.y)[0]);
	glProgramUniformMatrix4fv(UnifiedProgramName, UnifiedUniformMVP, 1, GL_FALSE, &MVP[0][0]);
	glProgramUniform1i(UnifiedProgramName, UnifiedUniformDiffuse, 0);
	glUseProgram(UnifiedProgramName);
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_INT, NULL, 1, 0);
	glUseProgram(0);

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
