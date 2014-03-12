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
	float gl_ClipDistance[1];
};

out block
{
	vec4 Position;
	vec2 Texcoord;
} Out;

void main()
{	
	vec4 Position = Transform.MVP * vec4(Position, 1.0);

	Out.Position = Position;
	Out.Texcoord = Texcoord;
	gl_Position = Position;
	gl_ClipDistance[0] = mix(-1.0, 1.0, Position.z > 2);
}
