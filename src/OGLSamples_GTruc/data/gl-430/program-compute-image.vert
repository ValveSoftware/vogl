#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

#define VERTEX		0
#define TRANSFORM0		1

#define SAMPLER_DIFFUSE			0 
#define SAMPLER_POSITION		4 
#define SAMPLER_TEXCOORD		5 
#define SAMPLER_COLOR			6 

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(binding = SAMPLER_POSITION) uniform samplerBuffer Position;
layout(binding = SAMPLER_TEXCOORD) uniform samplerBuffer Texcoord;
layout(binding = SAMPLER_COLOR) uniform samplerBuffer Color;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec4 Texcoord;
	vec4 Color;
} Out;

void main()
{	
	Out.Texcoord = texelFetch(Texcoord, gl_VertexID);
	Out.Color = texelFetch(Color, gl_VertexID);
	gl_Position = texelFetch(Position, gl_VertexID);
}
