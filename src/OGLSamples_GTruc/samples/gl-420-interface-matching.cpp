//**********************************
// OpenGL Shader Interface Matching
// 25/07/2011 - 25/07/2011
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
	char const * SAMPLE_NAME("OpenGL Shader Interface Matching");	
	char const * SAMPLE_VERT_SHADER("gl-420/interface-matching.vert");
	char const * SAMPLE_CONT_SHADER("gl-420/interface-matching.cont");
	char const * SAMPLE_EVAL_SHADER("gl-420/interface-matching.eval");
	char const * SAMPLE_GEOM_SHADER("gl-420/interface-matching.geom");
	char const * SAMPLE_FRAG_SHADER("gl-420/interface-matching.frag");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(4);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	GLsizei const VertexCount(4);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fc4f);
	glf::vertex_v2fc4f const VertexData[VertexCount] =
	{
		glf::vertex_v2fc4f(glm::vec2(-1.0f,-1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)),
		glf::vertex_v2fc4f(glm::vec2( 1.0f,-1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v2fc4f(glm::vec2( 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v2fc4f(glm::vec2(-1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f))
	};

	namespace program
	{
		enum type
		{
			VERT,
			FRAG,
			MAX
		};
	}//namespace program

	GLuint PipelineName(0);
	GLuint ProgramName[program::MAX] = {0, 0};
	GLuint ArrayBufferName(0);
	GLuint VertexArrayName(0);
	GLint UniformMVP(0);
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
	
	glGenProgramPipelines(1, &PipelineName);

	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + SAMPLE_VERT_SHADER);
		GLuint ContShaderName = glf::createShader(GL_TESS_CONTROL_SHADER, glf::DATA_DIRECTORY + SAMPLE_CONT_SHADER);
		GLuint EvalShaderName = glf::createShader(GL_TESS_EVALUATION_SHADER, glf::DATA_DIRECTORY + SAMPLE_EVAL_SHADER);
		GLuint GeomShaderName = glf::createShader(GL_GEOMETRY_SHADER, glf::DATA_DIRECTORY + SAMPLE_GEOM_SHADER);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + SAMPLE_FRAG_SHADER);

		ProgramName[program::VERT] = glCreateProgram();
		ProgramName[program::FRAG] = glCreateProgram();
		glProgramParameteri(ProgramName[program::VERT], GL_PROGRAM_SEPARABLE, GL_TRUE);
		glProgramParameteri(ProgramName[program::FRAG], GL_PROGRAM_SEPARABLE, GL_TRUE);

		glAttachShader(ProgramName[program::VERT], VertShaderName);
		glAttachShader(ProgramName[program::VERT], ContShaderName);
		glAttachShader(ProgramName[program::VERT], EvalShaderName);
		glAttachShader(ProgramName[program::VERT], GeomShaderName);
		glLinkProgram(ProgramName[program::VERT]);

		glAttachShader(ProgramName[program::FRAG], FragShaderName);
		glLinkProgram(ProgramName[program::FRAG]);

		glDeleteShader(VertShaderName);
		glDeleteShader(ContShaderName);
		glDeleteShader(EvalShaderName);
		glDeleteShader(GeomShaderName);
		glDeleteShader(FragShaderName);

		Validated = Validated && glf::checkProgram(ProgramName[program::VERT]);
		Validated = Validated && glf::checkProgram(ProgramName[program::FRAG]);
	}

	if(Validated)
	{
		glUseProgramStages(PipelineName, GL_VERTEX_SHADER_BIT | GL_TESS_CONTROL_SHADER_BIT | GL_TESS_EVALUATION_SHADER_BIT | GL_GEOMETRY_SHADER_BIT, ProgramName[program::VERT]);
		glUseProgramStages(PipelineName, GL_FRAGMENT_SHADER_BIT, ProgramName[program::FRAG]);
	}

	if(Validated)
	{
		UniformMVP = glGetUniformLocation(ProgramName[program::VERT], "MVP");
	}

	return Validated;
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, ArrayBufferName);
		glVertexAttribPointer(glf::semantic::attr::POSITION + 0, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fc4f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::POSITION + 1, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fc4f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fc4f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION + 0);
		glEnableVertexAttribArray(glf::semantic::attr::POSITION + 1);
		glEnableVertexAttribArray(glf::semantic::attr::COLOR);
	glBindVertexArray(0);

	std::vector<glf::vertexattrib> Valid(16); 
	Valid[glf::semantic::attr::POSITION + 0] = glf::vertexattrib(GL_TRUE, 0, 2, sizeof(glf::vertex_v2fc4f), GL_FLOAT, GL_FALSE, GL_FALSE, GL_FALSE, 0, NULL);
	Valid[glf::semantic::attr::POSITION + 1] = glf::vertexattrib(GL_TRUE, 0, 2, sizeof(glf::vertex_v2fc4f), GL_FLOAT, GL_FALSE, GL_FALSE, GL_FALSE, 0, NULL);
	Valid[glf::semantic::attr::COLOR] = glf::vertexattrib(GL_TRUE, 0, 4, sizeof(glf::vertex_v2fc4f), GL_FLOAT, GL_FALSE, GL_FALSE, GL_FALSE, 0, GLF_BUFFER_OFFSET(sizeof(glm::vec2)));

	glf::validateVAO42(VertexArrayName, Valid);

	return true;
}

