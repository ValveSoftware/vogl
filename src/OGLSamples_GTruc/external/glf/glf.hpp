#ifndef GLF_INCLUDED
#define GLF_INCLUDED

//#include <glo/glo.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "common.hpp"
#include "vertex.hpp"
#include "compiler.hpp"
#include "sementics.hpp"
#include "error.hpp"
//#include "window.hpp"

namespace glf
{
	enum mouse_button
	{
		MOUSE_BUTTON_NONE = 0,
		MOUSE_BUTTON_LEFT = (1 << 0),
		MOUSE_BUTTON_RIGHT = (1 << 1),
		MOUSE_BUTTON_MIDDLE = (1 << 2)
	};

	struct window
	{
		window(glm::ivec2 const & Size) :
			Size(Size),
			MouseOrigin(Size >> 1),
			MouseCurrent(Size >> 1),
			TranlationOrigin(0, 4),
			TranlationCurrent(0, 4),
			RotationOrigin(0), 
			RotationCurrent(0),
			MouseButtonFlags(0)
		{
			memset(KeyPressed, 0, sizeof(KeyPressed));	
		}

		glm::ivec2 Size;
		glm::vec2 MouseOrigin;
		glm::vec2 MouseCurrent;
		glm::vec2 TranlationOrigin;
		glm::vec2 TranlationCurrent;
		glm::vec2 RotationOrigin;
		glm::vec2 RotationCurrent;
		int MouseButtonFlags;
		std::size_t KeyPressed[512];
	};

	int version(int Major, int Minor);
	int run();

	struct vertexattrib
	{
		vertexattrib() :
			Enabled(GL_FALSE),
			Binding(0),
			Size(4),
			Stride(0),
			Type(GL_FLOAT),
			Normalized(GL_FALSE),
			Integer(GL_FALSE),
			Long(GL_FALSE),
			Divisor(0),
			Pointer(NULL)
		{}

		vertexattrib
		(
			GLint Enabled,
			GLint Binding,
			GLint Size,
			GLint Stride,
			GLint Type,
			GLint Normalized,
			GLint Integer,
			GLint Long,
			GLint Divisor,
			GLvoid* Pointer
		) :
			Enabled(Enabled),
			Binding(Binding),
			Size(Size),
			Stride(Stride),
			Type(Type),
			Normalized(Normalized),
			Integer(Integer),
			Long(Long),
			Divisor(Divisor),
			Pointer(Pointer)
		{}

		GLint Enabled;
		GLint Binding;
		GLint Size;
		GLint Stride;
		GLint Type;
		GLint Normalized;
		GLint Integer;
		GLint Long;
		GLint Divisor;
		GLvoid* Pointer;
	};

	bool operator== (vertexattrib const & A, vertexattrib const & B)
	{
		return A.Enabled == B.Enabled && 
			A.Size == B.Size && 
			A.Stride == B.Stride && 
			A.Type == B.Type && 
			A.Normalized == B.Normalized && 
			A.Integer == B.Integer && 
			A.Long == B.Long;
	}

	bool operator!= (vertexattrib const & A, vertexattrib const & B)
	{
		return !(A == B);
	}

}//namespace glf

namespace 
{
	extern glf::window Window;
}//namespace 

#include "glf.inl"

#endif//GLF_WINDOW_INCLUDED
