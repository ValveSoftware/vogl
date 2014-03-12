#version 420 core

#define POSITION		0
#define COLOR			3
#define FRAG_COLOR		0

layout(location = 0) in vec4 Color;
layout(location = FRAG_COLOR, index = 0) out vec4 FragColor;

void main()
{
	FragColor = Color;
}
