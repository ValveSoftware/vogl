#version 420 core

#include "texture-storage.glsl"
#line 5

#define COUNT 24

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
	mediump vec4 Lumimance[COUNT];
} Out;

void main()
{
/*
	mediump int A = int(0);
	lowp float B = float(A);
	highp int C = int(B);
*/
	for(int i = 0; i < int(COUNT); ++i)
		Out.Lumimance[i] = vec4(1.0) / vec4(COUNT);

	Out.Texcoord = Texcoord;
	gl_Position = Transform.MVP * vec4(Position, 0.0, 1.0);
}
