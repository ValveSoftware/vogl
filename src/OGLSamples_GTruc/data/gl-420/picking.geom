#version 420 core

precision highp float;

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

in block
{
	vec2 Texcoord;
} In[];

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
} Out;

void main()
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		Out.Texcoord = In[i].Texcoord;
		EmitVertex();
	}
	EndPrimitive();
}
