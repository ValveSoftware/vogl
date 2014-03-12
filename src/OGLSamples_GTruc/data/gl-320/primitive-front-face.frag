#version 150 core

const vec4 FrontColor = vec4(1.0, 0.5, 0.0, 1.0);
const vec4 BackColor = vec4(0.0, 0.5, 1.0, 1.0);

out vec4 Color;

void main()
{
	Color = gl_FrontFacing ? FrontColor : BackColor;
}

