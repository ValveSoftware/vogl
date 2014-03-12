#version 420 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

#define MATERIAL	0
#define TRANSFORM0	1

in block
{
	vec2 Texcoord;
	flat uint DrawID;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

layout(binding = 0) uniform sampler2D Diffuse[3];

void main()
{
	Color = texture(Diffuse[In.DrawID], In.Texcoord.st);
}

