#version 400 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0
#define FRAG_RED		0
#define FRAG_GREEN		1
#define FRAG_BLUE		2
#define FRAG_ALPHA		3

struct vertex
{
	vec4 Color;
};

in vertex Geom;
layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = Geom.Color;
}
