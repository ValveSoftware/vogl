//**********************************
// OpenGL Samples Pack 
// ogl-samples.g-truc.net
//**********************************
// OpenGL Image Store
// 21/11/2011 - 21/11/2011
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
	char const * SAMPLE_NAME("OpenGL Image Store");
	char const * VERT_SHADER_SOURCE_SAVE("gl-430/image-store-write.vert");
	char const * FRAG_SHADER_SOURCE_SAVE("gl-430/image-store-write.frag");
	char const * VERT_SHADER_SOURCE_READ("gl-430/image-store-read.vert");
	char const * FRAG_SHADER_SOURCE_READ("gl-430/image-store-read.frag");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	namespace pipeline
	{
		enum type
		{
			READ,
			SAVE,
			MAX
		};
	}//namespace pipeline

	glm::uvec2 ImageSize(0);

	GLuint VertexArrayName(0);
	GLuint PipelineName[pipeline::MAX] = {0, 0};
	GLuint ProgramName[pipeline::MAX] = {0, 0};
	GLuint TextureName(0);
}//namespace

bool initProgram()
{
	bool Validated(true);

	glf::compiler Compiler;

	glGenProgramPipelines(pipeline::MAX, PipelineName);

	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, 
			glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_READ, 
			"--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, 
			glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_READ,
			"--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[pipeline::READ] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::READ], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::READ], VertShaderName);
		glAttachShader(ProgramName[pipeline::READ], FragShaderName);
		glLinkProgram(ProgramName[pipeline::READ]);
		Validated = Validated && glf::checkProgram(ProgramName[pipeline::READ]);
	}

	if(Validated)
		glUseProgramStages(PipelineName[pipeline::READ], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[pipeline::READ]);

	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, 
			glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_SAVE, 
			"--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, 
			glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_SAVE,
			"--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[pipeline::SAVE] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::SAVE], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::SAVE], VertShaderName);
		glAttachShader(ProgramName[pipeline::SAVE], FragShaderName);
		glLinkProgram(ProgramName[pipeline::SAVE]);
		Validated = Validated && glf::checkProgram(ProgramName[pipeline::SAVE]);
	}

	if(Validated)
		glUseProgramStages(PipelineName[pipeline::SAVE], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[pipeline::SAVE]);

	return Validated;
}

bool initTexture()
{
	glGenTextures(1, &TextureName);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage2D(GL_TEXTURE_2D, GLint(1), GL_RGBA8, GLsizei(Window.Size.x), GLsizei(Window.Size.y));

	return true;
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
	glBindVertexArray(0);

	return true;
}

bool initDebugOutput()
{
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	glf::logImplementationDependentLimit(GL_MAX_TEXTURE_IMAGE_UNITS, "GL_MAX_TEXTURE_IMAGE_UNITS");

	return true;
}

bool begin()
{
	bool Validated(true);
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
	Validated = Validated && glf::checkExtension("GL_ARB_shader_image_size");

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initVertexArray();

	return Validated;
}

bool end()
{
	bool Validated(true);

	glDeleteTextures(1, &TextureName);
	glDeleteVertexArrays(1, &VertexArrayName);
	glDeleteProgram(ProgramName[pipeline::READ]);
	glDeleteProgram(ProgramName[pipeline::SAVE]);
	glDeleteProgramPipelines(pipeline::MAX, PipelineName);

	return Validated;
}

void display()
{
	glViewportIndexedf(0, 0, 0, float(Window.Size.x), float(Window.Size.y));
	glDrawBuffer(GL_BACK);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f, 0.5f, 1.0f, 1.0f)[0]);

	// Renderer to image
	{
		glDrawBuffer(GL_NONE);

		glBindProgramPipeline(PipelineName[pipeline::SAVE]);
		glBindImageTexture(glf::semantic::image::DIFFUSE, TextureName, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
		glBindVertexArray(VertexArrayName);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, 1, 0);
	}

	// Read from image
	{
		glDrawBuffer(GL_BACK);

		glBindProgramPipeline(PipelineName[pipeline::READ]);
		glBindImageTexture(glf::semantic::image::DIFFUSE, TextureName, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
		glBindVertexArray(VertexArrayName);
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, 1, 0);
	}

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
