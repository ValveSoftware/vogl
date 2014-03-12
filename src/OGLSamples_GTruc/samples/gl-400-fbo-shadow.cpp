//**********************************
// OpenGL Shadow Gather
// 11/02/2013 - 11/02/2013
//**********************************
// Christophe Riccio
// ogl-samples@g-truc.net
//**********************************
// G-Truc Creation
// www.g-truc.net
//**********************************


#include <glf/glf.hpp>
#include <glf/compiler.hpp>

namespace
{
	char const * SAMPLE_NAME("OpenGL Shadow Gather");
	char const * VERT_SHADER_SOURCE_DEPTH("gl-400/fbo-shadow-depth.vert");
	char const * VERT_SHADER_SOURCE_RENDER("gl-400/fbo-shadow-render.vert");
	char const * FRAG_SHADER_SOURCE_RENDER("gl-400/fbo-shadow-render.frag");
	char const * TEXTURE_DIFFUSE("kueken1-dxt1.dds");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(0);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(8);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v3fv4u8);
	glf::vertex_v3fv4u8 const VertexData[VertexCount] =
	{
		glf::vertex_v3fv4u8(glm::vec3(-1.0f,-1.0f, 0.0f), glm::u8vec4(255, 127,   0, 255)),
		glf::vertex_v3fv4u8(glm::vec3( 1.0f,-1.0f, 0.0f), glm::u8vec4(255, 127,   0, 255)),
		glf::vertex_v3fv4u8(glm::vec3( 1.0f, 1.0f, 0.0f), glm::u8vec4(255, 127,   0, 255)),
		glf::vertex_v3fv4u8(glm::vec3(-1.0f, 1.0f, 0.0f), glm::u8vec4(255, 127,   0, 255)),
		glf::vertex_v3fv4u8(glm::vec3(-0.1f,-0.1f, 0.2f), glm::u8vec4(  0, 127, 255, 255)),
		glf::vertex_v3fv4u8(glm::vec3( 0.1f,-0.1f, 0.2f), glm::u8vec4(  0, 127, 255, 255)),
		glf::vertex_v3fv4u8(glm::vec3( 0.1f, 0.1f, 0.2f), glm::u8vec4(  0, 127, 255, 255)),
		glf::vertex_v3fv4u8(glm::vec3(-0.1f, 0.1f, 0.2f), glm::u8vec4(  0, 127, 255, 255))
	};

	GLsizei const ElementCount(12);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0,
		4, 5, 6, 
		6, 7, 4,
	};

	namespace buffer
	{
		enum type
		{
			VERTEX,
			ELEMENT,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			COLORBUFFER,
			RENDERBUFFER,
			SHADOWMAP,
			MAX
		};
	}//namespace texture
	
	namespace program
	{
		enum type
		{
			DEPTH,
			RENDER,
			MAX
		};
	}//namespace program

	namespace framebuffer
	{
		enum type
		{
			FRAMEBUFFER,
			SHADOW,
			MAX
		};
	}//namespace framebuffer

	std::vector<GLuint> FramebufferName(framebuffer::MAX);
	std::vector<GLuint> ProgramName(program::MAX);
	std::vector<GLuint> VertexArrayName(program::MAX);
	std::vector<GLuint> BufferName(buffer::MAX);
	std::vector<GLuint> TextureName(texture::MAX);
	std::vector<GLint> UniformTransform(program::MAX);
	GLint UniformShadow(0);
	glm::ivec2 const ShadowSize(1024, 1024);
}//namespace

bool initProgram()
{
	bool Validated(true);
	
	if(Validated)
	{
		glf::compiler Compiler;
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_RENDER, 
			"--version 400 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_RENDER,
			"--version 400 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[program::RENDER] = glCreateProgram();
		glAttachShader(ProgramName[program::RENDER], VertShaderName);
		glAttachShader(ProgramName[program::RENDER], FragShaderName);
		glBindAttribLocation(ProgramName[program::RENDER], glf::semantic::attr::POSITION, "Position");
		glBindAttribLocation(ProgramName[program::RENDER], glf::semantic::attr::COLOR, "Color");
		glBindFragDataLocation(ProgramName[program::RENDER], glf::semantic::frag::COLOR, "Color");
		glLinkProgram(ProgramName[program::RENDER]);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		Validated = Validated && glf::checkProgram(ProgramName[program::RENDER]);
	}

	if(Validated)
	{
		UniformTransform[program::RENDER] = glGetUniformBlockIndex(ProgramName[program::RENDER], "transform");
		UniformShadow = glGetUniformLocation(ProgramName[program::RENDER], "Shadow");
	}

	if(Validated)
	{
		glf::compiler Compiler;
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_DEPTH, 
			"--version 400 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[program::DEPTH] = glCreateProgram();
		glAttachShader(ProgramName[program::DEPTH], VertShaderName);
		glBindAttribLocation(ProgramName[program::DEPTH], glf::semantic::attr::POSITION, "Position");
		glLinkProgram(ProgramName[program::DEPTH]);
		glDeleteShader(VertShaderName);

		Validated = Validated && glf::checkProgram(ProgramName[program::DEPTH]);
	}

	if(Validated)
	{
		UniformTransform[program::DEPTH] = glGetUniformBlockIndex(ProgramName[program::DEPTH], "transform");
	}

	return Validated;
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

	GLint UniformBufferOffset(0);

	glGetIntegerv(
		GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		&UniformBufferOffset);

	GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4) * 3), UniformBufferOffset);

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return true;
}

