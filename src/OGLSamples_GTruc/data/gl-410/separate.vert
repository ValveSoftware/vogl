#version 410 core

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define FRAG_COLOR		0

uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;
layout(location = TEXCOORD) in vec2 Texcoord;
layout(location = COLOR) out vec3 VertColor;
layout(location = TEXCOORD) out vec2 VertTexcoord;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{	
	gl_Position = MVP * vec4(Position, 0.0, 1.0);
	VertTexcoord = Texcoord;
	VertColor = vec3(1.0, 0.9, 0.8);
}

