//**********************************
// OpenGL Sampler Wrap
// 11/08/2013 - 11/08/2013
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
	char const * SAMPLE_NAME("OpenGL Sampler Wrap");	
	char const * VERT_SHADER_SOURCE("gl-440/sampler-wrap.vert");
	char const * FRAG_SHADER_SOURCE("gl-440/sampler-wrap.frag");
	char const * TEXTURE_DIFFUSE_DXT5("kueken1-dxt5.dds");
	int const SAMPLE_SIZE_WIDTH(1024);
	int const SAMPLE_SIZE_HEIGHT(768);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(4);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	struct vertex
	{
		vertex
		(
			glm::vec2 const & Position,
			glm::vec2 const & Texcoord
		) :
			Position(Position),
			Texcoord(Texcoord)
		{}

		glm::vec2 Position;
		glm::vec2 Texcoord;
	};

	// With DDS textures, v texture coordinate are reversed, from top to bottom
	GLsizei const VertexCount(6);
	GLsizeiptr const VertexSize(VertexCount * sizeof(vertex));
	vertex const VertexData[VertexCount] =
	{
		vertex(glm::vec2(-1.0f,-1.0f), glm::vec2(-2.0f, 2.0f)),
		vertex(glm::vec2( 1.0f,-1.0f), glm::vec2( 2.0f, 2.0f)),
		vertex(glm::vec2( 1.0f, 1.0f), glm::vec2( 2.0f,-2.0f)),
		vertex(glm::vec2( 1.0f, 1.0f), glm::vec2( 2.0f,-2.0f)),
		vertex(glm::vec2(-1.0f, 1.0f), glm::vec2(-2.0f,-2.0f)),
		vertex(glm::vec2(-1.0f,-1.0f), glm::vec2(-2.0f, 2.0f))
	};

	namespace viewport
	{
		enum type
		{
			VIEWPORT0,
			VIEWPORT1,
			VIEWPORT2,
			VIEWPORT3,
			VIEWPORT4,
			VIEWPORT5,
			MAX
		};
	}//namespace viewport

	namespace buffer
	{
		enum type
		{
			VERTEX,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	GLuint VertexArrayName(0);
	GLuint PipelineName(0);
	GLuint ProgramName(0);

	std::vector<GLuint> BufferName(buffer::MAX);
	GLuint TextureName(0);

	std::vector<GLuint> SamplerName(viewport::MAX);
	std::vector<glm::vec4> Viewport(viewport::MAX);

	glm::mat4* UniformPointer(NULL);
}//namespace

bool initDebugOutput()
{
	bool Validated(true);

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return Validated;
}

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(1, &PipelineName);

	if(Validated)
	{
		glf::compiler Compiler;
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, 
			glf::DATA_DIRECTORY + VERT_SHADER_SOURCE, 
			"--version 440 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, 
			glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE,
			"--version 440 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName = glCreateProgram();
		glProgramParameteri(ProgramName, GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName, VertShaderName);
		glAttachShader(ProgramName, FragShaderName);
		glLinkProgram(ProgramName);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		Validated = Validated && glf::checkProgram(ProgramName);
	}

	if(Validated)
		glUseProgramStages(PipelineName, GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName);

	return Validated && glf::checkError("initProgram");
}

bool initBuffer()
{
	// Generate a buffer object
	glGenBuffers(buffer::MAX, &BufferName[0]);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
	glBufferStorage(GL_ARRAY_BUFFER, VertexSize, VertexData, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLint UniformBufferOffset(0);

	glGetIntegerv(
		GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		&UniformBufferOffset);

	GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glBufferStorage(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return glf::checkError("initBuffer");;
}

bool initSampler()
{
	glGenSamplers(viewport::MAX, &SamplerName[0]);

	for(std::size_t i = 0; i < viewport::MAX; ++i)
	{
		glSamplerParameteri(SamplerName[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(SamplerName[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glm::vec4 BorderColor(0.0f, 0.5f, 1.0f, 1.0f);
		glSamplerParameterfv(SamplerName[i], GL_TEXTURE_BORDER_COLOR, &BorderColor[0]);
	}

	glSamplerParameteri(SamplerName[viewport::VIEWPORT0], GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT1], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT2], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT3], GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT4], GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_EDGE);
	if(glf::checkExtension("GL_EXT_texture_mirror_clamp"))
		glSamplerParameteri(SamplerName[viewport::VIEWPORT5], GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_BORDER_EXT);
	else
		glSamplerParameteri(SamplerName[viewport::VIEWPORT2], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);

	glSamplerParameteri(SamplerName[viewport::VIEWPORT0], GL_TEXTURE_WRAP_T, GL_REPEAT);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT1], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT2], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT3], GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	glSamplerParameteri(SamplerName[viewport::VIEWPORT4], GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_EDGE);
	if(glf::checkExtension("GL_EXT_texture_mirror_clamp"))
		glSamplerParameteri(SamplerName[viewport::VIEWPORT5], GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_BORDER_EXT);
	else
		glSamplerParameteri(SamplerName[viewport::VIEWPORT2], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	return glf::checkError("initSampler");
}

