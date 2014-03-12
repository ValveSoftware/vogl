#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

#define MATERIAL	0
#define TRANSFORM0	1
#define TRANSFORM1	2	

uniform int Instance;

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform[2];

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = POSITION) in vec2 Position;

void main()
{	
	gl_Position = Transform[Instance].MVP * vec4(Position, 0.0, 1.0);
}


