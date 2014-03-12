#version 150 core

uniform mat4 MV;
uniform mat4 MVP;
uniform vec3 Camera;

const vec3 ConstView = vec3(0, 0,-1);
const vec3 ConstNormal = vec3(0, 0, 1);

in vec2 Position;

out block
{
	vec3 Reflect;
} Out;

void main()
{	
	mat3 MV3x3 = mat3(MV);

	gl_Position = MVP * vec4(Position, 0.0, 1.0);
	vec3 P = MV3x3 * vec3(Position, 0.0);
	vec3 N = MV3x3 * ConstNormal;
	vec3 E = normalize(P - Camera);
	
	Out.Reflect = reflect(E, N);
}
