#version 150 core

in block
{
	vec4 Color;
} In;

out vec4 Color;

void main()
{
	Color = In.Color;
}

