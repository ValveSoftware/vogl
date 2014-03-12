#version 400 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0
#define FRAG_RED		0
#define FRAG_GREEN		1
#define FRAG_BLUE		2
#define FRAG_ALPHA		3

uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;

out vert
{
	vec2 Texcoord;
} Vert;

void main()
{	
	Vert.Texcoord = Texcoord;
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
}