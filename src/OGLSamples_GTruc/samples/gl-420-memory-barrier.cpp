//**********************************
// OpenGL Memory Barrier
// 11/03/2011 - 13/08/2011
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
	char const * SAMPLE_NAME = "OpenGL Memory barrier";
	char const * SHADER_VERT_SOURCE_UPDATE("gl-420/memory-barrier-update.vert");
	char const * SHADER_FRAG_SOURCE_UPDATE("gl-420/memory-barrier-update.frag");
	char const * SHADER_VERT_SOURCE_BLIT("gl-420/memory-barrier-blit.vert");
	char const * SHADER_FRAG_SOURCE_BLIT("gl-420/memory-barrier-blit.frag");
	char const * TEXTURE_DIFFUSE("kueken4-dxt1.dds");
	glm::ivec2 FRAMEBUFFER_SIZE(0);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(3);

	namespace program
	{
		enum type
		{
			UPDATE,
			BLIT,
			MAX
		};
	}//namespace program
	
	namespace pipeline
	{
		enum type
		{
			UPDATE,
			BLIT,
			MAX
		};
	}//namespace pipeline

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			COLORBUFFER,
			MAX
		};
	}//namespace texture

	GLuint VertexArrayName(0);
	GLuint TextureName[texture::MAX] = {0, 0};
	GLuint SamplerName(0);
	GLuint FramebufferName(0);

	GLuint ProgramName[program::MAX];
	GLuint PipelineName[pipeline::MAX];
}//namespace

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(pipeline::MAX, PipelineName);
	glBindProgramPipeline(PipelineName[pipeline::UPDATE]);
	glBindProgramPipeline(PipelineName[pipeline::BLIT]);
	glBindProgramPipeline(0);

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + SHADER_VERT_SOURCE_UPDATE);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + SHADER_FRAG_SOURCE_UPDATE);

		ProgramName[program::UPDATE] = glCreateProgram();
		glProgramParameteri(ProgramName[program::UPDATE], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[program::UPDATE], VertShaderName);
		glAttachShader(ProgramName[program::UPDATE], FragShaderName);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);
		glLinkProgram(ProgramName[program::UPDATE]);
		Validated = Validated && glf::checkProgram(ProgramName[program::UPDATE]);
	}

	if(Validated)
	{
		glUseProgramStages(PipelineName[pipeline::UPDATE], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[program::UPDATE]);
		Validated = Validated && glf::checkError("initProgram - stage");
	}

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + SHADER_VERT_SOURCE_BLIT);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + SHADER_FRAG_SOURCE_BLIT);

		ProgramName[program::BLIT] = glCreateProgram();
		glProgramParameteri(ProgramName[program::BLIT], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[program::BLIT], VertShaderName);
		glAttachShader(ProgramName[program::BLIT], FragShaderName);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);
		glLinkProgram(ProgramName[program::BLIT]);
		Validated = Validated && glf::checkProgram(ProgramName[program::BLIT]);
	}

	if(Validated)
	{
		glUseProgramStages(PipelineName[pipeline::BLIT], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[program::BLIT]);
		Validated = Validated && glf::checkError("initProgram - stage");
	}

	return Validated;
}

bool initSampler()
{
	bool Validated(true);

	glGenSamplers(1, &SamplerName);
	glSamplerParameteri(SamplerName, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(SamplerName, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(SamplerName, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return Validated;
}

bool initTexture2D()
{
	bool Validated(true);

	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));
	FRAMEBUFFER_SIZE = Texture.dimensions();

	glActiveTexture(GL_TEXTURE0);
	if(!TextureName[texture::DIFFUSE])
		glGenTextures(texture::MAX, TextureName);

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	glTexStorage2D(GL_TEXTURE_2D, GLint(Texture.levels()), GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 
		GLsizei(Texture.dimensions().x), GLsizei(Texture.dimensions().y));

	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glCompressedTexSubImage2D(
			GL_TEXTURE_2D,
			GLint(Level),
			0, 0,
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 
			GLsizei(Texture[Level].size()), 
			Texture[Level].data());
	}

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

	return Validated;
}

bool initFramebuffer()
{
	bool Validated(true);

	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::COLORBUFFER], 0);

	Validated = Validated && (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return Validated;
}

bool initVertexArray()
{
	bool Validated(true);

	glGenVertexArrays(1, &VertexArrayName);
    glBindVertexArray(VertexArrayName);
	glBindVertexArray(0);

	return Validated;
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
	bool Validated(true);
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initTexture2D();
	if(Validated)
		Validated = initSampler();
	if(Validated)
		Validated = initFramebuffer();

	return Validated;
}

bool end()
{
	bool Validated(true);

	glDeleteTextures(texture::MAX, TextureName);
	glDeleteFramebuffers(1, &FramebufferName);
	glDeleteProgram(ProgramName[program::BLIT]);
	glDeleteProgram(ProgramName[program::UPDATE]);
	glDeleteVertexArrays(1, &VertexArrayName);
	glDeleteSamplers(1, &SamplerName);
	glDeleteProgramPipelines(pipeline::MAX, PipelineName);

	return Validated;
}

void display()
{
	static int FrameIndex = 0;

	// Bind shared objects
	glViewportIndexedf(0, 0, 0, float(Window.Size.x), float(Window.Size.y));
	glBindSampler(0, SamplerName);
	glBindVertexArray(VertexArrayName);

	// Update a colorbuffer bound as a framebuffer attachement and as a texture
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);
	glBindProgramPipeline(PipelineName[pipeline::UPDATE]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, FrameIndex ? TextureName[texture::COLORBUFFER] : TextureName[texture::DIFFUSE]);

	glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 1, 0);

	// Blit to framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindProgramPipeline(PipelineName[pipeline::BLIT]);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);

	glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
	glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, VertexCount, 1, 0);
	glf::swapBuffers();

	FrameIndex = (FrameIndex + 1) % 256;
}

int main(int argc, char* argv[])
{
	return glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT, ::SAMPLE_MAJOR_VERSION, 
		::SAMPLE_MINOR_VERSION);
}
