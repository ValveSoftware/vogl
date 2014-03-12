#ifndef GLF_SEMENTICS_INCLUDED
#define GLF_SEMENTICS_INCLUDED

namespace glf{
namespace semantic
{
	namespace buffer
	{
		enum type
		{
			STATIC = 0,
			DYNAMIC = 1
		};
	};

	namespace uniform
	{
		enum type
		{
			MATERIAL = 0,
			TRANSFORM0 = 1,
			TRANSFORM1 = 2,
			INDIRECTION = 3
		};
	};

	namespace sampler
	{
		enum type
		{
			DIFFUSE		= 0,
			POSITION	= 4,
			TEXCOORD	= 5,
			COLOR		= 6
		};
	}//namespace sampling

	namespace image
	{
		enum type
		{
			DIFFUSE = 0,
			PICKING = 1
		};
	}//namesapce image

	namespace attr
	{
		enum type
		{
			POSITION = 0,
			COLOR	 = 3,
			TEXCOORD = 4,
			DRAW_ID  = 5
		};
	}//namespace attr

	namespace vert
	{
		enum type
		{
			POSITION = 0,
			COLOR	 = 3,
			TEXCOORD = 4,
			INSTANCE = 7
		};
	}//namespace vert

	namespace frag
	{
		enum type
		{
			COLOR	= 0,
			RED		= 0,
			GREEN	= 1,
			BLUE	= 2,
			ALPHA	= 0
		};
	}//namespace frag

	namespace renderbuffer
	{
		enum type
		{
			DEPTH,
			COLOR0
		};
	}//namespace renderbuffer

}//namespace semantic
}//namespace glf

#endif//GLF_SEMENTICS_INCLUDED
