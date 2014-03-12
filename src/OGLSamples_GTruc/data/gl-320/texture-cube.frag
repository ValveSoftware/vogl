#version 150 core

uniform samplerCube Environment;

in block
{
	vec3 Reflect;
} In;

out vec4 Color;

void main()
{
	Color = texture(Environment, In.Reflect);
}
