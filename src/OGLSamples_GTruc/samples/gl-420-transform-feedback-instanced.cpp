//**********************************
// OpenGL Samples Pack 
// ogl-samples.g-truc.net
//**********************************
// OpenGL Transform Feedback Instanced
// 01/07/2011 - 02/07/2011
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
	char const * SAMPLE_NAME("OpenGL Transform Feedback Instanced");
	char const * VERT_SHADER_SOURCE_TRANSFORM("gl-420/transform-stream.vert");
	char const * GEOM_SHADER_SOURCE_TRANSFORM("gl-420/transform-stream.geom");
	char const * VERT_SHADER_SOURCE_FEEDBACK("gl-420/feedback-stream.vert");
	char const * FRAG_SHADER_SOURCE_FEEDBACK("gl-420/feedback-stream.frag");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	GLsizei const VertexCount(4);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glm::vec4);
	glm::vec4 const VertexData[VertexCount] =
	{
		glm::vec4(-1.0f,-1.0f, 0.0f, 1.0f),
		glm::vec4( 1.0f,-1.0f, 0.0f, 1.0f),
		glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f)
	};

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLuint);
	GLuint const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0
	};

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	namespace pipeline
	{
		enum type
		{
			TRANSFORM,
			FEEDBACK,
			MAX
		};
	}//namespace pipeline

	GLuint PipelineName[pipeline::MAX] = {0, 0};
	GLuint ProgramName[pipeline::MAX] = {0, 0};
	GLuint VertexArrayName[pipeline::MAX] = {0, 0};

	GLuint FeedbackName(0);

	GLuint TransformArrayBufferName(0);
	GLuint TransformElementBufferName(0);
	GLint TransformUniformMVP(0);

	GLuint FeedbackArrayBufferName(0);
	GLint FeedbackUniformMVP(0);

	GLuint Query(0);

}//namespace

bool initProgram()
{
	bool Validated(true);
	
	glGenProgramPipelines(pipeline::MAX, PipelineName);

	glf::compiler Compiler;

	// Create program
	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER,
			glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_TRANSFORM, 
			"--version 420 --profile core");
		GLuint GeomShaderName = Compiler.create(GL_GEOMETRY_SHADER,
			glf::DATA_DIRECTORY + GEOM_SHADER_SOURCE_TRANSFORM,
			"--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[pipeline::TRANSFORM] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::TRANSFORM], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::TRANSFORM], VertShaderName);
		glAttachShader(ProgramName[pipeline::TRANSFORM], GeomShaderName);

		GLchar const * Strings[] = {"gl_Position", "block.Color"}; 
		glTransformFeedbackVaryings(ProgramName[pipeline::TRANSFORM], 2, Strings, GL_INTERLEAVED_ATTRIBS);
		glLinkProgram(ProgramName[pipeline::TRANSFORM]);

		Validated = Validated && glf::checkProgram(ProgramName[pipeline::TRANSFORM]);
	}

	if(Validated)
	{
		glUseProgramStages(
			PipelineName[pipeline::TRANSFORM], 
			GL_VERTEX_SHADER_BIT | GL_GEOMETRY_SHADER_BIT, 
			ProgramName[pipeline::TRANSFORM]);
	}

	// Get variables locations
	if(Validated)
	{
		TransformUniformMVP = glGetUniformLocation(ProgramName[pipeline::TRANSFORM], "MVP");
		Validated = Validated && (TransformUniformMVP >= 0);
	}

	// Create program
	if(Validated)
	{
		GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER,
			glf::DATA_DIRECTORY + VERT_SHADER_SOURCE_FEEDBACK, 
			"--version 420 --profile core");
		GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER,
			glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE_FEEDBACK,
			"--version 420 --profile core");
		Validated = Validated && Compiler.check();

		ProgramName[pipeline::FEEDBACK] = glCreateProgram();
		glProgramParameteri(ProgramName[pipeline::FEEDBACK], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glAttachShader(ProgramName[pipeline::FEEDBACK], VertShaderName);
		glAttachShader(ProgramName[pipeline::FEEDBACK], FragShaderName);
		glLinkProgram(ProgramName[pipeline::FEEDBACK]);
		Validated = Validated && glf::checkProgram(ProgramName[pipeline::FEEDBACK]);
	}

	if(Validated)
	{
		glUseProgramStages(
			PipelineName[pipeline::FEEDBACK], 
			GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, 
			ProgramName[pipeline::FEEDBACK]);
	}

	// Get variables locations
	if(Validated)
	{
		FeedbackUniformMVP = glGetUniformLocation(ProgramName[pipeline::FEEDBACK], "MVP");
		Validated = Validated && (FeedbackUniformMVP >= 0);
	}

	return Validated;
}

