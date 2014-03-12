#version 330 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define INSTANCE		7
#define FRAG_COLOR		0

uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;
layout(location = COLOR) in vec4 Color;

out block
{
	flat vec4 Color;
} Out;

void main()
{	
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
	Out.Color = Color;
}

