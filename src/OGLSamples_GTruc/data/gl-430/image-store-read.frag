#version 420 core
#extension GL_ARB_shader_image_size : require

#define FRAG_COLOR		0
#define DIFFUSE			0

in vec4 gl_FragCoord;
layout(binding = 0, rgba8) uniform image2D Diffuse;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

const float Border = 16;

void main()
{
	vec2 Size = vec2(imageSize(Diffuse));

	if(gl_FragCoord.x < Border)
		Color = vec4(1.0, 0.0, 0.0, 1.0);
	else if(gl_FragCoord.x > Size.x - Border * 2)
		Color = vec4(0.0, 1.0, 0.0, 1.0);
	else if(gl_FragCoord.y < Border)
		Color = vec4(1.0, 1.0, 0.0, 1.0);
	else if(gl_FragCoord.y > Size.y - Border * 2)
		Color = vec4(0.0, 0.0, 1.0, 1.0);
	else
		Color = imageLoad(Diffuse, ivec2(gl_FragCoord.xy));
}
