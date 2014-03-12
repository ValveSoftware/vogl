#version 400 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

layout(triangles, invocations = 1) in;
layout(triangle_strip, max_vertices = 4) out;

in block
{
	vec4 Color;
} In[];

out block
{
	vec4 Color;
} Out;

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		Out.Color = In[i].Color;
		EmitVertex();
	}
	EndPrimitive();
}

