#version 150 core

uniform sampler2DArray Diffuse;
uniform int Layer;

in block
{
	vec2 Texcoord;
} In;

out vec4 Color;

void main()
{
	Color = texture(Diffuse, vec3(In.Texcoord, float(Layer)));
}
