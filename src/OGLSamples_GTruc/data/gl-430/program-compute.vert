#version 420 core
#extension GL_ARB_shader_storage_buffer_object : require

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

#define VERTEX		0
#define TRANSFORM0		1

struct vertex
{
	vec4 Position;
	vec4 Texcoord;
	vec4 Color;
};

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

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
	vec4 Texcoord;
	vec4 Color;
} Out;

void main()
{	
	Out.Texcoord = Mesh.Vertex[gl_VertexID].Texcoord;
	if(gl_VertexID % 2 != 0)
		Out.Color = vec4(1.0);
	else
		Out.Color = Mesh.Vertex[gl_VertexID].Color;
	gl_Position = Transform.MVP * Mesh.Vertex[gl_VertexID].Position;
}
