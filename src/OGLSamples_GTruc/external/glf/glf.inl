
bool check();
bool begin();
bool end();
void display();

#define GLF_CONTEXT_CORE_PROFILE_BIT 0x0001
#define GLF_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x0002
#define GLF_CONTEXT_ES2_PROFILE_BIT 0x0004
#define GLF_BUFFER_OFFSET(i) ((char *)NULL + (i))

namespace glf
{
	static GLFWwindow* glf_window = NULL;
	inline void logImplementationDependentLimit(GLenum Value, std::string const & String)
	{
		GLint Result(0);
		glGetIntegerv(Value, &Result);
		std::string Message(glf::format("%s: %d", String.c_str(), Result));
#		if(!defined(__APPLE__))
			glDebugMessageInsertARB(GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_OTHER_ARB, 1, GL_DEBUG_SEVERITY_LOW_ARB, GLsizei(Message.size()), Message.c_str());
#		endif
	}

	inline void swapBuffers()
	{
		glfwSwapBuffers(glf_window);
		//assert(glGetError() == GL_NO_ERROR); // 'glutSwapBuffers' generates an here with OpenGL 3 > core profile ... :/
	}

	inline bool checkGLVersion(GLint MajorVersionRequire, GLint MinorVersionRequire)
	{
		GLint MajorVersionContext = 0;
		GLint MinorVersionContext = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &MajorVersionContext);
		glGetIntegerv(GL_MINOR_VERSION, &MinorVersionContext);
		printf("OpenGL Version Needed %d.%d ( %d.%d Found )\n",
			MajorVersionRequire, MinorVersionRequire,
			MajorVersionContext,MinorVersionContext);
		return glf::version(MajorVersionContext, MinorVersionContext) 
			>= glf::version(MajorVersionRequire, MinorVersionRequire);
	}

	inline bool checkExtension(char const * String)
	{
		GLint ExtensionCount = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &ExtensionCount);
		for(GLint i = 0; i < ExtensionCount; ++i)
			if(std::string((char const*)glGetStringi(GL_EXTENSIONS, i)) == std::string(String))
				return true;
		printf("Failed to find Extension: \"%s\"\n",String);
		return false;
	}

	inline int version(int Major, int Minor)
	{
		return Major * 100 + Minor * 10;
	}

	inline bool saveBinary
	(
		std::string const & Filename, 
		GLenum const & Format,
		std::vector<glm::byte> const & Data,
		GLint const & Size
	)
	{
		FILE* File = fopen(Filename.c_str(), "wb");

		if(File)
		{
			fwrite(&Format, sizeof(GLenum), 1, File);
			fwrite(&Size, sizeof(Size), 1, File);
			fwrite(&Data[0], Size, 1, File);
			fclose(File);
			return true;
		}
		return false;
	}

	inline bool loadBinary
	(
		std::string const & Filename,
		GLenum & Format,
		std::vector<glm::byte> & Data,
		GLint & Size
	)
	{
		FILE* File = fopen(Filename.c_str(), "rb");

		if(File)
		{
			fread(&Format, sizeof(GLenum), 1, File);
			fread(&Size, sizeof(Size), 1, File);
			Data.resize(Size);
			fread(&Data[0], Size, 1, File);
			fclose(File);
			return true;
		}
		return false;
	}

	inline std::string loadFile
	(
		std::string const & Filename
	)
	{
		std::string Result;
		
		std::ifstream Stream(Filename.c_str());
		if(!Stream.is_open())
			return Result;

		Stream.seekg(0, std::ios::end);
		Result.reserve(Stream.tellg());
		Stream.seekg(0, std::ios::beg);
		
		Result.assign(
			(std::istreambuf_iterator<char>(Stream)),
			std::istreambuf_iterator<char>());

		return Result;
	}

	static void APIENTRY debugOutput
	(
		GLenum source,
		GLenum type,
		GLuint id,
		GLenum severity,
		GLsizei length,
		const GLchar* message,
		GLvoid* userParam
	)
	{
		//FILE* f;
		//f = fopen("debug_output.txt","a");
		//if(f)
		{
			char debSource[32], debType[32], debSev[32];
			bool Error(false);

			if(source == GL_DEBUG_SOURCE_API_ARB)
				strcpy(debSource, "OpenGL");
			else if(source == GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB)
				strcpy(debSource, "Windows");
			else if(source == GL_DEBUG_SOURCE_SHADER_COMPILER_ARB)
				strcpy(debSource, "Shader Compiler");
			else if(source == GL_DEBUG_SOURCE_THIRD_PARTY_ARB)
				strcpy(debSource, "Third Party");
			else if(source == GL_DEBUG_SOURCE_APPLICATION_ARB)
				strcpy(debSource, "Application");
			else if (source == GL_DEBUG_SOURCE_OTHER_ARB)
				strcpy(debSource, "Other");
			else
				assert(0);
 
			if(type == GL_DEBUG_TYPE_ERROR)
				strcpy(debType, "error");
			else if(type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR)
				strcpy(debType, "deprecated behavior");
			else if(type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
				strcpy(debType, "undefined behavior");
			else if(type == GL_DEBUG_TYPE_PORTABILITY)
				strcpy(debType, "portability");
			else if(type == GL_DEBUG_TYPE_PERFORMANCE)
				strcpy(debType, "performance");
			else if(type == GL_DEBUG_TYPE_OTHER)
				strcpy(debType, "message");
			else if(type == GL_DEBUG_TYPE_MARKER)
				strcpy(debType, "marker");
			else if(type == GL_DEBUG_TYPE_PUSH_GROUP)
				strcpy(debType, "push group");
			else if(type == GL_DEBUG_TYPE_POP_GROUP)
				strcpy(debType, "pop group");
			else
				assert(0);
 
			if(severity == GL_DEBUG_SEVERITY_HIGH_ARB)
			{
				strcpy(debSev, "high");
				//Error = true;
			}
			else if(severity == GL_DEBUG_SEVERITY_MEDIUM_ARB)
				strcpy(debSev, "medium");
			else if(severity == GL_DEBUG_SEVERITY_LOW_ARB)
				strcpy(debSev, "low");
			else if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
				strcpy(debSev, "notification");
			else
				assert(0);

			 fprintf(stderr,"%s: %s(%s) %d: %s\n", debSource, debType, debSev, id, message);
			 assert(!Error);
			 //fclose(f);
		}
	}
/*
	void checkDebugOutput()
	{
			unsigned int count = 10; // max. num. of messages that will be read from the log
			int bufsize = 2048;
	 
			unsigned int* sources      = new unsigned int[count];
			unsigned int* types        = new unsigned int[count];
			unsigned int* ids   = new unsigned int[count];
			unsigned int* severities = new unsigned int[count];
			int* lengths = new int[count];
	 
			char* messageLog = new char[bufsize];
	 
			unsigned int retVal = glGetDebugMessageLogARB(count, bufsize, sources, types, ids, severities, lengths, messageLog);
			if(retVal > 0)
			{
					unsigned int pos = 0;
					for(unsigned int i=0; i<retVal; i++)
					{
						debugOutput(sources[i], types[i], ids[i], severities[i], NULL, &messageLog[pos], NULL);
						pos += lengths[i];
					}
			}
			delete [] sources;
			delete [] types;
			delete [] ids;
			delete [] severities;
			delete [] lengths;
			delete [] messageLog;
	}
*/

	static void cursor_position_callback(GLFWwindow* pWindow,double x, double y)
	{
		Window.MouseCurrent = glm::ivec2(x, y);
		Window.TranlationCurrent = Window.MouseButtonFlags & glf::MOUSE_BUTTON_LEFT ? Window.TranlationOrigin + (Window.MouseCurrent - Window.MouseOrigin) / 10.f : Window.TranlationOrigin;
		Window.RotationCurrent = Window.MouseButtonFlags & glf::MOUSE_BUTTON_RIGHT ? Window.RotationOrigin + (Window.MouseCurrent - Window.MouseOrigin) : Window.RotationOrigin;
	}

	static void mouse_button_callback (GLFWwindow* pWindow, int Button, int Action, int mods)
	{
		switch(Action)
		{
			case GLFW_PRESS:
			{
				Window.MouseOrigin = Window.MouseCurrent;
				switch(Button)
				{
					case GLFW_MOUSE_BUTTON_LEFT:
					{
						Window.MouseButtonFlags |= glf::MOUSE_BUTTON_LEFT;
						Window.TranlationOrigin = Window.TranlationCurrent;
					}
					break;
					case GLFW_MOUSE_BUTTON_MIDDLE:
					{
						Window.MouseButtonFlags |= glf::MOUSE_BUTTON_MIDDLE;
					}
					break;
					case GLFW_MOUSE_BUTTON_RIGHT:
					{
						Window.MouseButtonFlags |= glf::MOUSE_BUTTON_RIGHT;
						Window.RotationOrigin = Window.RotationCurrent;
					}
					break;
				}
			}
			break;
			case GLFW_RELEASE:
			{
				switch(Button)
				{
					case GLFW_MOUSE_BUTTON_LEFT:
					{
						Window.TranlationOrigin += (Window.MouseCurrent - Window.MouseOrigin) / 10.f;
						Window.MouseButtonFlags &= ~glf::MOUSE_BUTTON_LEFT;
					}
					break;
					case GLFW_MOUSE_BUTTON_MIDDLE:
					{
						Window.MouseButtonFlags &= ~glf::MOUSE_BUTTON_MIDDLE;
					}
					break;
					case GLFW_MOUSE_BUTTON_RIGHT:
					{
						Window.RotationOrigin += Window.MouseCurrent - Window.MouseOrigin;
						Window.MouseButtonFlags &= ~glf::MOUSE_BUTTON_RIGHT;
					}
					break;
				}
			}
			break;
		}
	}

	static void key_callback( GLFWwindow* pWindow, int key, int scancode, int action, int mods)
	{
		switch(action)
		{
			case GLFW_PRESS:
			{
				Window.KeyPressed[key] = 1;
				break;
			}
			case GLFW_RELEASE:
			{
				Window.KeyPressed[key] = 0;
				break;
			}
		}

		if(Window.KeyPressed[GLFW_KEY_ESCAPE] == 1)
		{
			glfwSetWindowShouldClose(pWindow, GL_TRUE);
		}
	}

	inline int run
	(
		int argc, char* argv[], 
		glm::ivec2 const & Size, 
		int Profile,
		int Major, int Minor
	)
	{
		glfwInit();


		if(version(Major, Minor) >= version(3, 2))
		{
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, Major);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, Minor);

#			if defined(__APPLE__)
				glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#			elif defined(__INTEL__)
				glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
#			else
				glfwWindowHint(GLFW_OPENGL_PROFILE, Profile == GLF_CONTEXT_CORE_PROFILE_BIT ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_COMPAT_PROFILE);
				glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, Profile == GLF_CONTEXT_CORE_PROFILE_BIT ? GL_TRUE : GL_FALSE);
