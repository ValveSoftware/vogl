#version 150 core

precision mediump float;
precision highp int;

#define COUNT 4

uniform transform
{
	mat4 MVP;
} Transform;

in vec2 Position;
in vec2 Texcoord;

out block
{
	vec2 Texcoord;
	vec4 Lumimance[COUNT];
} Out;

void main()
{
	lowp int Count = int(COUNT);

	for(lowp int i = 0; i < Count; ++i)
		Out.Lumimance[i] = vec4(1.0) / vec4(COUNT);

	Out.Texcoord = Texcoord;
	gl_Position = Transform.MVP * vec4(Position, 0.0, 1.0);
}
