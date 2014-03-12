#version 330 core

#define ATTR_POSITION	0
#define ATTR_COLOR		3
#define ATTR_TEXCOORD	4
#define FRAG_COLOR		0

layout(triangle_strip, max_vertices = 4) out;

in vec4 VertColor[];
out vec4 GeomColor;

void main()
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		GeomColor = VertColor[i];
		EmitVertex();
	}
	EndPrimitive();
}

