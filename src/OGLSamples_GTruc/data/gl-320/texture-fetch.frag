#version 150 core

uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
} In;

out vec4 Color;

void main()
{
	vec2 Size = textureSize(Diffuse, 0) - 1;
	vec2 Texcoord = In.Texcoord * Size;
	ivec2 Coord = ivec2(In.Texcoord * Size);
	
	vec4 Texel00 = texelFetch(Diffuse, Coord + ivec2(0, 0), 0);
	vec4 Texel10 = texelFetch(Diffuse, Coord + ivec2(1, 0), 0);
	vec4 Texel11 = texelFetch(Diffuse, Coord + ivec2(1, 1), 0);
	vec4 Texel01 = texelFetch(Diffuse, Coord + ivec2(0, 1), 0);
	
	vec2 SampleCoord = fract(Texcoord.xy);

	vec4 Texel0 = mix(Texel00, Texel01, SampleCoord.y);
	vec4 Texel1 = mix(Texel10, Texel11, SampleCoord.y);
	Color = mix(Texel0, Texel1, SampleCoord.x);
}
