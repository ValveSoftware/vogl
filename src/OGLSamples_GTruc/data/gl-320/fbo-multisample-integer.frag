#version 150 core

uniform usampler2D Diffuse;

in block
{
	vec2 Texcoord;
} In;

out uvec4 Color;

void main()
{
	Color = texture(Diffuse, In.Texcoord);
}
