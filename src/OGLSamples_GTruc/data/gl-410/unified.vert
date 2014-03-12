#version 410 core

// Declare all the semantics
#define ATTR_POSITION	0
#define ATTR_COLOR		3
#define ATTR_TEXCOORD	4
#define VERT_COLOR		3
#define VERT_TEXCOORD	4
#define FRAG_COLOR		0

uniform mat4 MVP;

layout(location = ATTR_POSITION) in vec2 Position;
layout(location = ATTR_TEXCOORD) in vec2 Texcoord;

layout(location = VERT_TEXCOORD) out vec2 VertTexcoord;
layout(location = VERT_COLOR) out vec3 VertColor;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{	
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
	VertTexcoord = Texcoord;
	VertColor = vec3(1.0, 0.9, 0.8);
}

