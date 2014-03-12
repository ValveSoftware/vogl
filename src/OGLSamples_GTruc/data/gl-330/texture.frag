#version 140

uniform sampler2D Diffuse;

in vec2 VertTexcoord;
out vec4 FragColor;

void main()
{
	FragColor = texture2D(Diffuse, VertTexcoord);
}
