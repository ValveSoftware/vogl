#version 150 core

uniform transform
{
	mat4 MVP;
} Transform;

const int VertexCount = 6;
const vec2 Position[VertexCount] = vec2[](
	vec2(-1.0,-1.0),
	vec2( 1.0,-1.0),
	vec2( 1.0, 1.0),
	vec2(-1.0,-1.0),
	vec2( 1.0, 1.0),
	vec2(-1.0, 1.0));

void main()
{	
	gl_Position = Transform.MVP * vec4(Position[gl_VertexID], 0.0, 1.0);
}

