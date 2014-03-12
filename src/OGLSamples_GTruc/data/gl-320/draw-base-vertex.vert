#version 150 core

uniform mat4 MVP;

in vec3 Position;
in vec4 Color;

out block
{
	flat int Index;
	vec4 Color;
} Out;

void main()
{
	gl_Position = MVP * vec4(Position, 1.0);
	Out.Color = Color;
	Out.Index = gl_VertexID / 4;
}

