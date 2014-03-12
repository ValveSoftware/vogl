#version 410 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define COMMON		0
#define VARYING		0
#define FRAG_COLOR	0

precision highp int;

uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;

layout(location = TEXCOORD) out vec2 VertTexcoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{	
	VertTexcoord = Texcoord;
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
}
