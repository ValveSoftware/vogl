#version 130

uniform sampler2D Diffuse;

in vec2 VertTexcoord;

out vec4 Color;

void main()
{
	Color = texture(Diffuse, VertTexcoord);
}