#			endif

#			if defined(NDEBUG)
				glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
#			else
				glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#			endif
		}
		glf_window = glfwCreateWindow(Size.x, Size.y, argv[0], NULL,NULL);
		assert(glf_window!= NULL);

		glfwSetMouseButtonCallback(glf_window,mouse_button_callback);
		glfwSetCursorPosCallback(glf_window,cursor_position_callback);
		glfwSetKeyCallback(glf_window,key_callback);
		glfwMakeContextCurrent(glf_window);

		glewExperimental = GL_TRUE;
		glewInit();
		glGetError();

		bool Run = begin();
		printf("Running Test\n");
		if(Run)
		{
			while(true)
			{
				display();
				// Wait for new events
				glfwWaitEvents();
				if(glfwWindowShouldClose(glf_window))
					break;
			}
		}
		printf("Test Ended\n");

		if(Run) {
				printf("Test Began Correctly.\n");
		}
		// Also Exits Program
		glfwTerminate();
		exit(EXIT_SUCCESS);
		return Run ? 0 : 1;
	}

	bool validateVAO43(GLuint VertexArrayName, std::vector<glf::vertexattrib> const & Expected)
	{
		bool Success = true;
#if !defined(__APPLE__)
		GLint MaxVertexAttrib(0);
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &MaxVertexAttrib);

		glBindVertexArray(VertexArrayName);
		for (GLuint AttribLocation = 0; AttribLocation < glm::min(GLuint(MaxVertexAttrib), GLuint(Expected.size())); ++AttribLocation)
		{
			glf::vertexattrib VertexAttrib;
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &VertexAttrib.Enabled);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VertexAttrib.Binding);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_SIZE, &VertexAttrib.Size);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &VertexAttrib.Stride);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_TYPE, &VertexAttrib.Type);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &VertexAttrib.Normalized);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &VertexAttrib.Integer);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_LONG, &VertexAttrib.Long);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &VertexAttrib.Divisor);
			glGetVertexAttribPointerv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_POINTER, &VertexAttrib.Pointer);
			Success = Success && (VertexAttrib == Expected[AttribLocation]);
			assert(Success);
		}
		glBindVertexArray(0);
