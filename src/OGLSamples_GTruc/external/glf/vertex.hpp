#ifndef GLF_VERTEX_INCLUDED
#define GLF_VERTEX_INCLUDED

#include "common.hpp"

namespace glf
{
	struct vertex_v2fv2f
	{
		vertex_v2fv2f
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

	struct vertex_v3fv2f
	{
		vertex_v3fv2f
		(
			glm::vec3 const & Position,
			glm::vec2 const & Texcoord
		) :
			Position(Position),
			Texcoord(Texcoord)
		{}

		glm::vec3 Position;
		glm::vec2 Texcoord;
	};

	struct vertex_v3fv4u8
	{
		vertex_v3fv4u8
		(
			glm::vec3 const & Position,
			glm::u8vec4 const & Color
		) :
			Position(Position),
			Color(Color)
		{}

		glm::vec3 Position;
		glm::u8vec4 Color;
	};

	struct vertex_v2fv3f
	{
		vertex_v2fv3f
		(
			glm::vec2 const & Position,
			glm::vec3 const & Texcoord
		) :
			Position(Position),
			Texcoord(Texcoord)
		{}

		glm::vec2 Position;
		glm::vec3 Texcoord;
	};

	struct vertex_v3fv3f
	{
		vertex_v3fv3f
		(
			glm::vec3 const & Position,
			glm::vec3 const & Texcoord
		) :
			Position(Position),
			Texcoord(Texcoord)
		{}

		glm::vec3 Position;
		glm::vec3 Texcoord;
	};

	struct vertex_v3fv3fv1i
	{
		vertex_v3fv3fv1i
		(
			glm::vec3 const & Position,
			glm::vec3 const & Texcoord,
			int const & DrawID
		) :
			Position(Position),
			Texcoord(Texcoord),
			DrawID(DrawID)
		{}

		glm::vec3 Position;
		glm::vec3 Texcoord;
		int DrawID;
	};

	struct vertex_v4fv2f
	{
		vertex_v4fv2f
		(
			glm::vec4 const & Position,
			glm::vec2 const & Texcoord
		) :
			Position(Position),
			Texcoord(Texcoord)
		{}

		glm::vec4 Position;
		glm::vec2 Texcoord;
	};

	struct vertex_v2fc4f
	{
		vertex_v2fc4f
		(
			glm::vec2 const & Position,
			glm::vec4 const & Color
		) :
			Position(Position),
			Color(Color)
		{}

		glm::vec2 Position;
		glm::vec4 Color;
	};

	struct vertex_v2fc4d
	{
		vertex_v2fc4d
		(
			glm::vec2 const & Position,
			glm::dvec4 const & Color
		) :
			Position(Position),
			Color(Color)
		{}

		glm::vec2 Position;
		glm::dvec4 Color;
	};

	struct vertex_v4fc4f
	{
		vertex_v4fc4f
		(
			glm::vec4 const & Position,
			glm::vec4 const & Color
		) :
			Position(Position),
			Color(Color)
		{}

		glm::vec4 Position;
		glm::vec4 Color;
	};

	struct vertex_v2fc4ub
	{
		vertex_v2fc4ub
		(
			glm::vec2 const & Position,
			glm::u8vec4 const & Color
		) :
			Position(Position),
			Color(Color)
		{}

		glm::vec2 Position;
		glm::u8vec4 Color;
	};

	struct vertex_v2fv2fv4ub
	{
		vertex_v2fv2fv4ub
		(
			glm::vec2 const & Position,
			glm::vec2 const & Texcoord,
			glm::u8vec4 const & Color
		) :
			Position(Position),
			Texcoord(Texcoord),
			Color(Color)
		{}

		glm::vec2 Position;
		glm::vec2 Texcoord;
		glm::u8vec4 Color;
	};

	struct vertex_v2fv2fv4f
	{
		vertex_v2fv2fv4f
		(
			glm::vec2 const & Position,
			glm::vec2 const & Texcoord,
			glm::vec4 const & Color
		) :
			Position(Position),
			Texcoord(Texcoord),
			Color(Color)
		{}

		glm::vec2 Position;
		glm::vec2 Texcoord;
		glm::vec4 Color;
	};

	struct vertex_v4fv4fv4f
	{
		vertex_v4fv4fv4f
		(
			glm::vec4 const & Position,
			glm::vec4 const & Texcoord,
			glm::vec4 const & Color
		) :
			Position(Position),
			Texcoord(Texcoord),
			Color(Color)
		{}

		glm::vec4 Position;
		glm::vec4 Texcoord;
		glm::vec4 Color;
	};

}//namespace glf

#endif//GLF_VERTEX_INCLUDED
