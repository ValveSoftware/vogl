#version 400 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

uniform dmat4 MVP;

layout(location = POSITION) in vec2 Position;

void main()
{	
	gl_Position = vec4(MVP * dvec4(Position, 0.0, 1.0));
}

