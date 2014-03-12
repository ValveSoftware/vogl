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

	Color = vec4(IntColor.rgb, 1023.0) / 1023.0;
}
