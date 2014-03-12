#version 420 core
#extension GL_NV_explicit_multisample : require

#define FRAG_COLOR		0

layout(binding = 0) uniform samplerRenderbuffer Diffuse;

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	// integer UV coordinates, needed for fetching multisampled texture
	ivec2 Texcoord = ivec2(textureSizeRenderbuffer(Diffuse) * In.Texcoord);

	vec4 Temp = vec4(0.0);
	
	// For each of the 4 samples
	for(int i = 0; i < 4; ++i)
		Temp += texelFetchRenderbuffer(Diffuse, Texcoord, i);

	Color = Temp * 0.25;
}
