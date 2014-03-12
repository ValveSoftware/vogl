#version 410 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define COMMON		0
#define FRAG_COLOR	0

precision highp int;

layout(triangles, invocations = 4) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = COMMON) out block
{
	flat int Instance;
} Out;

uniform mat4 MVP;

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = MVP * gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		Out.Instance = gl_InvocationID;
		EmitVertex();
	}

	EndPrimitive();
}

