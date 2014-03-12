#version 150 core

uniform transform
{
	mat4 MVP;
} Transform;

in vec2 Position;
in vec2 Texcoord;

out block
{
	vec2 Texcoord;
	flat int Instance;
} Out;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{	
	Out.Texcoord = Texcoord;
	Out.Instance = 0;
	gl_Position = Transform.MVP * vec4(Position, 0.0, 1.0);
}
