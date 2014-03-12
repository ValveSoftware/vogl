#ifndef GLF_ERROR_INCLUDED
#define GLF_ERROR_INCLUDED

#include "common.hpp"

namespace glf
{
	bool checkError(const char* Title);
	bool checkFramebuffer(GLuint FramebufferName);
}//namespace glf

#include "error.inl"

#endif//GLF_ERROR_INCLUDED
