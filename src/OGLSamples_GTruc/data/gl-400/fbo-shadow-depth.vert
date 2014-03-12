#version 400 core

uniform transform
{
	mat4 MVP;
	mat4 DepthMVP;
	mat4 DepthBiasMVP;
} Transform;

in vec3 Position;

void main()
{	
	gl_Position = Transform.DepthMVP * vec4(Position, 1.0);
}
