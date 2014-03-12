//**********************************
// Compiler
// 21/09/2012 - 21/09/2012
//**********************************
// Christophe Riccio
// ogl-samples@g-truc.net
//**********************************
// G-Truc Creation
// www.g-truc.net
//**********************************

namespace glf
{
	// compiler::commandline

	inline compiler::commandline::commandline
	(
		std::string const & Filename,
		std::string const & Arguments
	) :
		Profile("core"),
		Version(-1)
	{
		std::size_t PathOffset = Filename.find_last_of("/");
		std::string FilePath = Filename.substr(0, PathOffset + 1);

		this->Includes.push_back(FilePath);
		this->parseArguments(Arguments);
	}

	inline void compiler::commandline::parseArguments(std::string const & Arguments)
	{
		std::stringstream Stream(Arguments);
		std::string Param;

		while(!Stream.eof())
		{
			Stream >> Param;

			std::size_t FoundDefine = Param.find("-D");
			std::size_t FoundInclude = Param.find("-I");

			if(FoundDefine != std::string::npos)
				this->Defines.push_back(Param.substr(2, Param.size() - 2));
			else if(FoundInclude != std::string::npos)
				this->Includes.push_back(glf::DATA_DIRECTORY + Param.substr(2, Param.size() - 2));
			else if(Param == "--define")
			{
				std::string Define;
				Stream >> Define;
				this->Defines.push_back(Define);
			}
			else if((Param == "--version") || (Param == "-v"))
				Stream >> Version;
			else if((Param == "--profile") || (Param == "-p"))
				Stream >> Profile;
			else if (Param == "--include")
			{
				std::string Include;
				Stream >> Include;
				this->Includes.push_back(glf::DATA_DIRECTORY + Include);
			}
/*
			else 
			{
				std::stringstream err;
				err << "unknown parameter type: \"" << Param << "\"";
				glDebugMessageInsertARB(
					GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_OTHER_ARB, 1, GL_DEBUG_SEVERITY_LOW_ARB, 
					-1, std::string(std::string("unknown parameter type: \"") << Param << std::string("\"")).c_str());
			}
			if(!Stream) 
			{
				glDebugMessageInsertARB(
					GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_OTHER_ARB, 1, GL_DEBUG_SEVERITY_LOW_ARB, 
					-1, std::string(std::string("error parsing parameter: \"") << Param << std::string("\"")).c_str());
			}
*/
		}
	}

	inline std::string compiler::commandline::getDefines() const
	{
		std::string Result;
		for(std::size_t i = 0; i < this->Defines.size(); ++i)
			Result += std::string("#define ") + this->Defines[i] + std::string("\n");
		return Result;
	}

	// compiler::parser

	inline std::string compiler::parser::operator()  
	(
		commandline const & CommandLine,
		std::string const & Filename
	) const
	{
		std::string Source = glf::loadFile(Filename);

		std::stringstream Stream;
		Stream << Source;
		std::string Line, Text;

		// Handle command line version and profile arguments
		if(CommandLine.getVersion() != -1)
			Text += glf::format("#version %d %s\n", CommandLine.getVersion(), CommandLine.getProfile().c_str());

		// Handle command line defines
		Text += CommandLine.getDefines();

		while(std::getline(Stream, Line))
		{
			std::size_t Offset = 0;

			// Version
			Offset = Line.find("#version");
			if(Offset != std::string::npos)
			{
				std::size_t CommentOffset = Line.find("//");
				if(CommentOffset != std::string::npos && CommentOffset < Offset)
					continue;

				// Reorder so that the #version line is always the first of a shader text
				if(CommandLine.getVersion() == -1)
					Text = Line + std::string("\n") + Text;
				// else skip is version is only mentionned
				continue;
			}

			// Include
			Offset = Line.find("#include");
			if(Offset != std::string::npos)
			{
				std::size_t CommentOffset = Line.find("//");
				if(CommentOffset != std::string::npos && CommentOffset < Offset)
					continue;

				std::string Include = parseInclude(Line, Offset);

				std::vector<std::string> Includes = CommandLine.getIncludes();

				for(std::size_t i = 0; i < Includes.size(); ++i)
				{
					std::string PathName = Includes[i] + Include;
					std::string Source = glf::loadFile(PathName);
					if(!Source.empty())
					{
						Text += Source;
						break;
					}
				}

				continue;
			} 

			Text += Line + "\n";
		}

		return Text;
	}

	inline std::string compiler::parser::parseInclude(std::string const & Line, std::size_t const & Offset) const
	{
		std::string Result;

		std::string::size_type IncludeFirstQuote = Line.find("\"", Offset);
		std::string::size_type IncludeSecondQuote = Line.find("\"", IncludeFirstQuote + 1);

		return Line.substr(IncludeFirstQuote + 1, IncludeSecondQuote - IncludeFirstQuote - 1);
	}

	// compiler
	inline compiler::~compiler()
	{
		this->clear();
	}

	inline GLuint compiler::create(
		GLenum Type, 
		std::string const & Filename,
		std::string const & Arguments)
	{
		assert(!Filename.empty());
	
		commandline CommandLine(Filename, Arguments);

		std::string PreprocessedSource = parser()(CommandLine, Filename);
		char const * PreprocessedSourcePointer = PreprocessedSource.c_str();

		fprintf(stdout, "%s\n", PreprocessedSource.c_str());

		GLuint Name = glCreateShader(Type);
		glShaderSource(Name, 1, &PreprocessedSourcePointer, NULL);
		glCompileShader(Name);

		std::pair<files_map::iterator, bool> ResultFiles = this->ShaderFiles.insert(std::make_pair(Name, Filename));
		assert(ResultFiles.second);
		std::pair<names_map::iterator, bool> ResultNames = this->ShaderNames.insert(std::make_pair(Filename, Name));
		assert(ResultNames.second);
		std::pair<names_map::iterator, bool> ResultChecks = this->PendingChecks.insert(std::make_pair(Filename, Name));
		assert(ResultChecks.second);

		return Name;
	}

