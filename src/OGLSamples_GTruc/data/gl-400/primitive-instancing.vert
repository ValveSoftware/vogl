#version 400 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0
#define FRAG_RED		0
#define FRAG_GREEN		1
#define FRAG_BLUE		2
#define FRAG_ALPHA		3

layout(location = POSITION) in vec3 Position;

out block
{
	vec3 Color;
} Out;

void main()
{	
	gl_Position = vec4(Position, 1.0);
	Out.Color = vec3(1.0, 0.5, 0.0);
}

