#version 330 core

uniform sampler2D Diffuse;

in vec2 VertTexcoord;
out vec4 FragColor;

void main()
{
	float Height = texture2D(Diffuse, VertTexcoord).x;
	FragColor = vec4(VertTexcoord, Height, 1.0);
}
