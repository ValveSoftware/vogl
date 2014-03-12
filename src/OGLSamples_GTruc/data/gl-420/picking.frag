#version 420 compatibility

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

in vec4 gl_FragCoord;

layout(binding = 0) uniform sampler2D Diffuse;
layout(binding = 1, r32f) writeonly uniform imageBuffer Depth;

/*
layout(binding = PICKING) uniform picking
{
	uvec2 Coord;
} Picking;
*/

uvec2 PickingCoord = uvec2(320, 240);

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

void main()
{
	if(all(equal(PickingCoord, uvec2(gl_FragCoord.xy))))
	{
		imageStore(Depth, 0, vec4(gl_FragCoord.z, 0, 0, 0));
		Color = vec4(1, 0, 1, 1);
	}
	else
		Color = texture(Diffuse, In.Texcoord.st);
}
