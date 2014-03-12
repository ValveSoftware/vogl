#version 330 core

uniform mat4 MVP;

in vec2 Position;
out vec2 VertTexcoord;

void main()
{	
	VertTexcoord = Position;
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
}