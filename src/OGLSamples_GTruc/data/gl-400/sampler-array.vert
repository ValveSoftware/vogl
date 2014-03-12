#version 400 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define INSTANCE	7
#define FRAG_COLOR	0

uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;

out block
{
	vec2 Texcoord;
	float Instance;
} Out;

void main()
{	
	Out.Texcoord = Texcoord;
	Out.Instance = float(gl_InstanceID);
	gl_Position = MVP * vec4(Position, float(gl_InstanceID) * 0.5, 1.0);
}