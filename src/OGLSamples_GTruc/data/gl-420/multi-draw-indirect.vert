#version 420 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

#define TRANSFORM0	1

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec3 Position;
layout(location = TEXCOORD) in vec3 Texcoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

out block
{
	vec3 Texcoord;
} Out;

void main()
{	
	gl_Position = Transform.MVP * vec4(Position, 1.0);
	Out.Texcoord = Texcoord;
}

