#version 420 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define DRAW_ID			5

#define FRAG_COLOR		0

#define TRANSFORM0		1
#define INDIRECTION		3

#define MAX_DRAW		3

layout(binding = INDIRECTION) uniform indirection
{
	int Transform[MAX_DRAW];
} Indirection;

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP[MAX_DRAW];
} Transform;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;
layout(location = DRAW_ID) in int DrawID;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
	flat uint DrawID;
} Out;

void main()
{
	Out.DrawID = DrawID;
	Out.Texcoord = Texcoord.st;
	gl_Position = Transform.MVP[Indirection.Transform[DrawID]] * vec4(Position, 0.0, 1.0);
}
