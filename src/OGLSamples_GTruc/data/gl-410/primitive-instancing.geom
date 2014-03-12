#version 410 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 4) out;

in gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
} gl_in[];

layout(location = COLOR) flat in vec4 Color[];

out gl_PerVertex 
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location = COLOR) flat out vec4 GeomColor;

uniform mat4 MVP;

void main()
{	
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = MVP * (gl_in[i].gl_Position + vec4(vec2(0.0), - 0.5 + 0.25 * float(gl_InvocationID), 0.0));
		GeomColor = (vec4(gl_InvocationID + 1) / 6.0 + Color[i]) / 2.0; 
		EmitVertex();
	}
	EndPrimitive();
}

