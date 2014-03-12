//**********************************
// OpenGL Primitive smooth shading
// 26/08/2009 - 19/10/2010
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
	char const * SAMPLE_NAME = "OpenGL Primitive smooth shading";	
	std::string const SAMPLE_VERT_SHADER1("gl-400/tess.vert");
	std::string const SAMPLE_CONT_SHADER1("gl-400/tess.cont");
	std::string const SAMPLE_EVAL_SHADER1("gl-400/tess.eval");
	std::string const SAMPLE_GEOM_SHADER1("gl-400/tess.geom");
	std::string const SAMPLE_FRAG_SHADER1("gl-400/tess.frag");
	std::string const SAMPLE_VERT_SHADER2("gl-400/smooth-shading-geom.vert");
	std::string const SAMPLE_GEOM_SHADER2("gl-400/smooth-shading-geom.geom");
	std::string const SAMPLE_FRAG_SHADER2("gl-400/smooth-shading-geom.frag");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(0);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount = 4;
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fc4ub);
	glf::vertex_v2fc4ub const VertexData[VertexCount] =
	{
		glf::vertex_v2fc4ub(glm::vec2(-1.0f,-1.0f), glm::u8vec4(255,   0,   0, 255)),
		glf::vertex_v2fc4ub(glm::vec2( 1.0f,-1.0f), glm::u8vec4(255, 255, 255, 255)),
		glf::vertex_v2fc4ub(glm::vec2( 1.0f, 1.0f), glm::u8vec4(  0, 255,   0, 255)),
		glf::vertex_v2fc4ub(glm::vec2(-1.0f, 1.0f), glm::u8vec4(  0,   0, 255, 255))
		//glf::vertex_v2fc4ub(glm::vec2(-1.0f,-1.0f), glm::u8vec4(255, 148,  17, 255)),
		//glf::vertex_v2fc4ub(glm::vec2( 1.0f,-1.0f), glm::u8vec4( 17, 255,  58, 255)),
		//glf::vertex_v2fc4ub(glm::vec2( 1.0f, 1.0f), glm::u8vec4( 17, 219, 255, 255)),
		//glf::vertex_v2fc4ub(glm::vec2(-1.0f, 1.0f), glm::u8vec4(255,  17, 232, 255))
		//glf::vertex_v2fc4ub(glm::vec2(-1.0f,-1.0f), glm::u8vec4(242, 136,   0, 255)),
		//glf::vertex_v2fc4ub(glm::vec2( 1.0f,-1.0f), glm::u8vec4(  0, 202,  36, 255)),
		//glf::vertex_v2fc4ub(glm::vec2( 1.0f, 1.0f), glm::u8vec4(  0, 147, 175, 255)),
		//glf::vertex_v2fc4ub(glm::vec2(-1.0f, 1.0f), glm::u8vec4(185,   0, 166, 255))
	};

	GLsizei const ElementCount = 6;
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0
	};

	GLuint ProgramName[2] = {0, 0};
	GLuint ElementBufferName = 0;
	GLuint ArrayBufferName = 0;
	GLuint VertexArrayName = 0;
	GLint UniformMVP[2] = {0, 0};
	GLint UniformDiffuse[2] = {0, 0};

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
	
	// Create program
	if(Validated)
	{
		GLuint VertShader = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + SAMPLE_VERT_SHADER1);
		GLuint ContShader = glf::createShader(GL_TESS_CONTROL_SHADER, glf::DATA_DIRECTORY + SAMPLE_CONT_SHADER1);
		GLuint EvalShader = glf::createShader(GL_TESS_EVALUATION_SHADER, glf::DATA_DIRECTORY + SAMPLE_EVAL_SHADER1);
		GLuint GeomShader = glf::createShader(GL_GEOMETRY_SHADER, glf::DATA_DIRECTORY + SAMPLE_GEOM_SHADER1);
		GLuint FragShader = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + SAMPLE_FRAG_SHADER1);

		Validated = Validated && glf::checkShader(VertShader, SAMPLE_VERT_SHADER1);
		Validated = Validated && glf::checkShader(ContShader, SAMPLE_CONT_SHADER1);
		Validated = Validated && glf::checkShader(EvalShader, SAMPLE_EVAL_SHADER1);
		Validated = Validated && glf::checkShader(GeomShader, SAMPLE_GEOM_SHADER1);
		Validated = Validated && glf::checkShader(FragShader, SAMPLE_FRAG_SHADER1);

		ProgramName[0] = glCreateProgram();
		glAttachShader(ProgramName[0], VertShader);
		glAttachShader(ProgramName[0], GeomShader);
		glAttachShader(ProgramName[0], ContShader);
		glAttachShader(ProgramName[0], EvalShader);
		glAttachShader(ProgramName[0], FragShader);
		glDeleteShader(VertShader);
		glDeleteShader(GeomShader);
		glDeleteShader(ContShader);
		glDeleteShader(EvalShader);
		glDeleteShader(FragShader);

		glLinkProgram(ProgramName[0]);
		Validated = Validated && glf::checkProgram(ProgramName[0]);
	}

	// Get variables locations
	if(Validated)
	{
		UniformMVP[0] = glGetUniformLocation(ProgramName[0], "MVP");
	}

	// Create program
	if(Validated)
	{
		GLuint VertShader = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + SAMPLE_VERT_SHADER2);
		GLuint GeomShader = glf::createShader(GL_GEOMETRY_SHADER, glf::DATA_DIRECTORY + SAMPLE_GEOM_SHADER2);
		GLuint FragShader = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + SAMPLE_FRAG_SHADER2);

		Validated = Validated && glf::checkShader(VertShader, SAMPLE_VERT_SHADER2);
		Validated = Validated && glf::checkShader(GeomShader, SAMPLE_GEOM_SHADER2);
		Validated = Validated && glf::checkShader(FragShader, SAMPLE_FRAG_SHADER2);

		ProgramName[1] = glCreateProgram();
		glAttachShader(ProgramName[1], VertShader);
		glAttachShader(ProgramName[1], GeomShader);
		glAttachShader(ProgramName[1], FragShader);
		glDeleteShader(VertShader);
		glDeleteShader(GeomShader);
		glDeleteShader(FragShader);

		glLinkProgram(ProgramName[1]);
		Validated = Validated && glf::checkProgram(ProgramName[1]);
	}

	// Get variables locations
	if(Validated)
	{
		UniformMVP[1] = glGetUniformLocation(ProgramName[1], "MVP");
	}

	return Validated && glf::checkError("initProgram");
}

