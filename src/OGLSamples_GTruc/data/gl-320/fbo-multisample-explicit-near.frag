#version 330 core
#define FRAG_COLOR		0

uniform sampler2DMS Diffuse;

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	// integer UV coordinates, needed for fetching multisampled texture
	ivec2 Texcoord = ivec2(textureSize(Diffuse) * In.Texcoord);

	Color = texelFetch(Diffuse, Texcoord, 0);
}
