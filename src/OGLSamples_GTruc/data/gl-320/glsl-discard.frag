#version 150 core

uniform sampler2D Diffuse;

in block
{
	vec2 Texcoord;
} In;

out vec4 Color;

void main()
{
	vec4 Temp = texture(Diffuse, In.Texcoord);
	if(Temp.a < 0.2)
		discard;
	Color = Temp;
}
