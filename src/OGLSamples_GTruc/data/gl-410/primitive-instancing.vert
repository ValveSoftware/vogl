#version 410 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

layout(location = POSITION) in vec2 Position;
layout(location = COLOR) in vec4 Color;
layout(location = COLOR) flat out vec4 VertColor;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{	
	gl_Position = vec4(Position, 0.0, 1.0);
	VertColor = Color;
}