bool initTexture()
{
	bool Validated(true);

	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));
	assert(!Texture.empty());

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(texture::MAX, &TextureName[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glCompressedTexImage2D(
			GL_TEXTURE_2D,
			GLint(Level),
			GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			0, 
			GLsizei(Texture[Level].size()), 
			Texture[Level].data());
	}
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, GLint(0), GL_RGBA8, GLsizei(Window.Size.x), GLsizei(Window.Size.y), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::RENDERBUFFER]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, GLint(0), GL_DEPTH_COMPONENT24, GLsizei(Window.Size.x), GLsizei(Window.Size.y), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::SHADOWMAP]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexImage2D(GL_TEXTURE_2D, GLint(0), GL_DEPTH_COMPONENT24, ShadowSize.x, ShadowSize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	return Validated;
}

bool initVertexArray()
{
	glGenVertexArrays(program::MAX, &VertexArrayName[0]);
	glBindVertexArray(VertexArrayName[program::RENDER]);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v3fv4u8), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glf::vertex_v3fv4u8), GLF_BUFFER_OFFSET(sizeof(glm::vec3)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::COLOR);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBindVertexArray(0);

	return true;
}

bool initFramebuffer()
{
	bool Validated(true);

	glGenFramebuffers(framebuffer::MAX, &FramebufferName[0]);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::FRAMEBUFFER]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::COLORBUFFER], 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, TextureName[texture::RENDERBUFFER], 0);
	if(glf::checkFramebuffer(FramebufferName[framebuffer::FRAMEBUFFER]))
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::SHADOW]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, TextureName[texture::SHADOWMAP], 0);
	glDrawBuffer(GL_NONE);
	if(glf::checkFramebuffer(FramebufferName[framebuffer::FRAMEBUFFER]))
		return false;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();

	return Validated;
}

bool end()
{
	bool Validated(true);

	glDeleteFramebuffers(framebuffer::MAX, &FramebufferName[0]);
	for(std::size_t i = 0; i < program::MAX; ++i)
		glDeleteProgram(ProgramName[i]);
	glDeleteBuffers(buffer::MAX, &BufferName[0]);
	glDeleteTextures(texture::MAX, &TextureName[0]);
	glDeleteVertexArrays(program::MAX, &VertexArrayName[0]);

	return Validated;
}

void renderShadow()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glViewport(0, 0, ShadowSize.x, ShadowSize.y);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::SHADOW]);
	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH , 0, &Depth);

	// Bind rendering objects
	glUseProgram(ProgramName[program::DEPTH]);
	glUniformBlockBinding(ProgramName[program::DEPTH], UniformTransform[program::DEPTH], glf::semantic::uniform::TRANSFORM0);

	glBindVertexArray(VertexArrayName[program::RENDER]);
	glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, 0, 1, 0);

	glDisable(GL_DEPTH_TEST);
}

void renderFramebuffer()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glViewport(0, 0, Window.Size.x, Window.Size.y);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH , 0, &Depth);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)[0]);

	glUseProgram(ProgramName[program::RENDER]);
	glUniform1i(UniformShadow, 0);
	glUniformBlockBinding(ProgramName[program::RENDER], UniformTransform[program::RENDER], glf::semantic::uniform::TRANSFORM0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::SHADOWMAP]);

	glBindVertexArray(VertexArrayName[program::RENDER]);
	glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, 0, 1, 0);

	glDisable(GL_DEPTH_TEST);
}

void display()
{
	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(
		GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4) * 3,
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	// Update of the MVP matrix for the render pass
	{
		glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.01f, 5.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y + 3.0f));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y - 60.f, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x + 30.f, glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 Model = glm::mat4(1.0f);
		*(Pointer + 0) = Projection * View * Model;
	}

	// Update of the MVP matrix for the depth pass
	{
		glm::mat4 Projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f,-4.0f, 8.0f);
		glm::mat4 View = glm::lookAt(glm::vec3(0.5, 1.0, 2.0), glm::vec3(0), glm::vec3(0, 0, 1));
		glm::mat4 Model = glm::mat4(1.0f);
		glm::mat4 DepthMVP = Projection * View * Model;
		*(Pointer + 1) = DepthMVP;

		glm::mat4 BiasMatrix(
			0.5, 0.0, 0.0, 0.0, 
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 0.5, 0.0,
			0.5, 0.5, 0.5, 1.0);

		*(Pointer + 2) = BiasMatrix * DepthMVP;
	}

	glUnmapBuffer(GL_UNIFORM_BUFFER);

	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);

	renderShadow();
	renderFramebuffer();
	//renderSplash();

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
