#version 150 core

precision highp float;
precision highp int;

const vec4 Diffuse[2] = vec4[2](vec4(1.0, 0.5, 0.0, 1.0), vec4(0.0, 0.5, 1.0, 1.0));

in block
{
	flat int Instance;
} In;

out vec4 Color;

void main()
{
	Color = Diffuse[In.Instance];
}
