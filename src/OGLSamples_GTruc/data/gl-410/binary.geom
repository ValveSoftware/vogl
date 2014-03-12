#version 410 core

precision highp int;

layout(triangles, invocations = 1) in;
layout(triangle_strip, max_vertices = 4) out;

in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

in block
{
	vec2 Texcoord;
	vec3 Color;
} In[]; 

out gl_PerVertex 
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out block
{
	vec2 Texcoord;
	vec3 Color;
} Out; 

uniform mat4 MVP;

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		Out.Texcoord = In[i].Texcoord;
		Out.Color = In[i].Color;
		EmitVertex();
	}

	EndPrimitive();
}

