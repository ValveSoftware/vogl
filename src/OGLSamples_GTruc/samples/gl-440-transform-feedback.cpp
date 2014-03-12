//**********************************
// OpenGL Transform Feedback Object
// 20/05/2010 - 11/08/2013
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
	char const * SAMPLE_NAME("OpenGL Transform Feedback Object");
	char const * VERT_SHADER_SOURCE_TRANSFORM("gl-440/transform-feedback-transform.vert");
	char const * VERT_SHADER_SOURCE_FEEDBACK("gl-440/transform-feedback-feedback.vert");
	char const * FRAG_SHADER_SOURCE_FEEDBACK("gl-440/transform-feedback-feedback.frag");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(4);

	GLsizei const VertexCount(6);
	GLsizeiptr const PositionSize = VertexCount * sizeof(glm::vec4);
	glm::vec4 const PositionData[VertexCount] =
	{
		glm::vec4(-1.0f,-1.0f, 0.0f, 1.0f),
		glm::vec4( 1.0f,-1.0f, 0.0f, 1.0f),
		glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(-1.0f,-1.0f, 0.0f, 1.0f)
	};

	namespace buffer
	{
		enum type
		{
			VERTEX,
			FEEDBACK,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	namespace program
	{
		enum type
		{
			FEEDBACK,
			TRANSFORM,
			MAX
		};
	}//namespace program

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	std::vector<GLuint> BufferName(buffer::MAX);
	GLuint FeedbackName(0);

	std::vector<GLuint> PipelineName(program::MAX);
	std::vector<GLuint> ProgramName(program::MAX);
	std::vector<GLuint> VertexArrayName(program::MAX);

	glm::mat4* UniformPointer(NULL);
}//namespace

bool initDebugOutput()
{
	bool Validated(true);

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return Validated;
}

bool initProgram()
{
	bool Validated = true;

	glf::compiler Compiler;
	GLuint VertTransformShaderName = Compiler.create(GL_VERTEX_SHADER, 
		glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_TRANSFORM, "--version 440 --profile core");
	GLuint VertFeedbackShaderName = Compiler.create(GL_VERTEX_SHADER, 
		glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_FEEDBACK, "--version 440 --profile core");
	GLuint FragFeedbackShaderName = Compiler.create(GL_FRAGMENT_SHADER, 
		glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_FEEDBACK, "--version 440 --profile core");

	Validated = Validated && Compiler.check();

	// Create program
	if(Validated)
	{
		ProgramName[program::TRANSFORM] = glCreateProgram();
		glProgramParameteri(ProgramName[program::TRANSFORM], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[program::TRANSFORM], VertTransformShaderName);
		glDeleteShader(VertTransformShaderName);
		glLinkProgram(ProgramName[program::TRANSFORM]);

		Validated = Validated && glf::checkProgram(ProgramName[program::TRANSFORM]);
	}

	// Create program
	if(Validated)
	{
		ProgramName[program::FEEDBACK] = glCreateProgram();
		glProgramParameteri(ProgramName[program::FEEDBACK], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[program::FEEDBACK], VertFeedbackShaderName);
		glAttachShader(ProgramName[program::FEEDBACK], FragFeedbackShaderName);
		glDeleteShader(VertFeedbackShaderName);
		glDeleteShader(FragFeedbackShaderName);
		glLinkProgram(ProgramName[program::FEEDBACK]);
		Validated = Validated && glf::checkProgram(ProgramName[program::FEEDBACK]);
	}

	glGenProgramPipelines(program::MAX, &PipelineName[0]);

	if(Validated)
	{
		glUseProgramStages(PipelineName[program::TRANSFORM], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[program::TRANSFORM]);
		glUseProgramStages(PipelineName[program::FEEDBACK], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[program::FEEDBACK]);
	}

	return Validated && glf::checkError("initProgram");
}

bool initVertexArray()
{
	// Build a vertex array object
	glGenVertexArrays(program::MAX, &VertexArrayName[0]);

	glBindVertexArray(VertexArrayName[program::TRANSFORM]);
		glVertexAttribFormat(glf::semantic::attr::POSITION, 4, GL_FLOAT, GL_FALSE, 0);
		glVertexAttribBinding(glf::semantic::attr::POSITION, glf::semantic::buffer::STATIC);
		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
	glBindVertexArray(0);

	glBindVertexArray(VertexArrayName[program::FEEDBACK]);
		glVertexAttribFormat(glf::semantic::attr::POSITION, 4, GL_FLOAT, GL_FALSE, 0);
		glVertexAttribBinding(glf::semantic::attr::POSITION, glf::semantic::buffer::STATIC);
		glEnableVertexAttribArray(glf::semantic::attr::POSITION);

		glVertexAttribFormat(glf::semantic::attr::COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
		glVertexAttribBinding(glf::semantic::attr::COLOR, glf::semantic::buffer::STATIC);
		glEnableVertexAttribArray(glf::semantic::attr::COLOR);
	glBindVertexArray(0);

	return glf::checkError("initVertexArray");
}

bool initFeedback()
{
	// Generate a buffer object
	glGenTransformFeedbacks(1, &FeedbackName);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, FeedbackName);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, BufferName[buffer::FEEDBACK]); 
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	return glf::checkError("initFeedback");
}

