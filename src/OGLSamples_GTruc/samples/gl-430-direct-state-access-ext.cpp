//**********************************
// OpenGL Direct State Access
// 20/02/2011 - 05/07/2012
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
	char const * SAMPLE_NAME("OpenGL Direct State Access");	
	char const * TEXTURE_DIFFUSE("kueken3-bgr8.dds");
	glm::ivec2 const FRAMEBUFFER_SIZE(160, 120);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

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

	namespace buffer
	{
		enum type
		{
			VERTEX,
			TRANSFORM,
			BLIT,
			MAX
		};
	}//namespace buffer

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			MULTISAMPLE,
			DEPTH,
			MAX
		};
	}//namespace buffer

	char const * VERT_SHADER_SOURCE("gl-430/dsa-pass1.vert");
	char const * FRAG_SHADER_SOURCE[program::MAX] = 
	{
		"gl-430/dsa-pass1.frag",
		"gl-430/dsa-pass2-msaa-box.frag",
		"gl-430/dsa-pass2-msaa-near.frag",
	};

	GLuint PipelineName[program::MAX] = {0, 0, 0};
	GLuint ProgramName[program::MAX] = {0, 0, 0};
	GLuint BufferName[buffer::MAX] = {0, 0, 0};
	GLuint VertexArrayName(0);
	GLuint FramebufferName(0);
	GLuint TextureName[texture::MAX] = {0, 0, 0};
	GLuint SamplerName(0);
}//namespace

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

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(program::MAX, PipelineName);

	for(int i = 0; i < program::MAX; ++i)
	{
		glf::compiler Compiler;
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE, 
			"--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE[i],
			"--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[i] = glCreateProgram();
		glProgramParameteri(ProgramName[i], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[i], VertShaderName);
		glAttachShader(ProgramName[i], FragShaderName);
		glLinkProgram(ProgramName[i]);
		Validated = Validated && glf::checkProgram(ProgramName[i]);

		if(Validated)
			glUseProgramStages(PipelineName[i], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[i]);
	}

	return Validated;
}

bool initBuffer()
{
	glGenBuffers(buffer::MAX, BufferName);

	glNamedBufferDataEXT(BufferName[buffer::VERTEX], VertexSize, VertexData, GL_STATIC_DRAW);
	glNamedBufferDataEXT(BufferName[buffer::TRANSFORM], sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
	glNamedBufferDataEXT(BufferName[buffer::BLIT], sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);

	return true;
}

bool initTexture()
{
	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));
	assert(!Texture.empty());

	glGenTextures(texture::MAX, TextureName);

	//glTextureImage2DEXT(TextureName[texture::DIFFUSE], GL_TEXTURE_2D, 0, GL_RGB8, GLsizei(Texture[0].dimensions().x), GLsizei(Texture[0].dimensions().y), 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glTextureStorage2DEXT(TextureName[texture::DIFFUSE], GL_TEXTURE_2D, GLint(Texture.levels()), GL_RGB8, GLsizei(Texture[0].dimensions().x), GLsizei(Texture[0].dimensions().y));
	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glTextureSubImage2DEXT(
			TextureName[texture::DIFFUSE],
			GL_TEXTURE_2D, 
			GLint(Level), 
			0, 0, 
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			GL_BGR, 
			GL_UNSIGNED_BYTE, 
			Texture[Level].data());
	}
	if(Texture.levels() == 1)
		glGenerateTextureMipmapEXT(TextureName[texture::DIFFUSE], GL_TEXTURE_2D);

	// From GL_ARB_texture_storage_multisample
	glTextureStorage2DMultisampleEXT(TextureName[texture::MULTISAMPLE], GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, GL_TRUE);
	glTextureStorage2DMultisampleEXT(TextureName[texture::DEPTH], GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH_COMPONENT24, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, GL_TRUE);

	return true;
}

