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

#ifndef GLF_COMPILER_INCLUDED
#define GLF_COMPILER_INCLUDED

#include "common.hpp"

namespace glf
{
	std::string format(const char* msg, ...);
	std::string loadFile(std::string const & Filename);
	GLuint createShader(GLenum Type, std::string const & Source);
	GLuint createShader(GLenum Type, std::string const & Arguments, std::string const & Source);
	bool checkError(const char* Title);
	bool checkProgram(GLuint ProgramName);
	bool checkShader(GLuint ShaderName, std::string const & File);
	bool validateProgram(GLuint ProgramName);

	class compiler
	{
		typedef std::map<std::string, GLuint> names_map;
		typedef std::map<GLuint, std::string> files_map;

		class commandline
		{
			enum profile
			{
				CORE,
				COMPATIBILITY
			};

		public:
			commandline(std::string const & Filename, std::string const & Arguments);

			void parseArguments(std::string const & Arguments);

			int getVersion() const {return this->Version;}
			std::string getProfile() const {return this->Profile;}
			std::string getDefines() const;
			std::vector<std::string> getIncludes() const {return this->Includes;}

		private:
			std::string Profile;
			int Version;
			std::vector<std::string> Defines;
			std::vector<std::string> Includes;
		};

		class parser
		{
		public:
			std::string operator() (
				commandline const & CommandLine,
				std::string const & Filename) const;

		private:
			std::string parseInclude(std::string const & Line, std::size_t const & Offset) const;
		};

	public:
		~compiler();

		GLuint create(GLenum Type, std::string const & Filename, std::string const & Arguments = std::string());
		bool destroy(GLuint const & Name);

		bool check();
		// TODO: Not defined
		bool check(GLuint const & Name);
		void clear();

	private:
		bool loadBinary(
			std::string const & Filename,
			GLenum & Format,
			std::vector<glm::byte> & Data,
			GLint & Size);
		std::string loadFile(
			std::string const & Filename);

		names_map ShaderNames;
		files_map ShaderFiles;
		names_map PendingChecks;
	};

}//namespace glf

#include "compiler.inl"

#endif//GLF_COMPILER_INCLUDED
