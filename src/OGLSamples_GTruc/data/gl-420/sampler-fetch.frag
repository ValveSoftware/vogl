#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

layout(binding = 0) uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

vec4 textureTrilinear(in sampler2D Sampler, in vec2 Texcoord);

void main()
{
	Color = textureTrilinear(Diffuse, In.Texcoord);
}
