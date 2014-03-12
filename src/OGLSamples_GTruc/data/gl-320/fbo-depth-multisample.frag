#version 150 core

uniform sampler2DMS Diffuse;

in vec4 gl_FragCoord;
out vec4 Color;

float LinearizeDepth(float Depth)
{
	float n = 0.1; // camera z near
	float f = 8.0; // camera z far
	return (2.0 * n) / (f + n - Depth * (f - n));
}

void main()
{
	float Depth = texelFetch(Diffuse, ivec2(gl_FragCoord.xy), 0).r;
	for(int i = 1; i < 4; ++i)
		Depth = min(Depth, texelFetch(Diffuse, ivec2(gl_FragCoord.xy), i).r);

	float LinearDepth = LinearizeDepth(Depth);
		
	Color = vec4(LinearDepth, LinearDepth, LinearDepth, 1.0);
}



