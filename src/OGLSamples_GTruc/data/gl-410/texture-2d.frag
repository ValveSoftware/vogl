#version 410 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define COMMON		0
#define VARYING		0
#define FRAG_COLOR	0

precision highp int;

uniform sampler2D Diffuse;

layout(location = TEXCOORD) in vec2 Texcoord;
layout(location = FRAG_COLOR, index = 0) out vec4 FragColor;

void main()
{
	FragColor = texture(Diffuse, Texcoord);
}
