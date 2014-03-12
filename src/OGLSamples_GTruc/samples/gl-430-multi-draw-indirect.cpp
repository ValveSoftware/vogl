//**********************************
// OpenGL Multi Draw Indirect
// 25/07/2012 - 15/11/2012
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
	char const * SAMPLE_NAME("OpenGL Multi draw indirect");
	char const * VERT_SHADER_SOURCE("gl-430/multi-draw-indirect.vert");
	char const * FRAG_SHADER_SOURCE("gl-430/multi-draw-indirect.frag");
	char const * TEXTURE_DIFFUSE("kueken1-bgr8.dds");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const ElementCount(15);
	GLsizeiptr const ElementSize = ElementCount * sizeof(glm::uint32);
	glm::uint32 const ElementData[ElementCount] =
	{
		0, 1, 2,
		0, 2, 3,
		0, 1, 2,
		0, 1, 2,
		0, 2, 3
	};

	GLsizei const VertexCount(11);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 0.0f)),

		glf::vertex_v2fv2f(glm::vec2(-0.5f, -1.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2(1.5f, -1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2(0.5f, 1.0f), glm::vec2(1.0f, 0.0f)),

		glf::vertex_v2fv2f(glm::vec2(-0.5f, -1.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2(0.5f, -1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2(1.5f, 1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.5f, 1.0f), glm::vec2(0.0f, 0.0f))
	};

	GLsizei const DrawDataCount(3);
	GLsizeiptr const DrawSize = DrawDataCount * sizeof(glm::uint);
	glm::uint const DrawIDData[DrawDataCount] =
	{
		0, 1, 2
	};

	GLsizei const IndirectBufferCount(3);
	GLsizei const DrawCount[IndirectBufferCount] = {3, 2, 1};

	namespace buffer
	{
		enum type
		{
			VERTEX,
			ELEMENT,
			DRAW_ID,
			TRANSFORM,
			INDIRECT_A,
			INDIRECT_B,
			INDIRECT_C,
			VERTEX_INDIRECTION,
			MAX
		};
	}//namespace buffer

	namespace texture
	{
		enum type
		{
			TEXTURE_A,
			TEXTURE_B,
			TEXTURE_C,
			MAX
		};
	}//namespace texture

	struct DrawArraysIndirectCommand
	{
		GLuint count;
		GLuint primCount;
		GLuint first;
		GLuint baseInstance;
	};

	struct DrawElementsIndirectCommand
	{
		GLuint count;
		GLuint primCount;
		GLuint firstIndex;
		GLint  baseVertex;
		GLuint baseInstance;
	};

	GLuint VertexArrayName(0);
	GLuint PipelineName(0);
	GLuint ProgramName(0);
	GLuint BufferName[buffer::MAX];
	GLuint TextureName[texture::MAX];
	GLint UniformArrayStride(256);

}//namespace

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(1, &PipelineName);

	glf::compiler Compiler;
	GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE, 
		"--version 420 --profile core");
	GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE,
		"--version 420 --profile core");
	Validated = Validated && Compiler.check();

	ProgramName = glCreateProgram();
	glProgramParameteri(ProgramName, GL_PROGRAM_SEPARABLE, GL_TRUE);
	glAttachShader(ProgramName, VertShaderName);
	glAttachShader(ProgramName, FragShaderName);
	glLinkProgram(ProgramName);
	Validated = Validated && glf::checkProgram(ProgramName);

	GLint ActiveUniform(0);
	glGetProgramiv(ProgramName, GL_ACTIVE_UNIFORMS, &ActiveUniform);

	for (GLuint i = 0; i < (GLuint) ActiveUniform; ++i)
	{
		char Name[128];
		memset(Name, '\0', sizeof(Name));
		GLsizei Length(0);

		glGetActiveUniformName(ProgramName, i, GLsizei(sizeof(Name)), &Length, Name);

		std::string StringName(Name);

		if(StringName == std::string("transform.MVP[0]"))
			glGetActiveUniformsiv(ProgramName, 1, &i, GL_UNIFORM_ARRAY_STRIDE, &UniformArrayStride);
	}
	
	if(Validated)
		glUseProgramStages(PipelineName, GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName);

	return Validated;
}

bool initBuffer()
{
	glGenBuffers(buffer::MAX, BufferName);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::DRAW_ID]);
	glBufferData(GL_ARRAY_BUFFER, DrawSize, DrawIDData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	int VertexIndirection[3] = {0, 1, 2};
	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::VERTEX_INDIRECTION]);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(int) * 3, VertexIndirection, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	std::size_t Padding = glm::max(sizeof(glm::mat4), std::size_t(UniformArrayStride));
	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glBufferData(GL_UNIFORM_BUFFER, Padding * 3, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	DrawElementsIndirectCommand CommandA[3];
	CommandA[0].count = ElementCount;
	CommandA[0].primCount = 1;
	CommandA[0].firstIndex = 0;
	CommandA[0].baseVertex = 0;
	CommandA[0].baseInstance = 0;
	CommandA[1].count = ElementCount >> 1;
	CommandA[1].primCount = 1;
	CommandA[1].firstIndex = 6;
	CommandA[1].baseVertex = 4;
	CommandA[1].baseInstance = 1;
	CommandA[2].count = ElementCount;
	CommandA[2].primCount = 1;
	CommandA[2].firstIndex = 9;
	CommandA[2].baseVertex = 7;
	CommandA[2].baseInstance = 2;

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, BufferName[buffer::INDIRECT_A]);
	glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(CommandA), CommandA, GL_STATIC_DRAW);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	DrawElementsIndirectCommand CommandB[2];
	CommandB[0].count = ElementCount;
	CommandB[0].primCount = 1;
	CommandB[0].firstIndex = 0;
	CommandB[0].baseVertex = 0;
	CommandB[0].baseInstance = 0;
	CommandB[1].count = ElementCount;
	CommandB[1].primCount = 1;
	CommandB[1].firstIndex = 6;
	CommandB[1].baseVertex = 4;
	CommandB[1].baseInstance = 0;

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, BufferName[buffer::INDIRECT_B]);
	glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(CommandB), CommandB, GL_STATIC_DRAW);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	DrawElementsIndirectCommand CommandC[1];
	CommandC[0].count = ElementCount;
	CommandC[0].primCount = 1;
	CommandC[0].firstIndex = 0;
	CommandC[0].baseVertex = 8;
	CommandC[0].baseInstance = 0;

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, BufferName[buffer::INDIRECT_C]);
	glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(CommandC), CommandC, GL_STATIC_DRAW);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	return true;
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::DRAW_ID]);
		glVertexAttribIPointer(glf::semantic::attr::DRAW_ID, 1, GL_UNSIGNED_INT, sizeof(glm::uint), GLF_BUFFER_OFFSET(0));
		glVertexAttribDivisor(glf::semantic::attr::DRAW_ID, 1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);
		glEnableVertexAttribArray(glf::semantic::attr::DRAW_ID);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]); 
	glBindVertexArray(0);

	return true;
}

