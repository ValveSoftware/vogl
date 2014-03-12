#version 330 core

uniform mat4 MVP;

in vec4 Position;
out vec4 VertColor;

void main()
{	
	VertColor = Position;
	gl_Position = MVP * Position;
}