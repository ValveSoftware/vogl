#version 400 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define INSTANCE	7
#define FRAG_COLOR	0

uniform sampler2D Diffuse[2];
uniform uint DiffuseIndex;

in block
{
	vec2 Texcoord;
	float Instance;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

vec4 sampling(in sampler2D Sampler[2], in vec2 Texcoord)
{
	return texture(Sampler[DiffuseIndex], Texcoord);
}

void main()
{
	Color = sampling(Diffuse, In.Texcoord);
}
