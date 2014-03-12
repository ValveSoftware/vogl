#version 150

precision highp int;

uniform samplerBuffer Diffuse;

in block
{
	flat int Instance;
} In;

out vec4 Color;

void main()
{
	Color = texelFetch(Diffuse, In.Instance);
}
