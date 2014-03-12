#version 420 core

precision highp int;

vec4 hermite(in vec4 A, in vec4 B, in vec4 C, in vec4 D, in float s)
{
	mat4 Hermite = mat4(
		vec4( 2,-3, 0, 1), 
		vec4(-2, 3, 0, 0),
		vec4( 1,-2, 1, 0),
		vec4( 1,-1, 0, 0));

	vec4 Expo = vec4(s * s * s, s * s, s, 1);

	return Expo * Hermite * mat4(
		A[0], B[0], C[0], D[0],
		A[1], B[1], C[1], D[1],
		A[2], B[2], C[2], D[2],
		A[3], B[3], C[3], D[3]);
}

vec4 bezier(in vec4 A, in vec4 B, in vec4 C, in vec4 D, in float s)
{
	mat4 Bezier = mat4(
		vec4(-1, 3,-3, 1), 
		vec4( 3,-6, 3, 0),
		vec4(-3, 3, 0, 0),
		vec4( 1, 0, 0, 0));

	vec4 Expo = vec4(s * s * s, s * s, s, 1);

	return Expo * Bezier * mat4(
		A[0], B[0], C[0], D[0],
		A[1], B[1], C[1], D[1],
		A[2], B[2], C[2], D[2],
		A[3], B[3], C[3], D[3]);
}

vec4 catmullRom(in vec4 A, in vec4 B, in vec4 C, in vec4 D, in float s)
{
	mat4 CatmullRom = mat4(
		vec4(-1, 2,-1, 0), 
		vec4( 3,-5, 0, 2),
		vec4(-3, 4, 1, 0),
		vec4( 1,-1, 0, 0));

	vec4 Expo = vec4(s * s * s, s * s, s, 1);

	return 0.5 * Expo * CatmullRom * mat4(
		A[0], B[0], C[0], D[0],
		A[1], B[1], C[1], D[1],
		A[2], B[2], C[2], D[2],
		A[3], B[3], C[3], D[3]);
}

vec2 mirrorRepeat(in vec2 Texcoord)
{
	vec2 Clamp = vec2(ivec2(floor(Texcoord)) % ivec2(2));
	vec2 Floor = floor(Texcoord);
	vec2 Rest = Texcoord - Floor;
	vec2 Mirror = Clamp + Rest;
	
	return mix(vec2(1) - Rest, Rest, greaterThanEqual(Mirror, vec2(1)));
}

float textureLevel(in sampler2D Sampler, in vec2 Texcoord)
{
	vec2 TextureSize = vec2(textureSize(Sampler, 0));

	float LevelCount = max(log2(TextureSize.x), log2(TextureSize.y));

	vec2 dx = dFdx(Texcoord * TextureSize);
	vec2 dy = dFdy(Texcoord * TextureSize);
	float d = max(dot(dx, dx), dot(dy, dy));

	d = clamp(d, 1.0, pow(2, (LevelCount - 1) * 2));
  
	return 0.5 * log2(d);
}

vec4 fetchBilinear(
	in sampler2D Sampler, in vec2 Interpolant, in ivec2 Texelcoords[4], in int Lod)
{
	vec4 Texel00 = texelFetch(Sampler, Texelcoords[0], Lod);
	vec4 Texel10 = texelFetch(Sampler, Texelcoords[1], Lod);
	vec4 Texel11 = texelFetch(Sampler, Texelcoords[2], Lod);
	vec4 Texel01 = texelFetch(Sampler, Texelcoords[3], Lod);
	
	vec4 Texel0 = mix(Texel00, Texel01, Interpolant.y);
	vec4 Texel1 = mix(Texel10, Texel11, Interpolant.y);
	return mix(Texel0, Texel1, Interpolant.x);
}

