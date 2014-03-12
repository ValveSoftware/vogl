#version 150 core

const float Luminance[2] = float[2](1.0, 0.2);

in block
{
	flat int Index;
	vec4 Color;
} In;

out vec4 Color;

void main()
{
	Color = In.Color * Luminance[In.Index];
}
