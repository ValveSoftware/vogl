#version 400 core

const int VertexCount = 3;
const vec2 Position[VertexCount] = vec2[](
	vec2(-1.0,-1.0),
	vec2( 3.0,-1.0),
	vec2(-1.0, 3.0));

void main()
{	
	gl_Position = vec4(Position[gl_VertexID], 0.0, 1.0);
}
