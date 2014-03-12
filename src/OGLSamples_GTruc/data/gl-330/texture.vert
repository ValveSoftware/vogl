#version 140

uniform mat4 MVP;

in vec2 Position;
in vec2 Texcoord;
out vec2 VertTexcoord;

void main()
{	
	VertTexcoord = Texcoord;
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
}