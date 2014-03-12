#version 330 core

#define POSITION	0

precision highp float;
precision highp int;

uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;

out block
{
	float Instance;
} Out;

void main()
{	
	gl_Position = MVP * vec4(Position, float(gl_InstanceID) * 0.25 - 0.5, 1.0);
	Out.Instance = float(gl_InstanceID);
}
