#version 440 core

#define POSITION	0
#define COLOR		3
#define FRAG_COLOR	0

layout(location = POSITION) in vec4 Position;
layout(location = COLOR) in vec4 Color;

out block
{
	vec4 Color;
} Out;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{	
	gl_Position = Position;
	Out.Color = Color;
}

