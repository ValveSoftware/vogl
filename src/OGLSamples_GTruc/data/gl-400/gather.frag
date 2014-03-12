#version 400 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define INSTANCE	7
#define FRAG_COLOR	0

uniform sampler2D Diffuse;

in vert
{
	vec2 Texcoord;
} In;

layout(origin_upper_left) in vec4 gl_FragCoord;
layout(location = FRAG_COLOR, index = 0) out vec3 Color;

vec3 texelAverage(in vec2 Texcoord, in ivec2 Offset)
{
	vec4 Red = textureGatherOffset(Diffuse, Texcoord, Offset, 0);
	vec4 Green = textureGatherOffset(Diffuse, Texcoord, Offset, 1);
	vec4 Blue = textureGatherOffset(Diffuse, Texcoord, Offset, 2);

	vec3 Texel0 = vec3(Red[0], Green[0], Blue[0]); 
	vec3 Texel1 = vec3(Red[1], Green[1], Blue[1]); 
	vec3 Texel2 = vec3(Red[2], Green[2], Blue[2]); 
	vec3 Texel3 = vec3(Red[3], Green[3], Blue[3]); 

	return (Texel0 + Texel1 + Texel2 + Texel3) * 0.25;
}

void main()
{
	vec2 Size = textureSize(Diffuse, 0) - 1;
	vec2 Texcoord = In.Texcoord * Size;
	ivec2 Coord = ivec2(In.Texcoord * Size);
	
	Color = vec3(0);
	Color += texelAverage(In.Texcoord, ivec2( 8, 0));
	Color += texelAverage(In.Texcoord, ivec2( 0, 8));
	Color += texelAverage(In.Texcoord, ivec2(-8, 0));
	Color += texelAverage(In.Texcoord, ivec2( 0,-8));
	Color *= 0.25f;
}