///////////////
// Nearest 
vec4 textureNearest(
	in sampler2D Sampler, in vec2 Texcoord)
{
	int LodNearest = int(round(textureQueryLod(Sampler, Texcoord).x));

	ivec2 TextureSize = textureSize(Sampler, LodNearest);
	ivec2 Texelcoord = ivec2(TextureSize * Texcoord);

	return texelFetch(Sampler, Texelcoord, LodNearest);
}

vec4 textureNearestLod(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int LodNearest = int(round(Lod));

	ivec2 TextureSize = textureSize(Sampler, LodNearest);
	ivec2 Texelcoord = ivec2(TextureSize * Texcoord);

	return texelFetch(Sampler, Texelcoord, LodNearest);
}

vec4 textureNearestLod(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 TextureSize = textureSize(Sampler, Lod);
	ivec2 Texelcoord = ivec2(TextureSize * Texcoord);

	return texelFetch(Sampler, Texelcoord, Lod);
}

vec4 textureNearestLodRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int LodNearest = int(round(Lod));
	ivec2 Size = textureSize(Sampler, LodNearest);

	ivec2 TextureSize = textureSize(Sampler, LodNearest);
	ivec2 Texelcoord = ivec2(TextureSize * Texcoord) % Size;

	return texelFetch(Sampler, Texelcoord, LodNearest);
}

///////////////
// Bilinear 
vec4 textureBilinear(
	in sampler2D Sampler, in vec2 Texcoord)
{
	int Lod = int(round(textureQueryLod(Sampler, Texcoord).x));

	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
	
	ivec2 Texelcoords[] = ivec2[4](
		TexelIndex + ivec2(0, 0),
		TexelIndex + ivec2(1, 0),
		TexelIndex + ivec2(1, 1),
		TexelIndex + ivec2(0, 1));

	return fetchBilinear(
		Sampler, 
		fract(Texelcoord), 
		Texelcoords, 
		Lod);
}

vec4 textureBilinearLod(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
	
	ivec2 Texelcoords[] = ivec2[4](
		TexelIndex + ivec2(0, 0),
		TexelIndex + ivec2(1, 0),
		TexelIndex + ivec2(1, 1),
		TexelIndex + ivec2(0, 1));

	return fetchBilinear(
		Sampler, 
		fract(Texelcoord), 
		Texelcoords, 
		Lod);
}

vec4 textureBilinearLodRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
		
	ivec2 Texelcoords[] = ivec2[4](
		(TexelIndex + ivec2(0, 0)) % Size,
		(TexelIndex + ivec2(1, 0)) % Size,
		(TexelIndex + ivec2(1, 1)) % Size,
		(TexelIndex + ivec2(0, 1)) % Size);
	
	return fetchBilinear(
		Sampler, 
		fract(Texelcoord), 
		Texelcoords, 
		Lod);
}

vec4 textureBilinearLodMirrorRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = mirrorRepeat(Texcoord) * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
		
	ivec2 Texelcoords[] = ivec2[4](
		(TexelIndex + ivec2(0, 0)) % Size,
		(TexelIndex + ivec2(1, 0)) % Size,
		(TexelIndex + ivec2(1, 1)) % Size,
		(TexelIndex + ivec2(0, 1)) % Size);
	
	return fetchBilinear(
		Sampler, 
		fract(Texelcoord), 
		Texelcoords, 
		Lod);
}

vec4 textureBilinearLodClampToBorder(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = clamp(Texcoord, 0, 1) * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);
	
	ivec2 Texelcoords[] = ivec2[4](
		clamp(TexelIndex + ivec2(0, 0), ivec2(0), Size - 1),
		clamp(TexelIndex + ivec2(1, 0), ivec2(0), Size - 1),
		clamp(TexelIndex + ivec2(1, 1), ivec2(0), Size - 1),
		clamp(TexelIndex + ivec2(0, 1), ivec2(0), Size - 1));

	return fetchBilinear(
		Sampler, 
		fract(Texelcoord), 
		Texelcoords, 
		Lod);
}

