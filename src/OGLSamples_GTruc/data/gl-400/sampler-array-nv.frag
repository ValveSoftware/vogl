#version 400 core
#extension GL_NV_gpu_shader5 : require

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define INSTANCE	7
#define FRAG_COLOR	0

uniform sampler2D Diffuse[2];

in vert
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	int Index = (int(In.Texcoord.x * 8) + int(In.Texcoord.y * 8)) % 2;
	Color = texture(Diffuse[Index], In.Texcoord);
}