bool initVertexArray()
{
	// Build a vertex array object
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, ArrayBufferName);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fc4ub), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glf::vertex_v2fc4ub), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::COLOR);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return glf::checkError("initVertexArray");
}

bool initBuffer()
{
	// Generate a buffer object
	glGenBuffers(1, &ElementBufferName);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ElementBufferName);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenBuffers(1, &ArrayBufferName);
	glBindBuffer(GL_ARRAY_BUFFER, ArrayBufferName);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return glf::checkError("initBuffer");
}

bool begin()
{
	bool Validated = glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();

	return Validated && glf::checkError("begin");
}

bool end()
{
	glDeleteVertexArrays(1, &VertexArrayName);
	glDeleteBuffers(1, &ArrayBufferName);
	glDeleteBuffers(1, &ElementBufferName);
	glDeleteProgram(ProgramName[0]);
	glDeleteProgram(ProgramName[1]);

	return glf::checkError("end");
}

void display()
{
	glm::mat4 Projection = glm::perspective(45.0f, float(Window.Size.x / 2) / float(Window.Size.y), 0.1f, 100.0f);
	glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;
	
	glViewport(0, 0, Window.Size.x, Window.Size.y);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)[0]);

	glBindVertexArray(VertexArrayName);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ElementBufferName);

	glViewport(0, 0, Window.Size.x / 2, Window.Size.y);
	glUseProgram(ProgramName[0]);
	glUniformMatrix4fv(UniformMVP[0], 1, GL_FALSE, &MVP[0][0]);

	glPatchParameteri(GL_PATCH_VERTICES, VertexCount);
	glDrawArraysInstanced(GL_PATCHES, 0, VertexCount, 1);
	
	glViewport(Window.Size.x / 2, 0, Window.Size.x / 2, Window.Size.y);
	glUseProgram(ProgramName[1]);
	glUniformMatrix4fv(UniformMVP[1], 1, GL_FALSE, &MVP[0][0]);

	glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, NULL, 1, 0);
	
	glf::checkError("display");
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
