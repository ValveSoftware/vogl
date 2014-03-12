//**********************************
// OpenGL Texture Format
// 23/01/2012 - 16/02/2013
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
	char const * SAMPLE_NAME("OpenGL Texture Format");
	char const * VERT_SHADER_SOURCE("gl-320/texture-format.vert");
	char const * FRAG_SHADER_SOURCE[3] = 
	{
		"gl-320/texture-format-normalized.frag", 
		"gl-320/texture-format-uint.frag"
	};
	char const * TEXTURE_DIFFUSE("kueken2-bgra8.dds");
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(3);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	// With DDS textures, v texture coordinate are reversed, from top to bottom
	GLsizei const VertexCount(6);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v2fv2f);
	glf::vertex_v2fv2f const VertexData[VertexCount] =
	{
		glf::vertex_v2fv2f(glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f,-1.0f), glm::vec2(1.0f, 1.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2( 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 0.0f)),
		glf::vertex_v2fv2f(glm::vec2(-1.0f,-1.0f), glm::vec2(0.0f, 1.0f))
	};

	namespace texture
	{
		enum type
		{
			RGBA8, // GL_RGBA8
			RGBA8UI, // GL_RGBA8UI
			RGBA16F, // GL_RGBA16F
			RGBA32F, // GL_RGBA32F
			RGBA8_SNORM, 
			//RGB10_A2UI, // GL_RGB10_A2UI
/*
			RGBA8I,
			RGBA16I, // GL_RGBA16_SNORM
			RGBA32I, // GL_RGBA8I
			RGBA8U, // GL_RGBA8
			RGBA16U,
			RGBA32U,
			RGBA16F, // GL_RGBA16F
			RGBA32F, // GL_RGBA32F
			RGBA8_SNORM, // GL_RGBA8_SNORM
*/
			MAX
		};
	}//namespace texture

	namespace program
	{
		enum type
		{
			NORMALIZED, 
			UINT, 
			MAX
		};
	}//namespace program

	GLenum TextureInternalFormat[texture::MAX] = 
	{
		GL_RGBA8, 
		GL_RGBA8UI, 
		GL_RGBA16F,
		GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
		GL_RGBA8_SNORM
	};

	GLenum TextureFormat[texture::MAX] = 
	{
		GL_BGRA, 
		GL_BGRA_INTEGER, 
		GL_BGRA,
		GL_BGRA,
		GL_BGRA
	};

	GLuint VertexArrayName(0);
	GLuint ProgramName[program::MAX] = {0, 0};

	GLuint BufferName(0);
	GLuint TextureName[texture::MAX] = {0, 0};

	GLint UniformMVP[program::MAX] = {0, 0};
	GLint UniformDiffuse[program::MAX] = {0, 0};

	glm::ivec4 Viewport[texture::MAX] = 
	{
		glm::ivec4(  0,   0, 320, 240),
		glm::ivec4(320,   0, 320, 240),
		glm::ivec4(320, 240, 320, 240),
		glm::ivec4(  0, 240, 320, 240)
	};

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
	bool Validated(true);
	
	for(std::size_t i = 0; (i < program::MAX) && Validated; ++i)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE[i]);

		Validated = Validated && glf::checkShader(VertShaderName, VERT_SHADER_SOURCE);
		Validated = Validated && glf::checkShader(FragShaderName, FRAG_SHADER_SOURCE[i]);

		ProgramName[i] = glCreateProgram();
		glAttachShader(ProgramName[i], VertShaderName);
		glAttachShader(ProgramName[i], FragShaderName);
		glBindAttribLocation(ProgramName[i], glf::semantic::attr::POSITION, "Position");
		glBindAttribLocation(ProgramName[i], glf::semantic::attr::TEXCOORD, "Texcoord");
		glBindFragDataLocation(ProgramName[i], glf::semantic::frag::COLOR, "Color");
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		glLinkProgram(ProgramName[i]);
		Validated = Validated && glf::checkProgram(ProgramName[i]);

		UniformMVP[i] = glGetUniformLocation(ProgramName[i], "MVP");
		Validated = Validated && (UniformMVP[i] != -1);
		UniformDiffuse[i] = glGetUniformLocation(ProgramName[i], "Diffuse");
		Validated = Validated && (UniformDiffuse[i] != -1);
	}

	return Validated && glf::checkError("initProgram");
}

bool initBuffer()
{
	glGenBuffers(1, &BufferName);

	glBindBuffer(GL_ARRAY_BUFFER, BufferName);
	glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return glf::checkError("initBuffer");;
}

