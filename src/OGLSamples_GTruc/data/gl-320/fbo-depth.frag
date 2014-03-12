#version 150 core

uniform sampler2D Diffuse;

in vec4 gl_FragCoord;
out vec4 Color;

float LinearizeDepth(vec2 uv)
{
	float n = 0.1; // camera z near
	float f = 8.0; // camera z far
	float z = texture(Diffuse, uv).x;
	return (2.0 * n) / (f + n - z * (f - n));
}

void main()
{
	vec2 TextureSize = vec2(textureSize(Diffuse, 0));

	vec2 TexCoord = gl_FragCoord.xy / TextureSize;
	float Depth = texture(Diffuse, TexCoord).x;//LinearizeDepth(TexCoord);
		
	Color = vec4(Depth, Depth, Depth, 1.0);
}



