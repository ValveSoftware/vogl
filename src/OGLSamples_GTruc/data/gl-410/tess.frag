#version 410 core

#define POSITION		0
#define COLOR			3
#define FRAG_COLOR		0

struct vertex
{
	vec4 Color;
};

layout(location = 0) in vertex In;
layout(location = FRAG_COLOR, index = 0) out vec4 FragColor;

void main()
{
	FragColor = In.Color;
}
