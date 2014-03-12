#version 330 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

struct material
{
	sampler2D Diffuse[2];
	vec4 Color;
};

uniform material Material;

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	Color = (texture(Material.Diffuse[0], In.Texcoord) + texture(Material.Diffuse[1], In.Texcoord) + Material.Color) * 0.33;
}
