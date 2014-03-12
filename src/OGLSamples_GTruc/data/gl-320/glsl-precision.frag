#version 150 core

precision mediump float;
precision mediump int;

#define COUNT 4

uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
	vec4 Lumimance[COUNT];
} In;

out vec4 Color;

void main()
{
	highp uint First = uint(0);
	vec4 Luminance = vec4(0.0);

	for(uint i = First; i < uint(COUNT); ++i)
		Luminance += In.Lumimance[i];

	Color = texture(Diffuse, In.Texcoord) * Luminance;
}
