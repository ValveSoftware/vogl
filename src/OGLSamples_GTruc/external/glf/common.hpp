#ifndef GLF_COMMON_INCLUDED
#define GLF_COMMON_INCLUDED

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/euler_angles.hpp>

// GLI
#include <gli/gli.hpp>

// STL
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <istream>
#include <ostream>
#include <string>
#include <cstring>
#include <cstdarg>

namespace glf
{
#   if defined(OGL_DATA_DIRECTORY)
		static std::string const DATA_DIRECTORY(OGL_DATA_DIRECTORY);
#	elif defined(WIN32)
		static std::string const DATA_DIRECTORY("../data/");
#	elif defined(__APPLE__)
		static std::string const DATA_DIRECTORY("../../../data/");
#	else
		// For packages.
		static std::string const DATA_DIRECTORY("data/");
#	endif
}//namespace glf

#endif//GLF_COMMON_INCLUDED