bool initVertexArray()
{
	// Build a vertex array objects
	glGenVertexArrays(pipeline::MAX, VertexArrayName);

	glBindVertexArray(VertexArrayName[pipeline::TRANSFORM]);
		glBindBuffer(GL_ARRAY_BUFFER, TransformArrayBufferName);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TransformElementBufferName);
	glBindVertexArray(0);

	glBindVertexArray(VertexArrayName[pipeline::FEEDBACK]);
		glBindBuffer(GL_ARRAY_BUFFER, FeedbackArrayBufferName);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 4, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v4fc4f), 0);
		glVertexAttribPointer(glf::semantic::attr::COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v4fc4f), GLF_BUFFER_OFFSET(sizeof(glm::vec4)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::COLOR);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return true;
}

bool initFeedback()
{
	// Generate a buffer object
	glGenTransformFeedbacks(1, &FeedbackName);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, FeedbackName);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, FeedbackArrayBufferName); 
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
		
	return true;
}

bool initBuffer()
{
	glGenBuffers(1, &TransformElementBufferName);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TransformElementBufferName);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenBuffers(1, &TransformArrayBufferName);
	glBindBuffer(GL_ARRAY_BUFFER, TransformArrayBufferName);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &FeedbackArrayBufferName);
	glBindBuffer(GL_ARRAY_BUFFER, FeedbackArrayBufferName);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glf::vertex_v4fc4f) * 6, NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}

bool initDebugOutput()
{
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	glDebugMessageCallbackARB(&glf::debugOutput, NULL);

	return true;
}

bool initTest()
{
	glEnable(GL_DEPTH_TEST);

	return true;
}

bool begin()
{
	bool Validated(true);
	Validated = Validated && glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	glGenQueries(1, &Query);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initTest();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();
	if(Validated)
		Validated = initFeedback();

	return Validated;
}

bool end()
{
	glDeleteProgramPipelines(pipeline::MAX, PipelineName);
	glDeleteVertexArrays(pipeline::MAX, VertexArrayName);

	glDeleteBuffers(1, &TransformArrayBufferName);
	glDeleteProgram(ProgramName[pipeline::TRANSFORM]);

	glDeleteBuffers(1, &FeedbackArrayBufferName);
	glDeleteProgram(ProgramName[pipeline::FEEDBACK]);

	glDeleteQueries(1, &Query);
	glDeleteTransformFeedbacks(1, &FeedbackName);

	return true;
}

void display()
{
	// Compute the MVP (Model View Projection matrix)
	glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
	glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	glProgramUniformMatrix4fv(ProgramName[pipeline::TRANSFORM], TransformUniformMVP, 1, GL_FALSE, &MVP[0][0]);
	glProgramUniformMatrix4fv(ProgramName[pipeline::FEEDBACK], FeedbackUniformMVP, 1, GL_FALSE, &MVP[0][0]);

	glViewportIndexedf(0, 0, 0, GLfloat(Window.Size.x), GLfloat(Window.Size.y));

	float Depth(1.0f);
	glClearBufferfv(GL_DEPTH, 0, &Depth);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)[0]);

	// First draw, capture the attributes
	// Disable rasterisation, vertices processing only!
	glEnable(GL_RASTERIZER_DISCARD);

	glBindProgramPipeline(PipelineName[pipeline::TRANSFORM]);
	glBindVertexArray(VertexArrayName[pipeline::TRANSFORM]);

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, FeedbackName);
	glBeginTransformFeedback(GL_TRIANGLES);
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_INT, NULL, 1, 0);
	glEndTransformFeedback();

	glDisable(GL_RASTERIZER_DISCARD);

	// Second draw, reuse the captured attributes
	glBindProgramPipeline(PipelineName[pipeline::FEEDBACK]);
	glBindVertexArray(VertexArrayName[pipeline::FEEDBACK]);

	glDrawTransformFeedbackStreamInstanced(GL_TRIANGLE_STRIP, FeedbackName, 0, 5);

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

