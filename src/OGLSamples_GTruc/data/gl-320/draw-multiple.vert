#version 150 core

uniform mat4 MVP;

in vec4 Position;

void main()
{
	gl_Position = MVP * Position;
}

