#version 420 core

#define FRAG_COLOR	0

layout(binding = 0) uniform sampler2D Diffuse;

in vec4 gl_FragCoord;
layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = texelFetch(Diffuse, ivec2(gl_FragCoord.xy * 0.125), 0);
}
