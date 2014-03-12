#version 420 core

#define POSITION	0
#define COLOR		3
#define TEXCOORD	4
#define FRAG_COLOR	0

layout(binding = 0, offset = 0) uniform atomic_uint Atomic;

in block
{
	vec2 Texcoord;
} In;

layout(location = FRAG_COLOR, index = 0) out vec4 Color;

vec3 rgbColor(vec3 hsvColor)
{
	vec3 hsv = hsvColor;
	vec3 rgbColor;

	if(hsv.y == 0)
		// achromatic (grey)
		rgbColor = vec3(hsv.z);
	else
	{
		float sector = floor(hsv.x / float(60));
		float frac = (hsv.x / float(60)) - sector;
		// factorial part of h
		float o = hsv.z * (float(1) - hsv.y);
		float p = hsv.z * (float(1) - hsv.y * frac);
		float q = hsv.z * (float(1) - hsv.y * (float(1) - frac));

		switch(int(sector))
		{
		default:
		case 0:
			rgbColor.r = hsv.z;
			rgbColor.g = q;
			rgbColor.b = o;
			break;
		case 1:
			rgbColor.r = p;
			rgbColor.g = hsv.z;
			rgbColor.b = o;
			break;
		case 2:
			rgbColor.r = o;
			rgbColor.g = hsv.z;
			rgbColor.b = q;
			break;
		case 3:
			rgbColor.r = o;
			rgbColor.g = p;
			rgbColor.b = hsv.z;
			break;
		case 4:
			rgbColor.r = q; 
			rgbColor.g = o; 
			rgbColor.b = hsv.z;
			break;
		case 5:
			rgbColor.r = hsv.z; 
			rgbColor.g = o; 
			rgbColor.b = p;
			break;
		}
	}

	return rgbColor;
}

vec3 hsvColor(vec3 rgbColor)
{
	vec3 hsv = rgbColor;
	float Min   = min(min(rgbColor.r, rgbColor.g), rgbColor.b);
	float Max   = max(max(rgbColor.r, rgbColor.g), rgbColor.b);
	float Delta = Max - Min;

	hsv.z = Max;

	if(Max != float(0))
	{
		hsv.y = Delta / hsv.z;
		float h = float(0);

		if(rgbColor.r == Max)
			// between yellow & magenta
			h = float(0) + float(60) * (rgbColor.g - rgbColor.b) / Delta;
		else if(rgbColor.g == Max)
			// between cyan & yellow
			h = float(120) + float(60) * (rgbColor.b - rgbColor.r) / Delta;
		else
			// between magenta & cyan
			h = float(240) + float(60) * (rgbColor.r - rgbColor.g) / Delta;

		if(h < float(0)) 
			hsv.x = h + float(360);
		else
			hsv.x = h;
	}
	else
	{
		// If r = g = b = 0 then s = 0, h is undefined
		hsv.y = float(0);
		hsv.x = float(0);
	}

	return hsv;
}

void main()
{
	uint Mask = (1 << 8) - 1;
	uint Counter = atomicCounterIncrement(Atomic);

	//vec3 hsv = vec3(Counter / 360.0f, 1.0f, 1.0f);
	//Color = vec4(rgbColor(hsv), 1.0);
/*
	Color = vec4(
		((Counter >>  0) & Mask) * 1 % 256 / 255.f,
		((Counter >>  8) & Mask) * 1 % 256 / 255.f,
		((Counter >> 16) & Mask) * 1 % 256 / 255.f,
		0.5);
*/
	Color = vec4(
		((Counter >>  0 >> 0) & Mask) * 1 % 256 / 255.f,
		((Counter >>  8 >> 0) & Mask) * 1 % 256 / 255.f,
		((Counter >> 16 >> 0) & Mask) * 1 % 256 / 255.f,
		0.5);

}
