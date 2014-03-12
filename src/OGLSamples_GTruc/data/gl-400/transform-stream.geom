#version 400 core

#define POSITION	0
#define COLOR		3
#define FRAG_COLOR	0

precision highp float;

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in block
{
	vec4 Color;
} In[];

out block
{
	layout(stream = 0) vec4 Color;
} Out;

out gl_PerVertex 
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main()
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		Out.Color = In[i].Color;
		gl_Position = gl_in[i].gl_Position;
		EmitStreamVertex(0);
	}
	EndStreamPrimitive(0);
}
