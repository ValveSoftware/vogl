#version 150 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;
/*
in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];
*/
in block
{
	vec4 Color;
} In[];
/*
out gl_PerVertex 
{
	vec4 gl_Position;
};
*/
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

