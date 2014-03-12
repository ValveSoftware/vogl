#version 150 core

precision highp float;

in block
{
	vec4 Color;
} In;

out vec4 Color;

void main()
{
	Color = In.Color;
}

