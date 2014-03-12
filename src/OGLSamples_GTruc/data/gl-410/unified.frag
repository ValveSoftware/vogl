#version 410 core

// Declare all the semantics
#define ATTR_POSITION	0
#define ATTR_COLOR		3
#define ATTR_TEXCOORD	4
#define VERT_COLOR		3
#define VERT_TEXCOORD	4
#define FRAG_COLOR		0

uniform sampler2D Diffuse;

layout(location = VERT_TEXCOORD) in vec2 Texcoord;
layout(location = VERT_COLOR) in vec3 Color;

layout(location = FRAG_COLOR, index = 0) out vec4 FragColor;

void main()
{
	FragColor = texture(Diffuse, Texcoord) * vec4(Color, 1.0);
}
