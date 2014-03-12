#version 150 core

uniform mat4 MVP;

in vec2 Position;

void main()
{	
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
}
