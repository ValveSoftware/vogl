#version 150 core

uniform sampler2D Diffuse;

in vec4 gl_FragCoord;
out vec4 Color;

void main()
{
	vec2 TextureSize = vec2(textureSize(Diffuse, 0));

	//Color = texture(Diffuse, gl_FragCoord.xy / TextureSize);
	Color = texelFetch(Diffuse, ivec2(gl_FragCoord.xy), 0);
}
