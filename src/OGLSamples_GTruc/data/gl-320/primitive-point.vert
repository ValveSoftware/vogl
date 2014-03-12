#version 150 core

uniform mat4 MVP;
uniform mat4 MV;

in vec4 Position;
in vec4 Color;

out block
{
	vec4 Color;
} Out;

void main()
{
	Out.Color = Color;
	gl_Position = MVP * Position;
	gl_PointSize = 1.f / -(MV * Position).z;
}
