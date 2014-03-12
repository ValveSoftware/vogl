#version 150 core

uniform usampler2D Diffuse;

in block
{
	vec2 Texcoord;
} In;

out vec4 Color;

void main()
{
	uvec4 IntColor = texture(Diffuse, In.Texcoord);

	Color = vec4(IntColor) / 255.0;
}
