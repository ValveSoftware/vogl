#version 400 core

uniform sampler2D Diffuse;
uniform sampler2DShadow Shadow;

in block
{
	vec4 Color;
	vec4 ShadowCoord;
} In;

out vec4 Color;

void main()
{
	vec4 Diffuse = In.Color;

	ivec2 ShadowSize = textureSize(Shadow, 0);
	vec4 ShadowCoord = In.ShadowCoord;
	ShadowCoord.z -= 0.005;

	vec4 Gather = textureGather(Shadow, ShadowCoord.xy, ShadowCoord.z);

	vec2 TexelCoord = ShadowCoord.xy * vec2(ShadowSize.xy);
	vec2 SampleCoord = fract(TexelCoord + 0.5);

	float Texel0 = mix(Gather.x, Gather.y, SampleCoord.x);
	float Texel1 = mix(Gather.w, Gather.z, SampleCoord.x);
	float Visibility = mix(Texel1, Texel0, SampleCoord.y);
	Color = vec4(mix(vec4(0.5), vec4(1.0), Visibility) * Diffuse);
}
