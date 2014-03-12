#version 420 core

#include "texture-storage.glsl"
#line 5

#define COUNT 24

layout(binding = DIFFUSE) uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
	mediump vec4 Lumimance[COUNT];
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	int Init = 0;

	vec4 Luminance = vec4(0.0);
	for(uint i = Init; i < COUNT; ++i)
		Luminance += In.Lumimance[i];

#	ifdef FLAT_COLOR
		Color = vec4(0.0, 0.5, 1.0, 1.0) * Luminance;
#	else
		Color = texture(Diffuse, In.Texcoord.st) * Luminance;
#endif//
}
