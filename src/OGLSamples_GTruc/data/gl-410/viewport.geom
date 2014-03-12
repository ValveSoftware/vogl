#version 410 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

precision highp int;

layout(triangles, invocations = 4) in;
layout(triangle_strip, max_vertices = 3) out;

in gl_PerVertex
{
    vec4 gl_Position;
} gl_in[];

in block
{
	vec2 Texcoord;
} In[];

out block
{
	vec2 Texcoord;
	flat int Instance;
} Out;

uniform mat4 MVP;

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Layer = gl_InvocationID;
		gl_ViewportIndex = gl_InvocationID;
		gl_Position = MVP * gl_in[i].gl_Position;
		Out.Instance = gl_InvocationID;
		Out.Texcoord = In[i].Texcoord;
		EmitVertex();
	}

	EndPrimitive();
}

