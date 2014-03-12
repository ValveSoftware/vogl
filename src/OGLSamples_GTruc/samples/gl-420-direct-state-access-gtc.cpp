//**********************************
// OpenGL Framebuffer Multisample
// 20/02/2011 - 22/08/2011
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
	typedef GLintptr GLname;
	typedef void (GLAPIENTRY * PFNGLBINDNAMEDTEXTURELEVELPROC) (GLenum target, GLuint unit, GLname texture, GLint level);
	typedef void (GLAPIENTRY * PFNGLNAMEDTEXSTORAGEPROC) (GLname texture, GLenum target, GLsizei levels, GLsizei samples, GLboolean fixedsamplelocations, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

	PFNGLBINDNAMEDTEXTURELEVELPROC glBindNamedTextureLevelGTC = 0;
	PFNGLNAMEDTEXSTORAGEPROC glNamedTexStorageGTC = 0;

	std::string const SAMPLE_NAME = "OpenGL Framebuffer Multisample";	
	std::string const TEXTURE_DIFFUSE(glf::DATA_DIRECTORY + "kueken320-rgb8.tga");
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

	std::string const VERT_SHADER_SOURCE(glf::DATA_DIRECTORY + "420/instanced-image-2d.vert");
	std::string const FRAG_SHADER_SOURCE[program::MAX] = 
	{
		glf::DATA_DIRECTORY + "420/instanced-image-2d.frag",
		glf::DATA_DIRECTORY + "420/multisample-box.frag",
		glf::DATA_DIRECTORY + "420/multisample-near.frag",
	};

	GLname PipelineName[program::MAX] = {0, 0, 0};
	GLname ProgramName[program::MAX] = {0, 0, 0};
	GLname BufferName[buffer::MAX] = {0, 0, 0};
	GLname VertexArrayName(0);
	GLname FramebufferName(0);
	GLname TextureName[texture::MAX] = {0, 0, 0};
	GLname SamplerName(0);
}//namespace

