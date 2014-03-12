//**********************************
// OpenGL Samples Pack 
// ogl-samples.g-truc.net
//**********************************
// OpenGL Compute Program
// 12/08/2012 - 12/08/2012
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
	char const * SAMPLE_NAME("OpenGL Compute Program");
	char const * VS_SOURCE("gl-430/program-compute.vert");
	char const * FS_SOURCE("gl-430/program-compute.frag");
	char const * CS_SOURCE("gl-430/program-compute.comp");
	char const * TEXTURE_DIFFUSE("kueken1-bgr8.dds");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(8);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v4fv4fv4f);
	glf::vertex_v4fv4fv4f const VertexData[VertexCount] =
	{
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 1.0f, 0.5f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.5f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)),

	};

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0
	};

	namespace program
	{
		enum type
		{
			GRAPHICS,
			COMPUTE,
			MAX
		};
	}//namespace program

	namespace buffer
	{
		enum type
		{
			ELEMENT,
			INPUT,
			OUTPUT,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	namespace semantics
	{
		enum type
		{
			INPUT,
			OUTPUT
		};
	}//namespace semantics

	GLuint PipelineName[program::MAX] = {0, 0};
	GLuint ProgramName[program::MAX] = {0, 0};
	GLuint VertexArrayName(0);
	GLuint BufferName[buffer::MAX] = {0, 0, 0, 0};
	GLuint TextureName(0);
}//namespace

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(program::MAX, PipelineName);

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VS_SOURCE);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FS_SOURCE);
		GLuint ComputeShaderName = glf::createShader(GL_COMPUTE_SHADER, glf::DATA_DIRECTORY + CS_SOURCE);

		ProgramName[program::GRAPHICS] = glCreateProgram();
		glProgramParameteri(ProgramName[program::GRAPHICS], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[program::GRAPHICS], VertShaderName);
		glAttachShader(ProgramName[program::GRAPHICS], FragShaderName);
		glLinkProgram(ProgramName[program::GRAPHICS]);
		glDeleteShader(VertShaderName);
		Validated = Validated && glf::checkProgram(ProgramName[program::GRAPHICS]);

		ProgramName[program::COMPUTE] = glCreateProgram();
		glProgramParameteri(ProgramName[program::COMPUTE], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[program::COMPUTE], ComputeShaderName);
		glLinkProgram(ProgramName[program::COMPUTE]);
		glDeleteShader(ComputeShaderName);
		Validated = Validated && glf::checkProgram(ProgramName[program::COMPUTE]);
	}

	if(Validated)
	{
		glUseProgramStages(PipelineName[program::GRAPHICS], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[program::GRAPHICS]);
		glUseProgramStages(PipelineName[program::COMPUTE], GL_COMPUTE_SHADER_BIT, ProgramName[program::COMPUTE]);
	}
/*
	if(Validated)
	{
		GLint ActiveUniforms(0);
		glGetProgramInterfaceiv(ProgramName[program::GRAPHICS], GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES, &ActiveUniforms);

		for(GLint Index = 0; Index < ActiveUniforms; ++Index)
		{
			GLenum Props = GL_LOCATION;
			GLint Binding = -1;
			GLsizei Length = 0;
			std::vector<char> Name;
			Name.resize(64, '\0');

			glGetProgramResourceName(ProgramName[program::GRAPHICS], GL_PROGRAM_INPUT, Index, 64, &Length, &Name[0]);
			glGetProgramResourceiv(ProgramName[program::GRAPHICS], GL_PROGRAM_INPUT, Index, 1, &Props, sizeof(GLint), &Length, &Binding);

			continue;
			//assert(Binding == 0);
		}
	}

	if(Validated)
	{
		GLint ActiveUniforms(0);
		glGetProgramInterfaceiv(ProgramName[program::GRAPHICS], GL_UNIFORM, GL_ACTIVE_RESOURCES, &ActiveUniforms);

		for(GLint Index = 0; Index < ActiveUniforms; ++Index)
		{
			GLenum Props[2] = {GL_BUFFER_BINDING, GL_LOCATION};

			struct results
			{
				GLint Binding;
				GLint Location;
			};

			results Results = {0, 0};

			GLsizei NameLength = 0;
			GLsizei Length = 0;
			std::vector<char> Name;
			Name.resize(64, '\0');

			glGetProgramResourceName(ProgramName[program::GRAPHICS], GL_UNIFORM, Index, 64, &NameLength, &Name[0]);
			glGetProgramResourceiv(ProgramName[program::GRAPHICS], GL_UNIFORM, Index, 2, Props, sizeof(Props), &Length, (GLint*)&Results);

			continue;
			//assert(Binding == 0);
		}
	}
*/
	return Validated;
}

