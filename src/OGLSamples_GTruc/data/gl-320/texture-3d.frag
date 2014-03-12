#version 150 core

in vec4 gl_FragCoord;

uniform sampler3D Diffuse;
uniform mat3 Orientation;

out vec4 Color;

void main()
{
	vec3 SNorm = Orientation * vec3(gl_FragCoord.xy / vec2(640, 480) - 0.5, 0.0);

	Color = texture(Diffuse, SNorm * 0.707106781 + 0.5);
}
