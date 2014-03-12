#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define COMMON		0
#define FRAG_COLOR	0

#define MATERIAL	0
#define TRANSFORM0	1
#define TRANSFORM1	2	

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
	flat int Instance;
} Out;

void main()
{	
	Out.Instance = int(gl_InstanceID);
	Out.Texcoord = Texcoord;
	gl_Position = Transform.MVP * vec4(Position.x - 2.0 + float(gl_InstanceID) * 2.0, Position.y, 0.0, 1.0);
}