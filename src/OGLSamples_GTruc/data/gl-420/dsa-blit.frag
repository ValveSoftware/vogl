#version 420 core

#define FRAG_COLOR		0

uniform sampler2DMS Diffuse;

in vec4 gl_FragCoord;
layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	ivec2 Texcoord = ivec2(textureSize(Diffuse) * gl_FragCoord);

	vec4 Temp = vec4(0.0);
	
	// For each of the 4 samples
	for(int i = 0; i < 4; ++i)
		Temp += texelFetch(Diffuse, Texcoord, i);

	Color = Temp * 0.25;
}
