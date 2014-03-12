#version 150 core

uniform sampler2D Diffuse;

in block
{
	vec4 Color;
} In;

out vec4 Color;

void main()
{
	Color = In.Color * texture(Diffuse, gl_PointCoord);
}

