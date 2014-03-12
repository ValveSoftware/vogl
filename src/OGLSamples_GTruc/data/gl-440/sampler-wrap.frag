#version 440 core

#define FRAG_COLOR		0

layout(binding = 0) uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = texture2D(Diffuse, In.Texcoord);
}