bool initFramebuffer()
{
	bool Validated(true);

	glGenFramebuffers(1, &FramebufferName);
	glNamedFramebufferTextureEXT(FramebufferName, GL_DEPTH_ATTACHMENT, TextureName[texture::DEPTH], 0);
	glNamedFramebufferTextureEXT(FramebufferName, GL_COLOR_ATTACHMENT0, TextureName[texture::MULTISAMPLE], 0);
	Validated = Validated && (glCheckNamedFramebufferStatusEXT(FramebufferName, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	return Validated;
}

bool initVertexArray()
{
	GLuint Bindingindex(0);

	glGenVertexArrays(1, &VertexArrayName);
    glBindVertexBuffer( Bindingindex, BufferName[buffer::VERTEX], 0, sizeof(glf::vertex_v2fv2f));

    glVertexAttribBinding( glf::semantic::attr::POSITION, Bindingindex);
    glVertexAttribFormat( glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, 0);

    glVertexAttribBinding( glf::semantic::attr::TEXCOORD, Bindingindex);
    glVertexAttribFormat( glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2));

    glEnableVertexAttribArray( glf::semantic::attr::POSITION);
    glEnableVertexAttribArray( glf::semantic::attr::TEXCOORD);


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
	bool Validated = true;
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
	Validated = Validated && glf::checkExtension("GL_EXT_direct_state_access");
	Validated = Validated && glf::checkExtension("GL_ARB_texture_storage_multisample");
	Validated = Validated && glf::checkExtension("GL_ARB_vertex_attrib_binding");

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
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();

	return Validated;
}

bool end()
{
	glDeleteProgramPipelines(program::MAX, PipelineName);
	glDeleteBuffers(buffer::MAX, BufferName);
	glDeleteSamplers(1, &SamplerName);
	glDeleteTextures(texture::MAX, TextureName);
	glDeleteFramebuffers(1, &FramebufferName);
	glDeleteVertexArrays(1, &VertexArrayName);
	for(int i = 0; i < program::MAX; ++i)
		glDeleteProgram(ProgramName[i]);

	return true;
}

void renderFBO()
{
	{
		glm::mat4* Pointer = (glm::mat4*)glMapNamedBufferRangeEXT(
			BufferName[buffer::TRANSFORM], 0, sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		glm::mat4 Perspective = glm::perspective(45.0f, float(FRAMEBUFFER_SIZE.x) / FRAMEBUFFER_SIZE.y, 0.1f, 100.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0 - 4.0));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 Model = glm::mat4(1.0f);
		glm::mat4 MVP = Perspective * View * Model;

		*Pointer = MVP;
		glUnmapNamedBufferEXT(BufferName[buffer::TRANSFORM]);
		//glNamedBufferSubDataEXT(BufferName[buffer::TRANSFORM], 0, sizeof(glm::mat4), &MVP[0][0]);
	}

	glViewportIndexedf(0, 0, 0, float(FRAMEBUFFER_SIZE.x), float(FRAMEBUFFER_SIZE.y));
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH, 0, &Depth);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	glBindProgramPipeline(PipelineName[program::THROUGH]);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
	glBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glBindSampler(0, SamplerName);
	glBindVertexArray(VertexArrayName);

	glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 5, 0);

	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);
}

void resolveMultisampling()
{
	{
		glm::mat4* Pointer = (glm::mat4*)glMapNamedBufferRangeEXT(
			BufferName[buffer::BLIT], 0, sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

		glm::mat4 Perspective = glm::perspective(45.0f, float(Window.Size.x) / Window.Size.y, 0.1f, 100.0f);
		glm::mat4 ViewFlip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f,-1.0f, 1.0f));
		glm::mat4 ViewTranslate = glm::translate(ViewFlip, glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
		glm::mat4 View = glm::rotate(ViewTranslate,-15.f, glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 Model = glm::mat4(1.0f);
		glm::mat4 MVP = Perspective * View * Model;

		*Pointer = MVP;
		glUnmapNamedBufferEXT(BufferName[buffer::BLIT]);
		//glNamedBufferSubDataEXT(BufferName[buffer::BLIT], 0, sizeof(glm::mat4), &MVP[0][0]);
	}

	glViewportIndexedf(0, 0, 0, float(Window.Size.x), float(Window.Size.y));
	glEnable(GL_SCISSOR_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::BLIT]);
	glBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_2D_MULTISAMPLE, TextureName[texture::MULTISAMPLE]);
	glBindSampler(0, SamplerName);
	glBindVertexArray(VertexArrayName);

	// Box
	{
		glScissorIndexed(0, 1, 1, Window.Size.x  / 2 - 2, Window.Size.y - 2);
		glBindProgramPipeline(PipelineName[program::RESOLVE_BOX]);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 5, 0);
	}

	// Near
	{
		glScissorIndexed(0, Window.Size.x / 2 + 1, 1, Window.Size.x / 2 - 2, Window.Size.y - 2);
		glBindProgramPipeline(PipelineName[program::RESOLVE_NEAR]);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 5, 0);
	}

	glDisable(GL_SCISSOR_TEST);
}

void display()
{
	// Clear the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	// Pass 1: Render the scene in a multisampled framebuffer
	renderFBO();

	// Pass 2: Resolved and render the colorbuffer from the multisampled framebuffer
	resolveMultisampling();

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