bool initBuffer()
{
	// Generate a buffer object
	glGenBuffers(1, &ArrayBufferName);
	glBindBuffer(GL_ARRAY_BUFFER, ArrayBufferName);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}

bool initMax()
{
	GLint MaxVertexAttribs(0);
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &MaxVertexAttribs);
	//GL_MAX_DRAW_BUFFERS 8
	//GL_MAX_COLOR_ATTACHMENTS 8
	GLint MaxVertexOutput(0);
	glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &MaxVertexOutput);
	GLint MaxControlInput(0);
	glGetIntegerv(GL_MAX_TESS_CONTROL_INPUT_COMPONENTS, &MaxControlInput);
	GLint MaxControlOutput(0);
	glGetIntegerv(GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS, &MaxControlOutput);
	GLint MaxControlTotalOutput(0);
	glGetIntegerv(GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS, &MaxControlTotalOutput);
	GLint MaxEvaluationInput(0);
	glGetIntegerv(GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS, &MaxEvaluationInput);
	GLint MaxEvaluationOutput(0);
	glGetIntegerv(GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS, &MaxEvaluationOutput);
	GLint MaxGeometryInput(0);
	glGetIntegerv(GL_MAX_GEOMETRY_INPUT_COMPONENTS, &MaxGeometryInput);	
	GLint MaxGeometryOutput(0);
	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS, &MaxGeometryOutput);	
	GLint MaxFragmentInput(0);
	glGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS, &MaxFragmentInput);

	return true;
}

bool begin()
{
	bool Validated = glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();

	if(Validated)
		Validated = initMax();;
	if(Validated)
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initVertexArray();

	return Validated;
}

bool end()
{
	glDeleteVertexArrays(1, &VertexArrayName);
	glDeleteBuffers(1, &ArrayBufferName);
	for(std::size_t i = 0; i < program::MAX; ++i)
		glDeleteProgram(ProgramName[i]);

	return true;
}

