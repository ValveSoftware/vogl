#version 400 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

uniform transform
{
	mat4 MVP;
} Transform[2];

layout(location = POSITION) in vec2 Position;

void main()
{	
	gl_Position = Transform[gl_InstanceID].MVP * vec4(Position, 0.0, 1.0);
}
