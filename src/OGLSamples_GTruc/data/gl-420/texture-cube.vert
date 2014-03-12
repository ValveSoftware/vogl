#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

#define TRANSFORM0	1

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
	mat4 MV;
	vec3 Camera;
} Transform;

const vec3 ConstView = vec3(0, 0,-1);
const vec3 ConstNormal = vec3(0, 0, 1);

layout(location = POSITION) in vec2 Position;

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec3 Reflect;
} Out;

void main()
{	
	mat3 MV3x3 = mat3(Transform.MV);

	gl_Position = Transform.MVP * vec4(Position, 0.0, 1.0);
	vec3 P = MV3x3 * vec3(Position, 0.0);
	vec3 N = MV3x3 * ConstNormal;
	vec3 E = normalize(P - Transform.Camera);
	
	Out.Reflect = reflect(E, N);
}