bool initTexture()
{
	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(texture::MAX, TextureName);
	glActiveTexture(GL_TEXTURE0);

	for(std::size_t i = 0; i < texture::MAX; ++i)
	{
		glBindTexture(GL_TEXTURE_2D, TextureName[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
		{
			glTexImage2D(
				GL_TEXTURE_2D, 
				GLint(Level), 
				TextureInternalFormat[i], 
				GLsizei(Texture[Level].dimensions().x), 
				GLsizei(Texture[Level].dimensions().y), 
				0,  
				TextureFormat[i], 
				GL_UNSIGNED_BYTE, 
				Texture[Level].data());
		}
	}
	
	glBindTexture(GL_TEXTURE_2D, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	return glf::checkError("initTexture");
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(glf::vertex_v2fv2f), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnableVertexAttribArray(glf::semantic::attr::POSITION);
		glEnableVertexAttribArray(glf::semantic::attr::TEXCOORD);
	glBindVertexArray(0);

	return glf::checkError("initVertexArray");
}

bool begin()
{
	bool Validated = glf::checkGLVersion(SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);

	if(Validated && glf::checkExtension("GL_ARB_debug_output"))
		Validated = initDebugOutput();
	if(Validated)
		Validated = initTexture();
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
	glDeleteBuffers(1, &BufferName);
	for(std::size_t i = 0; i < program::MAX; ++i)
		glDeleteProgram(ProgramName[i]);
	glDeleteTextures(texture::MAX, TextureName);
	glDeleteVertexArrays(1, &VertexArrayName);

	return glf::checkError("end");
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

	glViewport(0, 0, Window.Size.x, Window.Size.y);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);
	glBindVertexArray(VertexArrayName);
	glActiveTexture(GL_TEXTURE0);

	{
		std::size_t ViewportIndex(0); 
		glViewport(Viewport[ViewportIndex].x, Viewport[ViewportIndex].y, Viewport[ViewportIndex].z, Viewport[ViewportIndex].w);

		glUseProgram(ProgramName[program::UINT]);
		glUniform1i(UniformDiffuse[program::UINT], 0);
		glUniformMatrix4fv(UniformMVP[program::UINT], 1, GL_FALSE, &MVP[0][0]);

		glBindTexture(GL_TEXTURE_2D, TextureName[texture::RGBA8UI]);

		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);
	}

	{
		std::size_t ViewportIndex(1); 
		glViewport(Viewport[ViewportIndex].x, Viewport[ViewportIndex].y, Viewport[ViewportIndex].z, Viewport[ViewportIndex].w);

		glUseProgram(ProgramName[program::NORMALIZED]);
		glUniform1i(UniformDiffuse[program::NORMALIZED], 0);
		glUniformMatrix4fv(UniformMVP[program::NORMALIZED], 1, GL_FALSE, &MVP[0][0]);

		glBindTexture(GL_TEXTURE_2D, TextureName[texture::RGBA16F]);

		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);
	}

	{
		std::size_t ViewportIndex(2); 
		glViewport(Viewport[ViewportIndex].x, Viewport[ViewportIndex].y, Viewport[ViewportIndex].z, Viewport[ViewportIndex].w);

		glUseProgram(ProgramName[program::NORMALIZED]);
		glUniform1i(UniformDiffuse[program::NORMALIZED], 0);
		glUniformMatrix4fv(UniformMVP[program::NORMALIZED], 1, GL_FALSE, &MVP[0][0]);

		glBindTexture(GL_TEXTURE_2D, TextureName[texture::RGBA8]);

		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);
	}

	{
		std::size_t ViewportIndex(3); 
		glViewport(Viewport[ViewportIndex].x, Viewport[ViewportIndex].y, Viewport[ViewportIndex].z, Viewport[ViewportIndex].w);

		glUseProgram(ProgramName[program::NORMALIZED]);
		glUniform1i(UniformDiffuse[program::NORMALIZED], 0);
		glUniformMatrix4fv(UniformMVP[program::NORMALIZED], 1, GL_FALSE, &MVP[0][0]);

		glBindTexture(GL_TEXTURE_2D, TextureName[texture::RGBA8_SNORM]);

		glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);
	}

	glf::checkError("display");
	glf::swapBuffers();
}

int main(int argc, char* argv[])
{
	return glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT, 
		::SAMPLE_MAJOR_VERSION, 
		::SAMPLE_MINOR_VERSION);
}
