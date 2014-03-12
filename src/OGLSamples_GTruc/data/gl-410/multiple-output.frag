#version 410 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0
#define FRAG_RED		0
#define FRAG_GREEN		1
#define FRAG_BLUE		2
#define FRAG_ALPHA		3

uniform sampler2D Diffuse;

layout(location = TEXCOORD) in vec2 Texcoord;
layout(location = FRAG_RED, index = 0) out float Red;
layout(location = FRAG_GREEN, index = 0) out float Green;
layout(location = FRAG_BLUE, index = 0) out float Blue;

void main()
{
	vec4 Color = texture(Diffuse, Texcoord);
	Red = Color.r;
	Green = Color.g;
	Blue = Color.b;
}