bool initBuffer()
{
	// Generate a buffer object
	glGenBuffers(buffer::MAX, &BufferName[0]);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::VERTEX]);
	glBufferStorage(GL_ARRAY_BUFFER, PositionSize, PositionData, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::FEEDBACK]);
	glBufferStorage(GL_ARRAY_BUFFER, sizeof(glf::vertex_v4fc4f) * VertexCount, NULL, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLint UniformBufferOffset(0);

	glGetIntegerv(
		GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		&UniformBufferOffset);

	GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	glBufferStorage(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return glf::checkError("initArrayBuffer");
}

bool begin()
{
	bool Validated = glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated)
		Validated = initDebugOutput();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initFeedback();

	glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
	UniformPointer = (glm::mat4*)glMapBufferRange(
		GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4),
		GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

	return Validated && glf::checkError("begin");
}

bool end()
{
	glf::checkError("end 0");

	glDeleteVertexArrays(program::MAX, &VertexArrayName[0]);
	glDeleteBuffers(buffer::MAX, &BufferName[0]);
	glDeleteProgram(ProgramName[program::FEEDBACK]);
	glDeleteProgram(ProgramName[program::TRANSFORM]);
	glDeleteProgramPipelines(program::MAX, &PipelineName[0]);

	glDeleteTransformFeedbacks(1, &FeedbackName);

	return glf::checkError("end");
}

void display()
{
	{
		// Compute the MVP (Model View Projection matrix)
		float Aspect = (Window.Size.x * 0.50f) / (Window.Size.y * 0.50f);
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

	// Set the display viewport
	glViewportIndexedf(0, 0.0f, 0.0f, float(Window.Size.x), float(Window.Size.y));

	// Clear color buffer
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)[0]);

	// First draw, capture the attributes
	// Disable rasterisation, vertices processing only!
	glEnable(GL_RASTERIZER_DISCARD);

	glBindProgramPipeline(PipelineName[program::TRANSFORM]);
	glBindBufferBase(GL_UNIFORM_BUFFER, glf::semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
	glBindVertexArray(VertexArrayName[program::TRANSFORM]);
	glBindVertexBuffer(glf::semantic::buffer::STATIC, BufferName[buffer::VERTEX], 0, sizeof(glm::vec4));

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, FeedbackName);
	glBeginTransformFeedback(GL_TRIANGLES);
		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);
	glEndTransformFeedback();
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	glDisable(GL_RASTERIZER_DISCARD);

	// Second draw, reuse the captured attributes
	glBindProgramPipeline(PipelineName[program::FEEDBACK]);
	glBindVertexArray(VertexArrayName[program::FEEDBACK]);
	glBindVertexBuffer(glf::semantic::buffer::STATIC, BufferName[buffer::FEEDBACK], 0, sizeof(glf::vertex_v4fc4f));
	
	glDrawTransformFeedback(GL_TRIANGLES, FeedbackName);

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
