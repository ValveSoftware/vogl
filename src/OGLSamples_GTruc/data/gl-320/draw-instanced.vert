#version 150 core

precision highp float;
precision highp int;
layout(std140, column_major) uniform;

uniform transform
{
	mat4 MVP[2];
} Transform;

in vec2 Position;

out block
{
	flat int Instance;
} Out;

void main()
{
	gl_Position = Transform.MVP[gl_InstanceID] * vec4(Position, 0.0, 1.0);
	Out.Instance = gl_InstanceID;
}
