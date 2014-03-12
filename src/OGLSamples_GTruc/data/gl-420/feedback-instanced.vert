#version 330 core

#define ATTR_POSITION	0
#define ATTR_COLOR		3
#define VERT_COLOR		3
#define FRAG_COLOR		0

layout(location = ATTR_POSITION) in vec4 Position;
layout(location = ATTR_COLOR) in vec4 Color;

out vec4 VertColor;

void main()
{	
	gl_Position = Position;
	VertColor = Color;
}

