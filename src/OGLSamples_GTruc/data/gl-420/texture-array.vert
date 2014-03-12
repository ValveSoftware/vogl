#version 330 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define INSTANCE		7
#define FRAG_COLOR		0

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
	Out.Instance = gl_InstanceID;
	Out.Texcoord = Texcoord;
	gl_Position = MVP * vec4(Position.x + Out.Instance * 2.4 - 1.2, Position.y, 0.0, 1.0);
}
