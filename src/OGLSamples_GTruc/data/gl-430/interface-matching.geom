#version 420 core
#extension GL_ARB_arrays_of_arrays : require

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
	float gl_PointSize;
	float gl_ClipDistance[];
} gl_in[];

layout(location = 0) in vertex st_In[][2];

in block
{
	vec4 Color;
} bl_In[]; 

out gl_PerVertex 
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location = 0) out vertex st_Out;

out vec4 ColorGNI;

out block
{
	vec4 Color;
} bl_Out; 

out block2
{
	vec4 Color;
} bl_Pou; 

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		ColorGNI = st_In[i][0].Color + st_In[i][1].Color;
		st_Out.Color = st_In[i][0].Color + st_In[i][1].Color;
		bl_Out.Color = bl_In[i].Color;
		bl_Pou.Color = st_In[i][0].Color + bl_In[i].Color;
		EmitVertex();
	}
	EndPrimitive();
}

