#version 410 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

uniform dmat4 MVP;

layout(location = POSITION) in dvec3 Position;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main()
{	
	gl_Position = vec4(MVP * dvec4(Position, 1.0));
}

