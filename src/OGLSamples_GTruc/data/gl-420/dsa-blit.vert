#version 330 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec2 Position;

const int VertexCount = 3;
const vec2 Position[VertexCount] = vec2[](
	vec2( 0.0f, 0.0f),
	vec2( 2.0f, 0.0f),
	vec2( 0.0f, 2.0f));

void main()
{	
	gl_Position = Transform.MVP * vec4(Position, 0.0, 1.0);
	//gl_Position = Transform.MVP * vec4(Position[gl_VertexID], 0.0, 1.0);
}
