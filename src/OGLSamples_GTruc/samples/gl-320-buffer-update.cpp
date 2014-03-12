//**********************************
// OpenGL Buffer Update
// 11/08/2009 - 23/02/2013
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
	char const * SAMPLE_NAME("OpenGL Buffer Update");
	char const * VERT_SHADER_SOURCE("gl-320/buffer-update.vert");
	char const * FRAG_SHADER_SOURCE("gl-320/buffer-update.frag");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(3);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(6);
	GLsizeiptr const PositionSize = VertexCount * sizeof(glm::vec2);
	glm::vec2 const PositionData[VertexCount] =
	{
		glm::vec2(-1.0f,-1.0f),
		glm::vec2( 1.0f,-1.0f),
		glm::vec2( 1.0f, 1.0f),
		glm::vec2( 1.0f, 1.0f),
		glm::vec2(-1.0f, 1.0f),
		glm::vec2(-1.0f,-1.0f)
	};

	namespace buffer
	{
		enum type
		{
			ARRAY,
			COPY,
			MATERIAL,
			TRANSFORM,
			MAX
		};
	}//namespace program

	GLuint VertexArrayName(0);
	GLuint ProgramName(0);
	std::vector<GLuint> BufferName(buffer::MAX);
	GLint UniformTransform(0);
	GLint UniformMaterial(0);
}//namespace

bool initDebugOutput()
{
#	ifdef GL_ARB_debug_output
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		glDebugMessageCallbackARB(&glf::debugOutput, NULL);
#	endif

	return true;
}

bool initProgram()
{
	bool Validated = true;
	
	glf::compiler Compiler;

	// Create program
	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE, "--version 150 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE, "--version 150 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName = glCreateProgram();
		glAttachShader(ProgramName, VertShaderName);
		glAttachShader(ProgramName, FragShaderName);
		glBindAttribLocation(ProgramName, glf::semantic::attr::POSITION, "Position");
		glBindFragDataLocation(ProgramName, glf::semantic::frag::COLOR, "Color");
		glLinkProgram(ProgramName);
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);
		Validated = Validated && glf::checkProgram(ProgramName);
	}

	// Get variables locations
	if(Validated)
	{
		UniformMaterial = glGetUniformBlockIndex(ProgramName, "material");
		UniformTransform = glGetUniformBlockIndex(ProgramName, "transform");
	}

	return Validated && glf::checkError("initProgram");
}
/*
// Buffer update using glBufferSubData
bool initArrayBuffer()
{
	// Generate a buffer object
	glGenBuffers(1, &BufferName);

	// Bind the buffer for use
	glBindBuffer(GL_ARRAY_BUFFER, BufferName);

	// Reserve buffer memory but don't copy the values
	glBufferData(GL_ARRAY_BUFFER, PositionSize, NULL, GL_STATIC_DRAW);

	// Copy the vertex data in the buffer, in this sample for the whole range of data.
	glBufferSubData(GL_ARRAY_BUFFER, 0, PositionSize, &PositionData[0][0]);

	// Unbind the buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return glf::checkError("initArrayBuffer");
}
*/
/*
// Buffer update using glMapBuffer
bool initArrayBuffer()
{
	// Generate a buffer object
	glGenBuffers(1, &BufferName);

	// Bind the buffer for use
	glBindBuffer(GL_ARRAY_BUFFER, BufferName);

	// Reserve buffer memory but don't copy the values
	glBufferData(GL_ARRAY_BUFFER, PositionSize, 0, GL_STATIC_DRAW);

	// Copy the vertex data in the buffer, in this sample for the whole range of data.
	GLvoid* Data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	memcpy(Data, PositionData, PositionSize);
	glUnmapBuffer(GL_ARRAY_BUFFER);

	// Unbind the buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return glf::checkError("initArrayBuffer");
}
*/

// Buffer update using glMapBufferRange
bool initBuffer()
{
	// Generate a buffer object
	glGenBuffers(buffer::MAX, &BufferName[0]);

	// Bind the buffer for use
	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::ARRAY]);

	// Reserve buffer memory but don't copy the values
	glBufferData(
		GL_ARRAY_BUFFER, 
		PositionSize, 
		0, 
		GL_STATIC_DRAW);

	// Copy the vertex data in the buffer, in this sample for the whole range of data.
	// It doesn't required to be the buffer size but pointers require no memory overlapping.
	GLvoid* Data = glMapBufferRange(
		GL_ARRAY_BUFFER, 
		0,				// Offset
		PositionSize,	// Size,
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
	memcpy(Data, PositionData, PositionSize);

	// Explicitly send the data to the graphic card.
	glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, PositionSize);

	glUnmapBuffer(GL_ARRAY_BUFFER);

	// Unbind the buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Copy buffer
	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::COPY]);
	glBufferData(GL_ARRAY_BUFFER, PositionSize, 0, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_COPY_READ_BUFFER, BufferName[buffer::ARRAY]);
	glBindBuffer(GL_COPY_WRITE_BUFFER, BufferName[buffer::COPY]);

	glCopyBufferSubData(
		GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
		0, 0,
		PositionSize);

	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

	GLint UniformBlockSize = 0;

	{
		glGetActiveUniformBlockiv(
			ProgramName, 
			UniformTransform,
			GL_UNIFORM_BLOCK_DATA_SIZE,
			&UniformBlockSize);

		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, 0, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	{
		glm::vec4 Diffuse(1.0f, 0.5f, 0.0f, 1.0f);

		glGetActiveUniformBlockiv(
			ProgramName, 
			UniformMaterial,
			GL_UNIFORM_BLOCK_DATA_SIZE,
			&UniformBlockSize);

		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::MATERIAL]);
		glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, &Diffuse[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	return glf::checkError("initBuffer");
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::COPY]);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), GLF_BUFFER_OFFSET(0));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
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
		Validated = initProgram();
	glf::checkError("initProgram Apple workaround");
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();

	return Validated && glf::checkError("begin");
}

bool end()
{
	// Delete objects
	glDeleteBuffers(buffer::MAX, &BufferName[0]);
	glDeleteProgram(ProgramName);
	glDeleteVertexArrays(1, &VertexArrayName);

	return glf::checkError("end");
}

void display()
{
	// Update of the uniform buffer
	{
		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(
			GL_UNIFORM_BUFFER, 0,	sizeof(glm::mat4),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
		glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
		glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 Model = glm::mat4(1.0f);
		glm::mat4 MVP = Projection * View * Model;
		
		*Pointer = MVP;

		// Make sure the uniform buffer is uploaded
		glUnmapBuffer(GL_UNIFORM_BUFFER);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glViewport(0, 0, Window.Size.x, Window.Size.y);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)[0]);

	glUseProgram(ProgramName);
	glUniformBlockBinding(ProgramName, UniformTransform, glf::semantic::uniform::TRANSFORM0);
	glUniformBlockBinding(ProgramName, UniformMaterial, glf::semantic::uniform::MATERIAL);

	// Attach the buffer to UBO binding point glf::semantic::uniform::TRANSFORM0
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
	// Attach the buffer to UBO binding point glf::semantic::uniform::MATERIAL
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::MATERIAL, BufferName[buffer::MATERIAL]);

	glBindVertexArray(VertexArrayName);
	glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);

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
