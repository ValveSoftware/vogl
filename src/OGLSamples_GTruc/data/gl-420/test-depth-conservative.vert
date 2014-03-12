#version 420 core

#define POSITION	0

#define TRANSFORM0	1

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec2 Position;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	float Instance;
} Out;

void main()
{	
	gl_Position = Transform.MVP * vec4(Position, float(gl_InstanceID) * 0.25 - 0.5, 1.0);
	Out.Instance = float(gl_InstanceID);
}