///////////////
// Trilinear 
vec4 textureTrilinear(
	in sampler2D Sampler, in vec2 Texcoord)
{
	float Lod = textureQueryLod(Sampler, Texcoord).x;

	int lodMin = int(floor(Lod));
	int lodMax = int(ceil(Lod));

	vec4 TexelMin = textureBilinearLod(Sampler, Texcoord, lodMin);
	vec4 TexelMax = textureBilinearLod(Sampler, Texcoord, lodMax);

	return mix(TexelMin, TexelMax, fract(Lod));
}

vec4 textureTrilinearLod(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int LodMin = int(floor(Lod));
	int LodMax = int(ceil(Lod));

	vec4 TexelMin = textureBilinearLod(Sampler, Texcoord, LodMin);
	vec4 TexelMax = textureBilinearLod(Sampler, Texcoord, LodMax);

	return mix(TexelMin, TexelMax, fract(Lod));
}

vec4 textureTrilinearLodRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int LodMin = int(floor(Lod));
	int LodMax = int(ceil(Lod));

	vec4 TexelMin = textureBilinearLodRepeat(Sampler, Texcoord, LodMin);
	vec4 TexelMax = textureBilinearLodRepeat(Sampler, Texcoord, LodMax);

	return mix(TexelMin, TexelMax, fract(Lod));
}

vec4 textureTrilinearLodMirrorRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int LodMin = int(floor(Lod));
	int LodMax = int(ceil(Lod));

	vec4 TexelMin = textureBilinearLodMirrorRepeat(Sampler, Texcoord, LodMin);
	vec4 TexelMax = textureBilinearLodMirrorRepeat(Sampler, Texcoord, LodMax);

	return mix(TexelMin, TexelMax, fract(Lod));
}

vec4 textureTrilinearLodClampToBorder(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int LodMin = int(floor(Lod));
	int LodMax = int(ceil(Lod));

	vec4 TexelMin = textureBilinearLodClampToBorder(Sampler, Texcoord, LodMin);
	vec4 TexelMax = textureBilinearLodClampToBorder(Sampler, Texcoord, LodMax);

	return mix(TexelMin, TexelMax, fract(Lod));
}

///////////////
// Bicubic 
vec4 textureBicubicLod(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);

	vec4 Texel00 = texelFetch(Sampler, TexelIndex + ivec2(-1,-1), Lod);
	vec4 Texel10 = texelFetch(Sampler, TexelIndex + ivec2( 0,-1), Lod);
	vec4 Texel20 = texelFetch(Sampler, TexelIndex + ivec2( 1,-1), Lod);
	vec4 Texel30 = texelFetch(Sampler, TexelIndex + ivec2( 2,-1), Lod);

	vec4 Texel01 = texelFetch(Sampler, TexelIndex + ivec2(-1, 0), Lod);
	vec4 Texel11 = texelFetch(Sampler, TexelIndex + ivec2( 0, 0), Lod);
	vec4 Texel21 = texelFetch(Sampler, TexelIndex + ivec2( 1, 0), Lod);
	vec4 Texel31 = texelFetch(Sampler, TexelIndex + ivec2( 2, 0), Lod);
	
	vec4 Texel02 = texelFetch(Sampler, TexelIndex + ivec2(-1, 1), Lod);
	vec4 Texel12 = texelFetch(Sampler, TexelIndex + ivec2( 0, 1), Lod);
	vec4 Texel22 = texelFetch(Sampler, TexelIndex + ivec2( 1, 1), Lod);
	vec4 Texel32 = texelFetch(Sampler, TexelIndex + ivec2( 2, 1), Lod);

	vec4 Texel03 = texelFetch(Sampler, TexelIndex + ivec2(-1, 2), Lod);
	vec4 Texel13 = texelFetch(Sampler, TexelIndex + ivec2( 0, 2), Lod);
	vec4 Texel23 = texelFetch(Sampler, TexelIndex + ivec2( 1, 2), Lod);
	vec4 Texel33 = texelFetch(Sampler, TexelIndex + ivec2( 2, 2), Lod);

	vec2 SplineCoord = fract(Texelcoord);

	vec4 Row0 = catmullRom(Texel00, Texel10, Texel20, Texel30, SplineCoord.x);
	vec4 Row1 = catmullRom(Texel01, Texel11, Texel21, Texel31, SplineCoord.x);
	vec4 Row2 = catmullRom(Texel02, Texel12, Texel22, Texel32, SplineCoord.x);
	vec4 Row3 = catmullRom(Texel03, Texel13, Texel23, Texel33, SplineCoord.x);

	return catmullRom(Row0, Row1, Row2, Row3, SplineCoord.y);
}

