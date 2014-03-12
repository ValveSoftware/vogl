#version 410 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

uniform sampler2D Diffuse;

layout(location = COLOR) in vec3 Color;
layout(location = TEXCOORD) in vec2 Texcoord;
layout(location = FRAG_COLOR, index = 0) out vec4 FragColor;

void main()
{
	FragColor = texture(Diffuse, Texcoord) * vec4(Color, 1.0);
}
