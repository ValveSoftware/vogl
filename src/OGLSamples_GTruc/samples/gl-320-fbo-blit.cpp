//**********************************
// OpenGL Framebuffer Blit
// 05/12/2009 - 22/02/2013
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
	char const * SAMPLE_NAME("OpenGL Framebuffer Blit");
	char const * VERT_SHADER_SOURCE("gl-320/fbo-blit.vert");
	char const * FRAG_SHADER_SOURCE("gl-320/fbo-blit.frag");
	char const * TEXTURE_DIFFUSE("kueken1-bgr8.dds");
	glm::ivec2 const FRAMEBUFFER_SIZE(512, 512);
	int const SAMPLE_SIZE_WIDTH(640);
	int const SAMPLE_SIZE_HEIGHT(480);
	int const SAMPLE_MAJOR_VERSION(3);
	int const SAMPLE_MINOR_VERSION(2);

	glf::window Window(glm::ivec2(SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT));

	struct vertex
	{
		vertex
		(
			glm::vec2 const & Position,
			glm::vec2 const & Texcoord
		) :
			Position(Position),
			Texcoord(Texcoord)
		{}

		glm::vec2 Position;
		glm::vec2 Texcoord;
	};

	// With DDS textures, v texture coordinate are reversed, from top to bottom
	GLsizei const VertexCount(6);
	GLsizeiptr const VertexSize = VertexCount * sizeof(vertex);
	vertex const VertexData[VertexCount] =
	{
		vertex(glm::vec2(-3.0f,-3.0f), glm::vec2(0.0f, 0.0f)),
		vertex(glm::vec2( 3.0f,-3.0f), glm::vec2(1.0f, 0.0f)),
		vertex(glm::vec2( 3.0f, 3.0f), glm::vec2(1.0f, 1.0f)),
		vertex(glm::vec2( 3.0f, 3.0f), glm::vec2(1.0f, 1.0f)),
		vertex(glm::vec2(-3.0f, 3.0f), glm::vec2(0.0f, 1.0f)),
		vertex(glm::vec2(-3.0f,-3.0f), glm::vec2(0.0f, 0.0f))
	};

	namespace framebuffer
	{
		enum type
		{
			RENDER,
			RESOLVE,
			MAX
		};
	}//namespace framebuffer

	namespace texture
	{
		enum type
		{
			DIFFUSE,
			COLORBUFFER,
			MAX
		};
	}//namespace texture

	GLuint ProgramName(0);
	GLint UniformMVP(0);
	GLint UniformDiffuse(0);

	GLuint VertexArrayName(0);
	GLuint BufferName(0);
	GLuint ColorRenderbufferName(0);
	std::vector<GLuint> TextureName(texture::MAX);
	std::vector<GLuint> FramebufferName(framebuffer::MAX);
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
	
	// Create program
	if(Validated)
	{
		GLuint VertShaderName = glf::createShader(GL_VERTEX_SHADER, glf::DATA_DIRECTORY + VERT_SHADER_SOURCE);
		GLuint FragShaderName = glf::createShader(GL_FRAGMENT_SHADER, glf::DATA_DIRECTORY + FRAG_SHADER_SOURCE);

		Validated = Validated && glf::checkShader(VertShaderName, VERT_SHADER_SOURCE);
		Validated = Validated && glf::checkShader(FragShaderName, FRAG_SHADER_SOURCE);

		ProgramName = glCreateProgram();
		glAttachShader(ProgramName, VertShaderName);
		glAttachShader(ProgramName, FragShaderName);
		glBindAttribLocation(ProgramName, glf::semantic::attr::POSITION, "Position");
		glBindAttribLocation(ProgramName, glf::semantic::attr::TEXCOORD, "Texcoord");
		glBindFragDataLocation(ProgramName, glf::semantic::frag::COLOR, "Color");
		glDeleteShader(VertShaderName);
		glDeleteShader(FragShaderName);

		glLinkProgram(ProgramName);
		Validated = Validated && glf::checkProgram(ProgramName);
	}

	if(Validated)
	{
		UniformMVP = glGetUniformLocation(ProgramName, "MVP");
		UniformDiffuse = glGetUniformLocation(ProgramName, "Diffuse");
	}

	return glf::checkError("initProgram");
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
	glGenTextures(texture::MAX, &TextureName[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	gli::texture2D Texture(gli::loadStorageDDS(glf::DATA_DIRECTORY + TEXTURE_DIFFUSE));
	for(std::size_t Level = 0; Level < Texture.levels(); ++Level)
	{
		glTexImage2D(
			GL_TEXTURE_2D, 
			GLint(Level), 
			GL_RGB8, 
			GLsizei(Texture[Level].dimensions().x), 
			GLsizei(Texture[Level].dimensions().y), 
			0,
			GL_BGR, 
			GL_UNSIGNED_BYTE, 
			Texture[Level].data());
	}
	glGenerateMipmap(GL_TEXTURE_2D); // Allocate all mipmaps memory

	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	return glf::checkError("initTexture");
}

bool initFramebuffer()
{
	glGenFramebuffers(framebuffer::MAX, &FramebufferName[0]);

	glGenRenderbuffers(1, &ColorRenderbufferName);
	glBindRenderbuffer(GL_RENDERBUFFER, ColorRenderbufferName);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::RENDER]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ColorRenderbufferName);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return glf::checkError("initFramebuffer");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::RESOLVE]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TextureName[texture::COLORBUFFER], 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return glf::checkError("initFramebuffer");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return glf::checkError("initFramebuffer");
}

