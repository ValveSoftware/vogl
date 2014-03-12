#version 330 core

#define ATTR_POSITION	0
#define VERT_POSITION	0
#define VERT_COLOR		3

uniform mat4 MVP;

layout(location = ATTR_POSITION) in vec4 Position;
out vec4 Color;

void main()
{	
	gl_Position = MVP * Position;
	Color = vec4(clamp(vec2(Position), 0.0, 1.0), 0.0, 1.0);
}
