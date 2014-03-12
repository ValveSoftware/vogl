#version 420 core
#extension GL_ARB_shader_image_size : require

in vec4 gl_FragCoord;
layout(binding = 0, rgba8) uniform coherent image2D Diffuse;

void main()
{
	ivec2 Size = imageSize(Diffuse);

	imageStore(
		Diffuse, 
		ivec2(gl_FragCoord.xy), 
		vec4(vec2(gl_FragCoord.xy) / vec2(Size), 0.0, 1.0) * vec4(1.0, 0.5, 0.0, 1.0));
}
