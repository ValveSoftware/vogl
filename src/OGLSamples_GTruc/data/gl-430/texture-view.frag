#version 420 core
#extension GL_ARB_texture_query_levels : require
#extension GL_ARB_fragment_layer_viewport : require

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

layout(binding = 0) uniform sampler2DArray Diffuse[2];

vec4 textureNearest(
	in sampler2DArray Sampler, in vec2 Texcoord)
{
	int LodNearest = int(round(textureQueryLod(Sampler, Texcoord).x));

	ivec2 TextureSize = textureSize(Sampler, LodNearest).xy;
	ivec2 Texelcoord = ivec2(TextureSize * Texcoord);

	return texelFetch(Sampler, ivec3(Texelcoord, 0), LodNearest);
}

vec4 fetchBilinear(
	in sampler2DArray Sampler, in vec2 Interpolant, in ivec2 Texelcoords[4], in int Lod)
{
	vec4 Texel00 = texelFetch(Sampler, ivec3(Texelcoords[0], 0), Lod);
	vec4 Texel10 = texelFetch(Sampler, ivec3(Texelcoords[1], 0), Lod);
	vec4 Texel11 = texelFetch(Sampler, ivec3(Texelcoords[2], 0), Lod);
	vec4 Texel01 = texelFetch(Sampler, ivec3(Texelcoords[3], 0), Lod);
	
	vec4 Texel0 = mix(Texel00, Texel01, Interpolant.y);
	vec4 Texel1 = mix(Texel10, Texel11, Interpolant.y);
	return mix(Texel0, Texel1, Interpolant.x);
}

vec4 textureBilinear(
	in sampler2DArray Sampler, in vec2 Texcoord)
{
	int Lod = int(round(textureQueryLod(Sampler, Texcoord).x));

	ivec2 Size = textureSize(Sampler, Lod).xy;
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
	
	ivec2 Texelcoords[] = ivec2[4](
		TexelIndex + ivec2(0, 0),
		TexelIndex + ivec2(1, 0),
		TexelIndex + ivec2(1, 1),
		TexelIndex + ivec2(0, 1));

	return fetchBilinear(
		Sampler, 
		fract(Texelcoord), 
		Texelcoords, 
		Lod);
}

vec4 textureBilinearLod(
	in sampler2DArray Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod).xy;
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
	
	ivec2 Texelcoords[] = ivec2[4](
		TexelIndex + ivec2(0, 0),
		TexelIndex + ivec2(1, 0),
		TexelIndex + ivec2(1, 1),
		TexelIndex + ivec2(0, 1));

	return fetchBilinear(
		Sampler, 
		fract(Texelcoord), 
		Texelcoords, 
		Lod);
}

vec4 textureTrilinear(
	in sampler2DArray Sampler, in vec2 Texcoord)
{
	float Lod = textureQueryLod(Sampler, Texcoord).x;

	int lodMin = int(floor(Lod));
	int lodMax = int(ceil(Lod));

	vec4 TexelMin = textureBilinearLod(Sampler, Texcoord, lodMin);
	vec4 TexelMax = textureBilinearLod(Sampler, Texcoord, lodMax);

	return mix(TexelMin, TexelMax, fract(Lod));
}

in block
{
	vec2 Texcoord;
	flat mediump int Instance;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	if(textureQueryLevels(Diffuse[gl_ViewportIndex]) == 1)
		Color = textureNearest(Diffuse[gl_ViewportIndex], In.Texcoord.st);
	else
		Color = textureTrilinear(Diffuse[gl_ViewportIndex], In.Texcoord.st);
}