vec4 textureBicubicLodRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = Texcoord * Size - 0.5;
	ivec2 TexelIndex = ivec2(Texelcoord);

	vec4 Texel00 = texelFetch(Sampler, (TexelIndex + ivec2(-1,-1)) % Size, Lod);
	vec4 Texel10 = texelFetch(Sampler, (TexelIndex + ivec2( 0,-1)) % Size, Lod);
	vec4 Texel20 = texelFetch(Sampler, (TexelIndex + ivec2( 1,-1)) % Size, Lod);
	vec4 Texel30 = texelFetch(Sampler, (TexelIndex + ivec2( 2,-1)) % Size, Lod);

	vec4 Texel01 = texelFetch(Sampler, (TexelIndex + ivec2(-1, 0)) % Size, Lod);
	vec4 Texel11 = texelFetch(Sampler, (TexelIndex + ivec2( 0, 0)) % Size, Lod);
	vec4 Texel21 = texelFetch(Sampler, (TexelIndex + ivec2( 1, 0)) % Size, Lod);
	vec4 Texel31 = texelFetch(Sampler, (TexelIndex + ivec2( 2, 0)) % Size, Lod);

	vec4 Texel02 = texelFetch(Sampler, (TexelIndex + ivec2(-1, 1)) % Size, Lod);
	vec4 Texel12 = texelFetch(Sampler, (TexelIndex + ivec2( 0, 1)) % Size, Lod);
	vec4 Texel22 = texelFetch(Sampler, (TexelIndex + ivec2( 1, 1)) % Size, Lod);
	vec4 Texel32 = texelFetch(Sampler, (TexelIndex + ivec2( 2, 1)) % Size, Lod);

	vec4 Texel03 = texelFetch(Sampler, (TexelIndex + ivec2(-1, 2)) % Size, Lod);
	vec4 Texel13 = texelFetch(Sampler, (TexelIndex + ivec2( 0, 2)) % Size, Lod);
	vec4 Texel23 = texelFetch(Sampler, (TexelIndex + ivec2( 1, 2)) % Size, Lod);
	vec4 Texel33 = texelFetch(Sampler, (TexelIndex + ivec2( 2, 2)) % Size, Lod);

	vec2 SplineCoord = fract(Texelcoord);

	vec4 Row0 = catmullRom(Texel00, Texel10, Texel20, Texel30, SplineCoord.x);
	vec4 Row1 = catmullRom(Texel01, Texel11, Texel21, Texel31, SplineCoord.x);
	vec4 Row2 = catmullRom(Texel02, Texel12, Texel22, Texel32, SplineCoord.x);
	vec4 Row3 = catmullRom(Texel03, Texel13, Texel23, Texel33, SplineCoord.x);

	return catmullRom(Row0, Row1, Row2, Row3, SplineCoord.y);
}

