#version 420 core

layout(triangles, invocations = 1) in;
layout(triangle_strip, max_vertices = 3) out;

in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

in block
{
	vec2 Texcoord;
	flat int Instance;
} In[];

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
	flat int Instance;
} Out;

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_ViewportIndex = In[i].Instance;
		gl_Position = gl_in[i].gl_Position;
		Out.Texcoord = In[i].Texcoord;
		Out.Instance = In[i].Instance;
		EmitVertex();
	}
	EndPrimitive();
}

