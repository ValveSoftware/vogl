#version 150 core

in vec4 Position;
in vec4 Color;

out block
{
	vec4 Color;
} Out;

void main()
{	
	gl_Position = Position;
	Out.Color = Color;
}

