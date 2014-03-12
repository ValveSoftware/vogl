#version 150 core

layout(std140, column_major) uniform;

uniform transform
{
	mat4 MVP;
} Transform;

in vec2 Position;

void main()
{	
	gl_Position = Transform.MVP * vec4(Position, 0.0, 1.0);
}
