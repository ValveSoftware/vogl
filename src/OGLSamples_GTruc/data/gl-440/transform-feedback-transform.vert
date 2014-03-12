#version 440 core

#define POSITION	0
#define COLOR		3
#define FRAG_COLOR	0

#define TRANSFORM0		1

layout(binding = TRANSFORM0) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = POSITION) in vec4 Position;

layout(xfb_buffer = 0, xfb_stride = 32) out;

out block
{
	layout(xfb_buffer = 0, xfb_offset = 16) vec4 Color;
} Out;

out gl_PerVertex
{
	layout(xfb_buffer = 0, xfb_offset = 0) vec4 gl_Position;
};

void main()
{	
	gl_Position = Transform.MVP * Position;
	Out.Color = vec4(clamp(vec2(Position), 0.0, 1.0), 0.0, 1.0);
}
