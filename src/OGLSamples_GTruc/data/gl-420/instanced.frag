#version 330 core

#define FRAG_COLOR		0

precision highp float;
precision highp int;

uniform vec4 Diffuse;

in block
{
	float Instance;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = Diffuse * In.Instance * 0.25;
}
