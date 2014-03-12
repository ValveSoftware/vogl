#version 330

#define POSITION		0
#define COLOR			3
#define TEXCOORD		4
#define INSTANCE		7
#define FRAG_COLOR		0

precision highp int;

uniform samplerBuffer Displacement;
uniform mat4 MVP;

layout(location = POSITION) in vec2 Position;

out block
{
	flat int Instance;
} Out;

void main()
{	
	Out.Instance = gl_InstanceID;
	gl_Position = MVP * (vec4(Position, 0.0, 0.0) + texelFetch(Displacement, gl_InstanceID));
}