bool initTexture()
{
	bool Validated(true);

	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));
	assert(!Texture.empty());

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(texture::MAX, TextureName);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::TEXTURE_A]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage2D(GL_TEXTURE_2D, GLint(Texture.levels()), GL_RGBA8, GLsizei(Texture.dimensions().x), GLsizei(Texture.dimensions().y));
	for(gli::texture2D::size_type Level = 0; Level < Texture.levels(); ++Level)
	{
		glTexSubImage2D(
			GL_TEXTURE_2D, 
			GLint(Level), 
			0, 0, 
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			GL_BGR, GL_UNSIGNED_BYTE, 
			Texture[Level].data());
	}
	
	///////////////////////////////////////////

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::TEXTURE_B]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage2D(GL_TEXTURE_2D, GLint(Texture.levels()), GL_RGBA8, GLsizei(Texture.dimensions().x), GLsizei(Texture.dimensions().y));
	for(gli::texture2D::size_type Level = 0; Level < Texture.levels(); ++Level)
	{
		glTexSubImage2D(
			GL_TEXTURE_2D, 
			GLint(Level), 
			0, 0, 
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			GL_BGR, GL_UNSIGNED_BYTE, 
			Texture[Level].data());
	}
	
	///////////////////////////////////////////

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::TEXTURE_C]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage2D(GL_TEXTURE_2D, GLint(Texture.levels()), GL_RGBA8, GLsizei(Texture.dimensions().x), GLsizei(Texture.dimensions().y));
	for(gli::texture2D::size_type Level = 0; Level < Texture.levels(); ++Level)
	{
		glTexSubImage2D(
			GL_TEXTURE_2D, 
			GLint(Level), 
			0, 0, 
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			GL_BGR, GL_UNSIGNED_BYTE, 
			Texture[Level].data());
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	return Validated;
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
	bool Success(true);

	// Validate OpenGL support
	Success = Success && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
	Success = Success && glf::checkExtension("GL_ARB_multi_draw_indirect");

	// Create and initialize objects
	if(Success && glf::checkExtension("GL_ARB_debug_output"))
		Success = initDebugOutput();
	if(Success)
		Success = initProgram();
	if(Success)
		Success = initBuffer();
	if(Success)
		Success = initVertexArray();
	if(Success)
		Success = initTexture();

	// Set initial rendering states
	glEnable(GL_DEPTH_TEST);
	glViewportIndexedfv(0, &glm::vec4(0, 0, Window.Size.x, Window.Size.y)[0]);
	glBindProgramPipeline(PipelineName);
	glBindVertexArray(VertexArrayName);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::INDIRECTION, BufferName[buffer::VERTEX_INDIRECTION]);
	glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);

	return Success;
}

bool end()
{
	glDeleteBuffers(buffer::MAX, BufferName);
	glDeleteProgramPipelines(1, &PipelineName);
	glDeleteProgram(ProgramName);
	glDeleteVertexArrays(1, &VertexArrayName);

	return true;
}

void display()
{
	// Selection of the indirect buffer to use
	if(Window.KeyPressed[32] >= IndirectBufferCount)
		Window.KeyPressed[32] = 0;

	std::size_t const IndirectBufferIndex = Window.KeyPressed[32];

	// Clear framebuffer
	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH, 0, &Depth);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f)[0]);

	{
		std::size_t Padding = glm::max(sizeof(glm::mat4), std::size_t(UniformArrayStride));

		// Update the transformation matrix
		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glm::byte* Pointer = (glm::byte*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, Padding * 3,
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y + 45.0f, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x + 45.0f, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 Model = glm::mat4(1.0f);

		*reinterpret_cast<glm::mat4*>(Pointer + Padding * 0) = Projection * View * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f));
		*reinterpret_cast<glm::mat4*>(Pointer + Padding * 1) = Projection * View * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
		*reinterpret_cast<glm::mat4*>(Pointer + Padding * 2) = Projection * View * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.5f));
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::TEXTURE_A]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::TEXTURE_B]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::TEXTURE_C]);

	// Draw
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, BufferName[buffer::INDIRECT_A] + GLuint(IndirectBufferIndex));
	glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, DrawCount[IndirectBufferIndex], sizeof(DrawElementsIndirectCommand));

	// Swap framebuffers
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