#endif//!defined(__APPLE__)
		return Success;
	}

	bool validateVAO42(GLuint VertexArrayName, std::vector<glf::vertexattrib> const & Expected)
	{
		bool Success = true;
#if !defined(__APPLE__)
		GLint MaxVertexAttrib(0);
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &MaxVertexAttrib);

		glBindVertexArray(VertexArrayName);
		for(GLuint AttribLocation = 0; AttribLocation < glm::min(GLuint(MaxVertexAttrib), GLuint(Expected.size())); ++AttribLocation)
		{
			glf::vertexattrib VertexAttrib;
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &VertexAttrib.Enabled);
			//glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VertexAttrib.Binding);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_SIZE, &VertexAttrib.Size);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &VertexAttrib.Stride);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_TYPE, &VertexAttrib.Type);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &VertexAttrib.Normalized);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &VertexAttrib.Integer);
			//glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_LONG, &VertexAttrib.Long);
			glGetVertexAttribiv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &VertexAttrib.Divisor);
			glGetVertexAttribPointerv(AttribLocation, GL_VERTEX_ATTRIB_ARRAY_POINTER, &VertexAttrib.Pointer);
			Success = Success && (VertexAttrib == Expected[AttribLocation]);
			assert(Success);
		}
		glBindVertexArray(0);
#endif//!defined(__APPLE__)
		return Success;
	}

}//namespace glf
