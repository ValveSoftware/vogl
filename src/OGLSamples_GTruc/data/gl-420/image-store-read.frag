#version 420 core

#define FRAG_COLOR		0
#define DIFFUSE			0

in vec4 gl_FragCoord;
layout(binding = 0, rgba8) coherent uniform image2D Diffuse;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = imageLoad(Diffuse, ivec2(gl_FragCoord.xy));
}
