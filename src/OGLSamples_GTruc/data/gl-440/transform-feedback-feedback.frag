#version 440 core

#define POSITION	0
#define COLOR		3
#define FRAG_COLOR	0

in block
{
	vec4 Color;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = In.Color;
}