bool initSampler()
{
	glGenNames(1, GL_SAMPLER, &SamplerName);
	glNamedSamplerParameteri(SamplerName, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glNamedSamplerParameteri(SamplerName, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glNamedSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glNamedSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glNamedSamplerParameterfv(SamplerName, GL_TEXTURE_BORDER_COLOR, &glm::vec4(0.0f)[0]);
	glNamedSamplerParameterf(SamplerName, GL_TEXTURE_MIN_LOD, -1000.f);
	glNamedSamplerParameterf(SamplerName, GL_TEXTURE_MAX_LOD, 1000.f);
	glNamedSamplerParameterf(SamplerName, GL_TEXTURE_LOD_BIAS, 0.0f);
	glNamedSamplerParameteri(SamplerName, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glNamedSamplerParameteri(SamplerName, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	return glf::checkError("initSampler");
}

bool initProgram()
{
	bool Validated = true;
	
	glGenNames(program::MAX, GL_PROGRAM_PIPELINE, PipelineName);
	glGenNames(program::MAX, GL_PROGRAM_GTC, ProgramName);

	for(int i = 0; i < program::MAX; ++i)
	{
		GLname VertShaderName(0);
		GLname FragShaderName(0);
		glGenNames(1, GL_VERTEX_SHADER, &VertShaderName);
		glGenNames(1, GL_FRAGMENT_SHADER, &FragShaderName);

		std::string VertSourceContent = glf::loadFile(VERT_SHADER_SOURCE);
		char const * VertSourcePointer = VertSourceContent.c_str();
		glNamedShaderSource(VertShaderName, 1, &VertSourcePointer, NULL);
		glCompileNamedShader(VertShaderName)

		std::string FragSourceContent = glf::loadFile(FRAG_SHADER_SOURCE[i]);
		char const * FragSourcePointer = FragSourceContent.c_str();
		glNamedShaderSource(FragShaderName, 1, &FragSourcePointer, NULL);
		glCompileNamedShader(FragShaderName)

		glNamedProgramParameteriGTC(ProgramName[i], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glNamedProgramShaderGTC(ProgramName[i], VertShaderName);
		glNamedProgramShaderGTC(ProgramName[i], FragShaderName);
		glDeleteNames(1, &VertShaderName);
		glDeleteNames(1, &FragShaderName);

		glLinkNamedProgram(ProgramName[i]);
		Validated = Validated && glf::checkProgram(ProgramName[i]);

		glProgramPipelineStagesGTC(PipelineName[i], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[i]);
	}

	return Validated && glf::checkError("initProgram");
}

bool initBuffer()
{
	glGenNames(buffer::MAX, GL_BUFFER_GTC, BufferName);
	glNamedBufferDataGTC(BufferName[buffer::VERTEX], VertexSize, VertexData, GL_STATIC_DRAW);
	glNamedBufferDataGTC(BufferName[buffer::TRANSFORM], sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
	glNamedBufferDataGTC(BufferName[buffer::BLIT], sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);

	return glf::checkError("initBuffer");;
}

bool initTexture()
{
	glGenNames(texture::MAX, GL_TEXTURE, TextureName);

	gli::texture2D Texture = gli::load(TEXTURE_DIFFUSE);

	glNamedTexStorageGTC(TextureName[texture::DIFFUSE], GL_TEXTURE_2D, GLsizei(1), GL_TRUE, GLint(Texture.levels()), GL_RGB8, GLsizei(Texture[0].dimensions().x), GLsizei(Texture[0].dimensions().y), GLsizei(1));
	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glNamedTexSubImageGTC(
			TextureName[texture::DIFFUSE],
			GLint(Level), 
			0, 0, 0,
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			GLsizei(Texture[Level].dimensions().z), 
			GL_RGB, 
			GL_UNSIGNED_BYTE, 
			Texture[Level].data());
	}
	if(Texture.levels() == 1)
		glGenerateNamedTexMipmapGTC(TextureName[texture::DIFFUSE]);

	glNamedTexStorageGTC(TextureName[texture::MULTISAMPLE], GL_TEXTURE_2D_MULTISAMPLE, GLsizei(4), GL_TRUE, GLint(Texture.levels()), GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, GLsizei(1));
	glNamedTexStorageGTC(TextureName[texture::DEPTH], GL_TEXTURE_2D_MULTISAMPLE, GLsizei(4), GL_TRUE, GLint(Texture.levels()), GL_DEPTH_COMPONENT24, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, GLsizei(1));

	return glf::checkError("initTexture");
}

bool initFramebuffer()
{
	bool Validated(true);

	glGenNames(1, GL_FRAMEBUFFER, &FramebufferName);
	glNamedFramebufferTextureGTC(FramebufferName, GL_DEPTH_ATTACHMENT, TextureName[texture::DEPTH], 0);
	glNamedFramebufferTextureGTC(FramebufferName, GL_COLOR_ATTACHMENT0, TextureName[texture::MULTISAMPLE], 0);
	Validated = Validated && (glCheckNamedFramebufferStatusGTC(FramebufferName, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	return glf::checkError("initFramebuffer");
}

bool initVertexArray()
{
	glGenNames(1, GL_VERTEX_ARRAY, &VertexArrayName);
	glNamedVertexArrayAttribGTC(VertexArrayName, glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), 0, 0);
	glNamedVertexArrayAttribGTC(VertexArrayName, glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), sizeof(glm::vec2), 0);
	glEnableNamedVertexArrayAttribGTC(VertexArrayName, glf::semantic::attr::POSITION);
	glEnableNamedVertexArrayAttribGTC(VertexArrayName, glf::semantic::attr::TEXCOORD);

	return glf::checkError("initVertexArray");
}

bool initDebugOutput()
{
	bool Validated(true);

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return Validated;
}

bool begin()
{
	bool Validated = true;
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
	Validated = Validated && glf::checkExtension("GL_GTC_direct_state_access");

	if(Validated)
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

	return Validated && glf::checkError("begin");
}

bool end()
{
	glDeleteNames(program::MAX, PipelineName);
	glDeleteNames(buffer::MAX, BufferName);
	glDeleteNames(1, &SamplerName);
	glDeleteNames(texture::MAX, TextureName);
	glDeleteNames(1, &FramebufferName);
	glDeleteNames(1, &VertexArrayName);
	glDeleteNames(program::MAX, ProgramName);

	return glf::checkError("end");
}

void renderFBO()
{
	{
		glm::mat4* Pointer = (glm::mat4*)glMapNamedBufferRangeGTC(
			BufferName[buffer::TRANSFORM], 0, sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

		glm::mat4 Perspective = glm::perspective(45.0f, float(FRAMEBUFFER_SIZE.x) / FRAMEBUFFER_SIZE.y, 0.1f, 100.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0 - 4.0));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 Model = glm::mat4(1.0f);
		glm::mat4 MVP = Perspective * View * Model;

		*Pointer = MVP;
		glUnmapNamedBufferGTC(BufferName[buffer::TRANSFORM]);
		//glNamedBufferSubDataGTC(BufferName[buffer::TRANSFORM], 0, sizeof(glm::mat4), &MVP[0][0]);
	}

	glViewportIndexedf(0, 0, 0, float(FRAMEBUFFER_SIZE.x), float(FRAMEBUFFER_SIZE.y));
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);

	glBindNamedImageGTC(GL_FRAMEBUFFER, glf::semantic::renderbuffer::COLOR0, TextureName[texture::MULTISAMPLE], 0);
	glBindNamedImageGTC(GL_FRAMEBUFFER, glf::semantic::renderbuffer::DEPTH, TextureName[texture::DEPTH], 0);
	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH, 0, &Depth);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	glBindNamedProgramPipelineGTC(PipelineName[program::THROUGH]);
	glBindNamedBufferGTC(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
	glBindNamedTextureGTC(GL_TEXTURE, 0, TextureName[texture::DIFFUSE]);
	glBindNamedSamplerGTC(GL_TEXTURE, 0, SamplerName);
	glBindNamedVertexArrayGTC(GL_VERTEX_INPUT, 0, VertexArrayName[vertexarray::VERTEX_INPUT]);
	glBindNamedVertexArrayGTC(GL_TRANSFORM_FEEDBACK, 0, VertexArrayName[vertexarray::TRANSFORM_FEEDBACK]);
	glBindNamedBufferGTC(GL_ARRAY_BUFFER, glf::semantic::attr::POSITION, BufferName[buffer::VERTEX]);
	glBindNamedBufferGTC(GL_ARRAY_BUFFER, glf::semantic::attr::TEXCOORD, BufferName[buffer::VERTEX]);

	glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 5, 0);

	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);

	glf::checkError("renderFBO");
}

void resolveMultisampling()
{
	{
		glm::mat4* Pointer = (glm::mat4*)glMapNamedBufferRangeGTC(
			BufferName[buffer::BLIT], 0, sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

		glm::mat4 Perspective = glm::perspective(45.0f, float(Window.Size.x) / Window.Size.y, 0.1f, 100.0f);
		glm::mat4 ViewFlip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f,-1.0f, 1.0f));
		glm::mat4 ViewTranslate = glm::translate(ViewFlip, glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
		glm::mat4 View = glm::rotate(ViewTranslate,-15.f, glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 Model = glm::mat4(1.0f);
		glm::mat4 MVP = Perspective * View * Model;

		*Pointer = MVP;
		glUnmapNamedBufferGTC(BufferName[buffer::BLIT]);
		//glNamedBufferSubDataEXT(BufferName[buffer::BLIT], 0, sizeof(glm::mat4), &MVP[0][0]);
	}

	glViewportIndexedf(0, 0, 0, float(Window.Size.x), float(Window.Size.y));
	glEnable(GL_SCISSOR_TEST);
	glBindNamedTextureLevelGTC(GL_FRAMEBUFFER, 0, 0, 0);

	glBindNamedBufferGTC(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::BLIT]);
	glBindNamedTextureGTC(GL_TEXTURE, 0, TextureName[texture::MULTISAMPLE]);
	glBindNamedSamplerGTC(0, SamplerName);
	glBindNamedVertexArrayGTC(VertexArrayName);

	// Box
	{
		glScissorIndexed(0, 1, 1, Window.Size.x  / 2 - 2, Window.Size.y - 2);
		glBindNamedProgramPipelineGTC(PipelineName[program::RESOLVE_BOX]);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 5, 0);
	}

	// Near
	{
		glScissorIndexed(0, Window.Size.x / 2 + 1, 1, Window.Size.x / 2 - 2, Window.Size.y - 2);
		glBindNamedProgramPipelineGTC(PipelineName[program::RESOLVE_NEAR]);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 5, 0);
	}

	//glBindImageBaseGTC(GL_READ_FRAMEBUFFER, glf::semantic::renderbuffer::COLOR0, TextureName[texture::MULTISAMPLE], 0);
	//glBindImageBaseGTC(GL_READ_FRAMEBUFFER, glf::semantic::renderbuffer::DEPTH, TextureName[texture::DEPTH], 0);
	//glBindImageBaseGTC(GL_DRAW_FRAMEBUFFER, glf::semantic::renderbuffer::COLOR0, 0, 0);
	//glBlitFramebuffer(
	//	0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 
	//	0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 
	//	GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glDisable(GL_SCISSOR_TEST);

	glf::checkError("renderFB");
}

void display()
{
	// Clear the framebuffer
	glBindNamedTextureLevelGTC(GL_FRAMEBUFFER, 0, 0, 0);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);

	// Pass 1: Render the scene in a multisampled framebuffer
	renderFBO();

	// Pass 2: Resolved and render the colorbuffer from the multisampled framebuffer
	resolveMultisampling();

	glf::checkError("display");
	glf::swapBuffers();
}

int main(int argc, char* argv[])
{
	if(glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		::SAMPLE_MAJOR_VERSION, 
		::SAMPLE_MINOR_VERSION))
		return 0;
	return 1;
}
