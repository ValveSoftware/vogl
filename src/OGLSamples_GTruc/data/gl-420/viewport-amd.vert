#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4

precision highp int;

uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;

out block
{ 
	vec2 Texcoord;
	flat int Instance;
} Out;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{	
	Out.Texcoord = Texcoord;
	Out.Instance = gl_InstanceID;

	gl_Position = MVP * vec4(Position, 0.0, 1.0);
	gl_Layer = gl_InstanceID;
	gl_ViewportIndex = gl_InstanceID;
}
