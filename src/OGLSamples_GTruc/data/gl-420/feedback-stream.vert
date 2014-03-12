#version 420 core

#define POSITION	0
#define COLOR		3
#define FRAG_COLOR	0

uniform mat4 MVP;

layout(location = POSITION) in vec4 Position;
layout(location = COLOR) in vec4 Color;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec4 Color;
} Out;

void main()
{	
	gl_Position = Position + MVP * vec4(0, 0, float(gl_InstanceID) * 0.25 - 0.5, 0);
	Out.Color = Color;
}

