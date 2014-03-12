#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

#define TRANSFORM0	1

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec3 Position;
layout(location = TEXCOORD) in vec2 Texcoord;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
	mediump int Instance;
} Out;

void main()
{	
	Out.Texcoord = Texcoord;
	Out.Instance = mediump int(gl_InstanceID);
	gl_Position = Transform.MVP * vec4(Position, 1.0);
}
