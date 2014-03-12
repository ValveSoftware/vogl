#version 150 core

precision highp int;

layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

uniform mat4 MVP;

out block
{
	flat int Instance;
} Out;

void main()
{	
	for(int Layer = 0; Layer < 4; ++Layer)
	{
		gl_Layer = Layer;

		for(int i = 0; i < gl_in.length(); ++i)
		{
			gl_Position = MVP * gl_in[i].gl_Position;
			Out.Instance = Layer;
			EmitVertex();
		}

		EndPrimitive();
	}
}

