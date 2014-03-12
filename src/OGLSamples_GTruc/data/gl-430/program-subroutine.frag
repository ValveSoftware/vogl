#version 420 core
#extension GL_ARB_explicit_uniform_location : require

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

#define SAMPLER_DXT1	0
#define SAMPLER_RGB8	1

#define SAMPLING_DXT1	0
#define SAMPLING_RGB8	1

#define MAX_INSTANCE	2

subroutine vec4 diffuse();

layout(location = 0) subroutine uniform diffuse Diffuse[MAX_INSTANCE];
layout(binding = SAMPLER_DXT1) uniform sampler2D DiffuseDXT1;
layout(binding = SAMPLER_RGB8) uniform sampler2D DiffuseRGB8;

in block
{
	vec2 Texcoord;
	flat int Instance;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

layout(index = SAMPLING_DXT1) subroutine(diffuse)
vec4 diffuseLQ()
{
	return texture(DiffuseDXT1, In.Texcoord);
}

layout(index = SAMPLING_RGB8) subroutine(diffuse)
vec4 diffuseHQ()
{
	return texture(DiffuseRGB8, In.Texcoord);
}

void main()
{
	Color = Diffuse[In.Instance]();
}
