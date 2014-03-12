#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define COMMON		0
#define FRAG_COLOR	0

layout(binding = 0) uniform sampler2D Diffuse;

in vec4 gl_FragCoord;
layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	vec2 TextureSize = vec2(textureSize(Diffuse, 0));
	vec2 Texcoord = vec2(gl_FragCoord.x / TextureSize.x, gl_FragCoord.y / TextureSize.y);

	Color = texture(Diffuse, Texcoord);
}
