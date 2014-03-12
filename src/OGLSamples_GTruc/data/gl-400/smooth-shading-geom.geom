#version 400 core

#define ATTR_POSITION	0
#define ATTR_COLOR		3
#define ATTR_TEXCOORD	4
#define FRAG_COLOR		0

layout(triangles, invocations = 1) in;
layout(triangle_strip, max_vertices = 4) out;
precision highp float;

in block
{
	vec4 Color;
} In[];

out block
{
	layout(stream = 0) vec4 Color;
} Out;

void main()
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		Out.Color = In[i].Color;
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();
}

