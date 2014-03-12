#version 420 core

#define POSITION		0
#define COLOR			3
#define FRAG_COLOR		0

#define COUNT 2

uniform mat4 MVP;

struct my_vertex
{
	vec2 Position[2];
};

struct vertex
{
	vec4 Color;
};

layout(location = POSITION) in vec2 Position[2];
//layout(location = POSITION) in my_vertex Input;
layout(location = COLOR) in vec4 Color;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location = 0) out vertex st_Out;

out block
{
	vec4 Color;
	float Lumimance[COUNT];
} bl_Out; 

void main()
{
	//gl_Position = MVP * vec4((Input.Position[0] + Input.Position[1]) * 0.5, 0.0, 1.0);
	gl_Position = MVP * vec4((Position[0] + Position[1]) * 0.5, 0.0, 1.0);
	st_Out.Color = Color * 0.75;
	bl_Out.Color = Color * 0.25;

	for(int i = 0; i < COUNT; ++i)
		bl_Out.Lumimance[i] = 1.0 / float(COUNT);
}
