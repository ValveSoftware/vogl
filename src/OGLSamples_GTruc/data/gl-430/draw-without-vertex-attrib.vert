#version 420 core
#extension GL_ARB_shader_storage_buffer_object : require

#define TRANSFORM0	1
#define VERTEX		0

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

struct vertex
{
	vec2 Position;
	vec2 Texcoord;
};

layout(binding = VERTEX) buffer mesh
{
	vertex Vertex[];
} Mesh;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
} Out;

void main()
{	
	Out.Texcoord = Mesh.Vertex[gl_VertexID].Texcoord;
	gl_Position = Transform.MVP * vec4(Mesh.Vertex[gl_VertexID].Position, 0.0, 1.0);
}