bool validate(GLuint const & ProgramName)
{
	bool Error = false;

	// Pipeline object validation
	{
		GLint Status(0);
		GLint LengthMax(0);
		glValidateProgramPipeline(PipelineName);
		glGetProgramPipelineiv(PipelineName, GL_VALIDATE_STATUS, &Status);
		glGetProgramPipelineiv(PipelineName, GL_INFO_LOG_LENGTH, &LengthMax);

		GLsizei LengthQuery(0);
		std::vector<GLchar> InfoLog(LengthMax + 1, '\0');
		glGetProgramPipelineInfoLog(PipelineName, GLsizei(InfoLog.size()), &LengthQuery, &InfoLog[0]);

		glDebugMessageInsertARB(
			GL_DEBUG_SOURCE_APPLICATION_ARB, 
			GL_DEBUG_TYPE_OTHER_ARB, 76,
			GL_DEBUG_SEVERITY_LOW_ARB,
			LengthQuery, 
			&InfoLog[0]);
	}

	GLint ActiveAttributeMaxLength(0);
	GLint ActiveAttribute(0);
	glGetProgramiv(ProgramName, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &ActiveAttributeMaxLength);
	glGetProgramiv(ProgramName, GL_ACTIVE_ATTRIBUTES, &ActiveAttribute);

	GLsizei AttribLength(0);
	GLint AttribSize(0);
	GLenum AttribType(0);
	std::vector<GLchar> AttribName(ActiveAttributeMaxLength, '\0');

	for(GLint i = 0; i < ActiveAttribute; ++i)
	{
		glGetActiveAttrib(ProgramName,
 			GLuint(i),
 			GLsizei(ActiveAttributeMaxLength),
 			&AttribLength,
 			&AttribSize,
 			&AttribType,
 			&AttribName[0]);

		std::string NameString;
		NameString.insert(NameString.begin(), AttribName.begin(), AttribName.end());
		std::vector<GLchar> NameSwap(ActiveAttributeMaxLength, '\0');
		std::swap(AttribName, NameSwap);

		GLint AttribLocation = glGetAttribLocation(ProgramName, NameString.c_str());

		glf::vertexattrib VertexAttrib;
		glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &VertexAttrib.Enabled);
		//glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VertexAttrib.Binding);
		glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_SIZE, &VertexAttrib.Size);
		glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &VertexAttrib.Stride);
		glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_TYPE, &VertexAttrib.Type);
		glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &VertexAttrib.Normalized);
		glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &VertexAttrib.Integer);
		glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &VertexAttrib.Divisor);

		glGetVertexAttribPointerv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_POINTER, &VertexAttrib.Pointer);

		if(GL_VERTEX_ATTRIB_ARRAY_INTEGER == GL_TRUE)
		{
			if(!(
				VertexAttrib.Type == GL_INT ||  
				VertexAttrib.Type == GL_INT_VEC2 || 
				VertexAttrib.Type == GL_INT_VEC3 || 
				VertexAttrib.Type == GL_INT_VEC4 || 
				VertexAttrib.Type == GL_UNSIGNED_INT || 
				VertexAttrib.Type == GL_UNSIGNED_INT_VEC2 || 
				VertexAttrib.Type == GL_UNSIGNED_INT_VEC3 || 
				VertexAttrib.Type == GL_UNSIGNED_INT_VEC4))
				return true;

			if(!(
				VertexAttrib.Type == GL_BYTE || 
				VertexAttrib.Type == GL_UNSIGNED_BYTE || 
				VertexAttrib.Type == GL_SHORT || 
				VertexAttrib.Type == GL_UNSIGNED_SHORT || 
				VertexAttrib.Type == GL_INT || 
				VertexAttrib.Type == GL_UNSIGNED_INT))
				return true;

			//if(AttribSize > 1)
			//GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_INT, GL_UNSIGNED_INT, GL_FLOAT, and GL_DOUBLE
		}
		else if(VertexAttrib.Long == GL_TRUE) // OpenGL Spec bug 
		{
			if( VertexAttrib.Type == GL_DOUBLE || 
				VertexAttrib.Type == GL_DOUBLE_VEC2 || 
				VertexAttrib.Type == GL_DOUBLE_VEC3 || 
				VertexAttrib.Type == GL_DOUBLE_VEC4 || 
				VertexAttrib.Type == GL_DOUBLE_MAT2 || 
				VertexAttrib.Type == GL_DOUBLE_MAT3 || 
				VertexAttrib.Type == GL_DOUBLE_MAT4 || 
				VertexAttrib.Type == GL_DOUBLE_MAT2x3 || 
				VertexAttrib.Type == GL_DOUBLE_MAT2x4 || 
				VertexAttrib.Type == GL_DOUBLE_MAT3x2 ||
				VertexAttrib.Type == GL_DOUBLE_MAT3x4 || 
				VertexAttrib.Type == GL_DOUBLE_MAT4x2 || 
				VertexAttrib.Type == GL_DOUBLE_MAT4x3)
			{
				if(VertexAttrib.Type != GL_DOUBLE)
					return true;
			}
			else// if((VertexAttrib.Normalized == GL_TRUE) || (GL_VERTEX_ATTRIB_ARRAY_FLOAT == GL_TRUE))
			{
				if(!(
					VertexAttrib.Type == GL_FLOAT ||  
					VertexAttrib.Type == GL_FLOAT_VEC2 || 
					VertexAttrib.Type == GL_FLOAT_VEC3 || 
					VertexAttrib.Type == GL_FLOAT_VEC4 || 
					VertexAttrib.Type == GL_FLOAT_MAT2 || 
					VertexAttrib.Type == GL_FLOAT_MAT3 || 
					VertexAttrib.Type == GL_FLOAT_MAT4 || 
					VertexAttrib.Type == GL_FLOAT_MAT2x3 || 
					VertexAttrib.Type == GL_FLOAT_MAT2x4 || 
					VertexAttrib.Type == GL_FLOAT_MAT3x2 || 
					VertexAttrib.Type == GL_FLOAT_MAT3x4 || 
					VertexAttrib.Type == GL_FLOAT_MAT4x2 || 
					VertexAttrib.Type == GL_FLOAT_MAT4x3))
					return true;

				// It could be any vertex array attribute type
			}
		}

		printf("glGetActiveAttrib(\n\t%d, \n\t%d, \n\t%d, \n\t%d, \n\t%d, \n\t%s)\n", 
			i, AttribLocation, AttribLength, AttribSize, AttribType, NameString.c_str());
	}

	return Error;
}

void display()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
	glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Projection * View * Model;

	glProgramUniformMatrix4fv(ProgramName[program::VERT], UniformMVP, 1, GL_FALSE, &MVP[0][0]);

	glViewportIndexedfv(0, &glm::vec4(0, 0, Window.Size)[0]);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f)[0]);

	glBindProgramPipeline(PipelineName);

	glBindVertexArray(VertexArrayName);
	glPatchParameteri(GL_PATCH_VERTICES, VertexCount);

	assert(!validate(ProgramName[program::VERT]));
	glDrawArraysInstancedBaseInstance(GL_PATCHES, 0, VertexCount, 1, 0);

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
