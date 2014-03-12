#version 400 core

#define FRAG_COLOR		0

uniform sampler2DArray Diffuse;
uniform int Layer;

in vec4 gl_FragCoord;
layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	vec2 TextureSize = vec2(textureSize(Diffuse, 0).xy);

	Color = texture(Diffuse, vec3(gl_FragCoord.xy / TextureSize, Layer));
	//Color = texelFetch(Diffuse, ivec2(gl_FragCoord.xy), 0);
}