	inline bool compiler::destroy(GLuint const & Name)
	{
		files_map::iterator NameIterator = this->ShaderFiles.find(Name);
		if(NameIterator == this->ShaderFiles.end())
			return false; // Shader name not found
		std::string File = NameIterator->second;
		this->ShaderFiles.erase(NameIterator);

		// Remove from the pending checks list
		names_map::iterator PendingIterator = this->PendingChecks.find(File);
		if(PendingIterator != this->PendingChecks.end())
			this->PendingChecks.erase(PendingIterator);

		// Remove from the pending checks list
		names_map::iterator FileIterator = this->ShaderNames.find(File);
		assert(FileIterator != this->ShaderNames.end());
		this->ShaderNames.erase(FileIterator);

		return true;
	}

	// TODO Interaction with KHR_debug
	inline bool compiler::check()
	{
		bool Success(true);

		for
		(
			names_map::iterator ShaderIterator = PendingChecks.begin();
			ShaderIterator != PendingChecks.end();
			++ShaderIterator
		)
		{
			GLuint ShaderName = ShaderIterator->second;
			GLint Result = GL_FALSE;
			glGetShaderiv(ShaderName, GL_COMPILE_STATUS, &Result);

			if(Result == GL_FALSE)
			{
				int InfoLogLength;
				glGetShaderiv(ShaderName, GL_INFO_LOG_LENGTH, &InfoLogLength);
				if(InfoLogLength > 0)
				{
					std::vector<char> Buffer(InfoLogLength);
					glGetShaderInfoLog(ShaderName, InfoLogLength, NULL, &Buffer[0]);
					fprintf(stdout, "%s\n", &Buffer[0]);
				}
			}

			Success = Success && Result == GL_TRUE;
		}
	
		return Success; 
	}

	inline void compiler::clear()
	{
		for
		(
			names_map::iterator ShaderNameIterator = this->ShaderNames.begin(); 
			ShaderNameIterator != this->ShaderNames.end(); 
			++ShaderNameIterator
		)
			glDeleteShader(ShaderNameIterator->second);
		this->ShaderNames.clear();
		this->ShaderFiles.clear();
		this->PendingChecks.clear();
	}

	inline bool compiler::loadBinary
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

	inline std::string compiler::loadFile
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

	inline std::string format(const char* msg, ...)
	{
		std::size_t const STRING_BUFFER(10000);
		char text[STRING_BUFFER];
		va_list list;

		if(msg == 0)
			return std::string();

		va_start(list, msg);
			vsprintf(text, msg, list);
		va_end(list);

		return std::string(text);
	}

	inline bool validateProgram(GLuint ProgramName)
	{
		if(!ProgramName)
			return false;

		glValidateProgram(ProgramName);
		GLint Result = GL_FALSE;
		glGetProgramiv(ProgramName, GL_VALIDATE_STATUS, &Result);

		if(Result == GL_FALSE)
		{
			fprintf(stdout, "Validate program\n");
			int InfoLogLength;
			glGetProgramiv(ProgramName, GL_INFO_LOG_LENGTH, &InfoLogLength);
			if(InfoLogLength > 0)
			{
				std::vector<char> Buffer(InfoLogLength);
				glGetProgramInfoLog(ProgramName, InfoLogLength, NULL, &Buffer[0]);
				fprintf(stdout, "%s\n", &Buffer[0]);
			}
		}

		return Result == GL_TRUE;
	}

	inline bool checkProgram(GLuint ProgramName)
	{
		if(!ProgramName)
			return false;

		GLint Result = GL_FALSE;
		glGetProgramiv(ProgramName, GL_LINK_STATUS, &Result);

		fprintf(stdout, "Linking program\n");
		int InfoLogLength;
		glGetProgramiv(ProgramName, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if(InfoLogLength > 0)
		{
			std::vector<char> Buffer(std::max(InfoLogLength, int(1)));
			glGetProgramInfoLog(ProgramName, InfoLogLength, NULL, &Buffer[0]);
			fprintf(stdout, "%s\n", &Buffer[0]);
		}

		return Result == GL_TRUE;
	}

	inline bool checkShader
	(
		GLuint ShaderName, 
		std::string const & File
	)
	{
		if(!ShaderName)
			return false;

		GLint Result = GL_FALSE;
		glGetShaderiv(ShaderName, GL_COMPILE_STATUS, &Result);

		fprintf(stdout, "Compiling shader\n%s...\n", File.c_str());
		int InfoLogLength;
		glGetShaderiv(ShaderName, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if(InfoLogLength > 0)
		{
			std::vector<char> Buffer(InfoLogLength);
			glGetShaderInfoLog(ShaderName, InfoLogLength, NULL, &Buffer[0]);
			fprintf(stdout, "%s\n", &Buffer[0]);
		}

		return Result == GL_TRUE;
	}

	inline GLuint createShader
	(
		GLenum Type,
		std::string const & Source
	)
	{
		return createShader(Type, std::string(), Source);
	}

	inline GLuint createShader
	(
		GLenum Type,
		std::string const & Arguments, 
		std::string const & Source
	)
	{
		GLuint Name = 0;

		if(!Source.empty())
		{
			std::string SourceContent = glf::loadFile(Source);
			char const * SourcePointer = SourceContent.c_str();
			Name = glCreateShader(Type);
			glShaderSource(Name, 1, &SourcePointer, NULL);
			glCompileShader(Name);
		}

		return Name;
	}
}//namespace glf
