#version 410 core
#extension GL_NV_gpu_shader5 : enable
#extension GL_EXT_shader_image_load_store : enable

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define COMMON		0
#define VARYING		0
#define FRAG_COLOR	0

layout(size1x32) coherent uniform uimage2D ImageData;
uniform uvec2 ImageSize;

struct vertex
{
	vec2 Texcoord;
};

layout(location = VARYING) in vertex In;
layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	unsigned int Fetch = imageLoad(ImageData, ivec2(In.Texcoord * ImageSize)).x;
	Color = unpackUnorm4x8(Fetch);
}
