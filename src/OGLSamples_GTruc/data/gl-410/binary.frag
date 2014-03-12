#version 410 core

#define FRAG_COLOR	0

uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
	vec3 Color;
} In; 

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = texture(Diffuse, In.Texcoord) * vec4(In.Color, 1.0);
}
