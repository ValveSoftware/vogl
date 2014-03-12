uniform mat4 MVP;

attribute vec4 Position;

void main()
{	
	gl_Position = MVP * Position;
}