bool initBuffer()
{
	bool Validated(true);

	glGenBuffers(buffer::MAX, BufferName);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::INPUT]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::OUTPUT]);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLint UniformBufferOffset(0);

	glGetIntegerv(
		GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		&UniformBufferOffset);

	GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return Validated;
}

bool initTexture()
{
	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));
	assert(!Texture.empty());

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(1, &TextureName);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
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

	return true;
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
	glBindVertexArray(0);

	return true;
}

bool initDebugOutput()
{
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_UNIFORM_BLOCKS, "GL_MAX_COMPUTE_UNIFORM_BLOCKS");
	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, "GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS");
	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_IMAGE_UNIFORMS, "GL_MAX_COMPUTE_IMAGE_UNIFORMS");
	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, "GL_MAX_COMPUTE_SHARED_MEMORY_SIZE");
	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, "GL_MAX_COMPUTE_UNIFORM_COMPONENTS");
	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS, "GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS");
	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_ATOMIC_COUNTERS, "GL_MAX_COMPUTE_ATOMIC_COUNTERS");
	glf::logImplementationDependentLimit(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS, "GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS");
	glf::logImplementationDependentLimit(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, "GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS");

	//glf::logImplementationDependentLimit(GL_MAX_COMPUTE_WORK_GROUP_COUNT, "GL_MAX_COMPUTE_WORK_GROUP_COUNT");
	//glf::logImplementationDependentLimit(GL_MAX_COMPUTE_WORK_GROUP_SIZE, "GL_MAX_COMPUTE_WORK_GROUP_SIZE");

	return true;
}

bool begin()
{
	bool Validated(true);
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
	Validated = Validated && glf::checkExtension("GL_ARB_compute_shader");

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

	return Validated;
}

bool end()
{
	glDeleteProgramPipelines(program::MAX, PipelineName);
	glDeleteProgram(ProgramName[program::GRAPHICS]);
	glDeleteProgram(ProgramName[program::COMPUTE]);
	glDeleteBuffers(buffer::MAX, BufferName);
	glDeleteTextures(1, &TextureName);
	glDeleteVertexArrays(1, &VertexArrayName);

	return true;
}

void display()
{
	// Update of the uniform buffer
	{
		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(
			GL_UNIFORM_BUFFER, 0,	sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		glm::mat4 Projection = glm::perspectiveFov(45.f, 640.f, 480.f, 0.1f, 100.0f);
		//glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 Model = glm::mat4(1.0f);
		
		*Pointer = Projection * View * Model;

		// Make sure the uniform buffer is uploaded
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}

	glBindProgramPipeline(PipelineName[program::COMPUTE]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, semantics::INPUT, BufferName[buffer::INPUT]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, semantics::OUTPUT, BufferName[buffer::OUTPUT]);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
	glDispatchCompute(GLuint(VertexCount), 1, 1);

	glViewportIndexedf(0, 0, 0, GLfloat(Window.Size.x), GLfloat(Window.Size.y));
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f)[0]);

	glBindProgramPipeline(PipelineName[program::GRAPHICS]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName);
	glBindVertexArray(VertexArrayName);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, semantics::INPUT, BufferName[buffer::OUTPUT]);

	glDrawElementsInstancedBaseVertexBaseInstance(
		GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, GLF_BUFFER_OFFSET(0), 1, 0, 0);

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
