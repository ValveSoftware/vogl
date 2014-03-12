#version 150

precision highp int;

uniform samplerBuffer Displacement;
uniform mat4 MVP;

in vec2 Position;

out block
{
	flat int Instance;
} Out;

void main()
{	
	Out.Instance = gl_InstanceID;
	gl_Position = MVP * (vec4(Position, 0.0, 0.0) + texelFetch(Displacement, gl_InstanceID));
}
