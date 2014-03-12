#version 440 core

#define POSITION		0
#define COLOR			3
#define FRAG_COLOR		0

#define TRANSFORM0		1

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec2 Position[2];
layout(location = COLOR) in dvec4 Color;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

struct vertex
{
	vec4 Color;
};

layout(location = 0) out vertex st_Out[2];

layout(location = 0 + 1 * st_Out.length()) out block
{
	vec4 Color;
	mediump float Lumimance[2];
} bl_Out; 

void main()
{	
	gl_Position = Transform.MVP * vec4((Position[0] + Position[1]) * 0.5, 0.0, 1.0);
	st_Out[0].Color = vec4(Color) * 0.25;
	st_Out[1].Color = vec4(Color) * 0.50;
	bl_Out.Color = vec4(Color) * 0.25;

	for(int i = 0; i < 2; ++i)
		bl_Out.Lumimance[i] = 1.0 / 2.0;
}