bool initTexture()
{
	glGenTextures(1, &TextureName);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName);

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

	return glf::checkError("initTexture");
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glVertexAttribFormat(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, 0);
		glVertexAttribBinding(glf::semantic::attr::POSITION, glf::semantic::buffer::STATIC);
		glEnableVertexAttribArray(glf::semantic::attr::POSITION);

		glVertexAttribFormat(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2));
		glVertexAttribBinding(glf::semantic::attr::TEXCOORD, glf::semantic::buffer::STATIC);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);
	glBindVertexArray(0);

	return glf::checkError("initVertexArray");
}

bool begin()
{
	glm::vec2 ViewportSize(Window.Size.x * 0.33f, Window.Size.y * 0.50f);

	Viewport[viewport::VIEWPORT0] = glm::vec4(ViewportSize.x * 0.0f, ViewportSize.y * 0.0f, ViewportSize.x * 1.0f, ViewportSize.y * 1.0f);
	Viewport[viewport::VIEWPORT1] = glm::vec4(ViewportSize.x * 1.0f, ViewportSize.y * 0.0f, ViewportSize.x * 1.0f, ViewportSize.y * 1.0f);
	Viewport[viewport::VIEWPORT2] = glm::vec4(ViewportSize.x * 2.0f, ViewportSize.y * 0.0f, ViewportSize.x * 1.0f, ViewportSize.y * 1.0f);
	Viewport[viewport::VIEWPORT3] = glm::vec4(ViewportSize.x * 0.0f, ViewportSize.y * 1.0f, ViewportSize.x * 1.0f, ViewportSize.y * 1.0f);
	Viewport[viewport::VIEWPORT4] = glm::vec4(ViewportSize.x * 1.0f, ViewportSize.y * 1.0f, ViewportSize.x * 1.0f, ViewportSize.y * 1.0f);
	Viewport[viewport::VIEWPORT5] = glm::vec4(ViewportSize.x * 2.0f, ViewportSize.y * 1.0f, ViewportSize.x * 1.0f, ViewportSize.y * 1.0f);

	bool Validated = glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated)
		Validated = initDebugOutput();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initSampler();
	if(Validated)
		Validated = initVertexArray();

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	UniformPointer = (glm::mat4*)glMapBufferRange(
		GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4),
		GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

	return Validated && glf::checkError("begin");
}

bool end()
{
	glDeleteBuffers(buffer::MAX, &BufferName[0]);
	glDeleteProgram(ProgramName);
	glDeleteProgramPipelines(1, &PipelineName);
	glDeleteTextures(1, &TextureName);
	glDeleteVertexArrays(1, &VertexArrayName);

	return glf::checkError("end");
}

void display()
{
	{
		// Compute the MVP (Model View Projection matrix)
		float Aspect = (Window.Size.x * 0.33f) / (Window.Size.y * 0.50f);
		glm::mat4 Projection = glm::perspective(45.0f, Aspect, 0.1f, 100.0f);
		glm::mat4 ViewTranslateZ = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslateZ, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 ViewRotateY = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 View = ViewRotateY;
		glm::mat4 Model = glm::mat4(1.0f);
		glm::mat4 MVP = Projection * View * Model;

		*UniformPointer = MVP;
	}

	glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4));

	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	glBindProgramPipeline(PipelineName);
	glBindBuffersBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, 1, &BufferName[buffer::TRANSFORM]);
	glBindTextures(glf::semantic::sampler::DIFFUSE, 1, &TextureName);
	glBindVertexArray(VertexArrayName);
	glBindVertexBuffer(glf::semantic::buffer::STATIC, BufferName[buffer::VERTEX], 0, GLsizei(sizeof(vertex)));

	for(std::size_t Index = 0; Index < viewport::MAX; ++Index)
	{
		glViewportIndexedf(0, 
			Viewport[Index].x, 
			Viewport[Index].y, 
			Viewport[Index].z, 
			Viewport[Index].w);

		glBindSamplers(0, 1, &SamplerName[Index]);
		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);
	}

	glf::checkError("display");
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
