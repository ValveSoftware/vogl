#version 420 core
#extension GL_ARB_shader_image_size : require

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

layout(binding = 0, rgba8) uniform image2D Diffuse[3];

in block
{
	vec2 Texcoord;
	flat int Instance;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

vec4 fetchBilinear(layout(rgba8) in image2D Image, in vec2 Interpolant, in ivec2 Texelcoords[4])
{
	vec4 Texel00 = imageLoad(Image, Texelcoords[0]);
	vec4 Texel10 = imageLoad(Image, Texelcoords[1]);
	vec4 Texel11 = imageLoad(Image, Texelcoords[2]);
	vec4 Texel01 = imageLoad(Image, Texelcoords[3]);
	
	vec4 Texel0 = mix(Texel00, Texel01, Interpolant.y);
	vec4 Texel1 = mix(Texel10, Texel11, Interpolant.y);
	return mix(Texel0, Texel1, Interpolant.x);
}

vec4 imageBilinear(layout(rgba8) in image2D Image, in vec2 Texcoord)
{
	//const ivec2 SizeArray[3] = ivec2[3](ivec2(256), ivec2(128), ivec2(64));
	//ivec2 Size = SizeArray[Instance];

	ivec2 Size = imageSize(Image);
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
	
	ivec2 Texelcoords[] = ivec2[4](
		TexelIndex + ivec2(0, 0),
		TexelIndex + ivec2(1, 0),
		TexelIndex + ivec2(1, 1),
		TexelIndex + ivec2(0, 1));

	return fetchBilinear(
		Image, 
		fract(Texelcoord), 
		Texelcoords);
}

void main()
{
	Color = imageBilinear(Diffuse[In.Instance], In.Texcoord);
}
