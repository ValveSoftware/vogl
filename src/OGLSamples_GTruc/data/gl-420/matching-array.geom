#version 420 core

#define POSITION		0
#define COLOR			3
#define FRAG_COLOR		0

struct vertex
{
	vec4 Color;
};

layout(triangles, invocations = 1) in;
layout(triangle_strip, max_vertices = 4) out;

in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location = 0) in vec4 Color[][1];

out gl_PerVertex 
{
	vec4 gl_Position;
};

layout(location = 0) out vec4 ColorGeom;

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		ColorGeom = Color[0][i];
		EmitVertex();
	}
	EndPrimitive();
}

