#version 420 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define INSTANCE		7
#define FRAG_COLOR		0
#define FRAG_RED		0
#define FRAG_GREEN		1
#define FRAG_BLUE		2
#define FRAG_ALPHA		3

#define TRANSFORM0		1

#define DIFFUSE			0

layout(binding = DIFFUSE) uniform sampler2D Diffuse;

in vert
{
	vec2 Texcoord;
} In;

layout(location = FRAG_RED) out float Red;
layout(location = FRAG_GREEN) out float Green;
layout(location = FRAG_BLUE) out float Blue;

void main()
{
	vec4 Color = texture(Diffuse, In.Texcoord);
	Red = Color.r;
	Green = Color.g;
	Blue = Color.b;
}
