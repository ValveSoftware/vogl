#version 150 core

uniform transform
{
	mat4 MVP;
} Transform;

in vec4 Position;

void main()
{
	gl_Position = Transform.MVP * Position;
}