bool initVertexArray()
{
	glGenVertexArrays(1, &VertexArrayName);
	glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, BufferName);
		glVertexAttribPointer(glf::semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), GLF_BUFFER_OFFSET(0));
		glVertexAttribPointer(glf::semantic::attr::TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), GLF_BUFFER_OFFSET(sizeof(glm::vec2)));
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
		Validated = initProgram();
	if(Validated)
		Validated = initBuffer();
	if(Validated)
		Validated = initTexture();
	if(Validated)
		Validated = initFramebuffer();
	if(Validated)
		Validated = initVertexArray();

	return Validated && glf::checkError("begin");
}

bool end()
{
	glDeleteBuffers(1, &BufferName);
	glDeleteProgram(ProgramName);
	glDeleteTextures(texture::MAX, &TextureName[0]);
	glDeleteRenderbuffers(1, &ColorRenderbufferName);
	glDeleteFramebuffers(framebuffer::MAX, &FramebufferName[0]);
	glDeleteVertexArrays(1, &VertexArrayName);

	return glf::checkError("end");
}

void renderFBO()
{
	glm::mat4 Perspective = glm::perspective(45.0f, float(FRAMEBUFFER_SIZE.y) / FRAMEBUFFER_SIZE.x, 0.1f, 100.0f);
	glm::mat4 ViewFlip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f,-1.0f, 1.0f));
	glm::mat4 View = glm::translate(ViewFlip, glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0 + 2.0));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Perspective * View * Model;
	glUniformMatrix4fv(UniformMVP, 1, GL_FALSE, &MVP[0][0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::DIFFUSE]);
	glBindVertexArray(VertexArrayName);

	glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);

	glf::checkError("renderFBO");
}

void renderFB()
{
	glm::mat4 Perspective = glm::perspective(45.0f, float(Window.Size.x) / Window.Size.y, 0.1f, 100.0f);
	glm::mat4 ViewTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Window.TranlationCurrent.y * 2.0));
	glm::mat4 ViewRotateX = glm::rotate(ViewTranslate, Window.RotationCurrent.y, glm::vec3(1.f, 0.f, 0.f));
	glm::mat4 View = glm::rotate(ViewRotateX, Window.RotationCurrent.x, glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 Model = glm::mat4(1.0f);
	glm::mat4 MVP = Perspective * View * Model;
	glUniformMatrix4fv(UniformMVP, 1, GL_FALSE, &MVP[0][0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);
	glBindVertexArray(VertexArrayName);

	glDrawArraysInstanced(GL_TRIANGLES, 0, VertexCount, 1);

	glf::checkError("renderFB");
}

void display()
{
	glUseProgram(ProgramName);
	glUniform1i(UniformDiffuse, 0);

	// Pass 1
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName[framebuffer::RENDER]);
	glViewport(0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(0.0f, 0.5f, 1.0f, 1.0f)[0]);
	renderFBO();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Generate FBO mipmaps
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureName[texture::COLORBUFFER]);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Blit framebuffers
	GLint const Border = 2;
	int const Tile = 4;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferName[framebuffer::RENDER]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferName[framebuffer::RESOLVE]);

	for(int j = 0; j < Tile; ++j)
	for(int i = 0; i < Tile; ++i)
	{
		if((i + j) % 2)
			continue;

		glBlitFramebuffer(0, 0, FRAMEBUFFER_SIZE.x, FRAMEBUFFER_SIZE.y, 
			FRAMEBUFFER_SIZE.x / Tile * (i + 0) + Border, 
			FRAMEBUFFER_SIZE.x / Tile * (j + 0) + Border, 
			FRAMEBUFFER_SIZE.y / Tile * (i + 1) - Border, 
			FRAMEBUFFER_SIZE.y / Tile * (j + 1) - Border, 
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}

	// Pass 2
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Window.Size.x, Window.Size.y);
	glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)[0]);
	renderFB();

	glf::swapBuffers();
	glf::checkError("Render");
}

int main(int argc, char* argv[])
{
	return glf::run(
		argc, argv,
		glm::ivec2(::SAMPLE_SIZE_WIDTH, ::SAMPLE_SIZE_HEIGHT), 
		GLF_CONTEXT_CORE_PROFILE_BIT, 
		::SAMPLE_MAJOR_VERSION, ::SAMPLE_MINOR_VERSION);
}
