#version 150 core

uniform mat4 MVP;
uniform mat4 MV;

in vec2 Position;
in vec4 Color;

out block
{
	vec4 Color;
} Out;

void main()
{
	Out.Color = Color;
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
	gl_PointSize = 256.0 / -(MV * vec4(Position, 0.0, 1.0)).z;
}
