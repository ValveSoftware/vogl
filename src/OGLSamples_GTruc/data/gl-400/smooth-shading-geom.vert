#version 400 core

#define ATTR_POSITION	0
#define ATTR_COLOR		3
#define ATTR_TEXCOORD	4
#define FRAG_COLOR		0

precision highp float;

uniform mat4 MVP;

layout(location = ATTR_POSITION) in vec2 Position;
layout(location = ATTR_COLOR) in vec4 Color;

out block
{
	vec4 Color;
} Out;

void main()
{	
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
	Out.Color = Color;
}

