#version 330 core

#define FRAG_COLOR		0

uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color0;
layout(location = FRAG_COLOR, index = 1) out vec4 Color1;

void main()
{
	Color0.rgba = texture(Diffuse, In.Texcoord).rgba * 0.5;
	Color1.rgba = texture(Diffuse, In.Texcoord).bgra * 0.5;
}
