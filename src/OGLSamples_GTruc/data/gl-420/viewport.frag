#version 410 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define COMMON		0
#define FRAG_COLOR	0

precision highp int;

uniform sampler2DArray Diffuse;

layout(location = COMMON) in block
{
	vec2 Texcoord;
	flat int Instance;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = texture(Diffuse, vec3(In.Texcoord, In.Instance));
}