vec4 textureBicubicLodRepeat2(
	in sampler2D Sampler, in vec2 Texcoord, in int Lod)
{
	ivec2 Size = textureSize(Sampler, Lod);
	vec2 Texelcoord = Texcoord * Size - 1.5;
	ivec2 TexelIndex = ivec2(Texelcoord);

	vec4 Texel00 = texelFetch(Sampler, (TexelIndex + ivec2(0, 0)) % Size, Lod);
	vec4 Texel10 = texelFetch(Sampler, (TexelIndex + ivec2(1, 0)) % Size, Lod);
	vec4 Texel20 = texelFetch(Sampler, (TexelIndex + ivec2(2, 0)) % Size, Lod);
	vec4 Texel30 = texelFetch(Sampler, (TexelIndex + ivec2(3, 0)) % Size, Lod);

	vec4 Texel01 = texelFetch(Sampler, (TexelIndex + ivec2(0, 1)) % Size, Lod);
	vec4 Texel11 = texelFetch(Sampler, (TexelIndex + ivec2(1, 1)) % Size, Lod);
	vec4 Texel21 = texelFetch(Sampler, (TexelIndex + ivec2(2, 1)) % Size, Lod);
	vec4 Texel31 = texelFetch(Sampler, (TexelIndex + ivec2(3, 1)) % Size, Lod);

	vec4 Texel02 = texelFetch(Sampler, (TexelIndex + ivec2(0, 2)) % Size, Lod);
	vec4 Texel12 = texelFetch(Sampler, (TexelIndex + ivec2(1, 2)) % Size, Lod);
	vec4 Texel22 = texelFetch(Sampler, (TexelIndex + ivec2(2, 2)) % Size, Lod);
	vec4 Texel32 = texelFetch(Sampler, (TexelIndex + ivec2(3, 2)) % Size, Lod);

	vec4 Texel03 = texelFetch(Sampler, (TexelIndex + ivec2(0, 3)) % Size, Lod);
	vec4 Texel13 = texelFetch(Sampler, (TexelIndex + ivec2(1, 3)) % Size, Lod);
	vec4 Texel23 = texelFetch(Sampler, (TexelIndex + ivec2(2, 3)) % Size, Lod);
	vec4 Texel33 = texelFetch(Sampler, (TexelIndex + ivec2(3, 3)) % Size, Lod);

	vec2 SplineCoord = fract(Texelcoord);

	vec4 Row0 = catmullRom(Texel00, Texel10, Texel20, Texel30, SplineCoord.x);
	vec4 Row1 = catmullRom(Texel01, Texel11, Texel21, Texel31, SplineCoord.x);
	vec4 Row2 = catmullRom(Texel02, Texel12, Texel22, Texel32, SplineCoord.x);
	vec4 Row3 = catmullRom(Texel03, Texel13, Texel23, Texel33, SplineCoord.x);

	return catmullRom(Row0, Row1, Row2, Row3, SplineCoord.y);
}

vec4 textureBicubicLinearLodRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int lodMin = int(floor(Lod));
	int lodMax = int(ceil(Lod));

	vec4 TexelMin = textureBicubicLodRepeat(Sampler, Texcoord, lodMin);
	vec4 TexelMax = textureBicubicLodRepeat(Sampler, Texcoord, lodMax);

	return mix(TexelMin, TexelMax, fract(Lod));
}

///////////////
// Tricubic 
vec4 textureTricubicLodRepeat(
	in sampler2D Sampler, in vec2 Texcoord, in float Lod)
{
	int lodMin = int(floor(Lod));
	int lodMax = int(ceil(Lod));

	vec4 TexelA = textureBicubicLodRepeat(Sampler, Texcoord, max(lodMin - 1, 0));
	vec4 TexelB = textureBicubicLodRepeat(Sampler, Texcoord, lodMin);
	vec4 TexelC = textureBicubicLodRepeat(Sampler, Texcoord, lodMax);
	vec4 TexelD = textureBicubicLodRepeat(Sampler, Texcoord, lodMax + 1);

	return catmullRom(TexelA, TexelB, TexelC, TexelD, fract(Lod));
}

/////////////////////
// Color procesing 
vec4 squeeze(in vec4 Color, in float Ratio)
{
	return Color * (1.0 - Ratio) + (Ratio * 0.5);
}


