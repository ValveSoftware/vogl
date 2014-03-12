// stb_wtf_3d.c -- public domain -- Sean Barrett 2011-03-30
//
// this is the software used to create OK Go: Space-Time WTF
//     http://www.youtube.com/watch?v=QL2NuY5pGIY
//
// the software in this file is in the public domain; you
// can modify and use it for any purpose without limitations
//
// note it relies on a few unreleased libraries so you probably
// can't make it work as is. also the data files aren't included.
// it's probably not really very interesting, but hey.


// uncomment this to do the final rendering
//#define FINAL 24


// comment this for higher quality rendering
#define FAST

// make sure FAST is disabled if FINAL is true
#ifdef FINAL
#ifdef FAST
#undef FAST
#endif
#endif

// get a bmp/png writer
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <assert.h>  // we use the 'assert' function
#include <ctype.h>   // we use some 'ctype' functions
#include <windows.h> // we use some windows functions

// disable warnings in MSVC about converting between integers and real
// numbers in various ways, because they're annoying when writing this
// kind of numerically-tweaked graphics code
#pragma warning(disable:4244; disable:4305)

// we use various "stb" libraries, so we need to make them accessible
#define STB_GLEXT_DECLARE "glext_list.h"
#include "stb_image.h"     // png loader
#include "stb_wingraph.h"  // semi-portable wrapper of window handling in Windows
#include "stb_gl.h"        // OpenGL helper functions
#include "stb.h"           // tons of misc C helper routines
#include "stb_vec.h"       // vector of 3 floats, plus some matrix routines

// create a flag that is only true once we're passed various initalization;
// this way, when windows sends events to us *before* we've finished initializing,
// we can avoid doing anything that might go horrible wrong since initialization
// isn't done yet
int initialized=0;

// the size of the display in pixels
int sx,sy;

// the location and orientation of a camera. not sure why camera is a 4-vec.
// probably only used when editing camera keyframes
float camloc[4],camang[3];

// the color for the fog; 1,1,1 is white
float fcolor[4] = { 1,1,1,1 };

// are we currently rendering a shadowmap?
int computing_shadows;

// an old description of an xbox360 gamepad, but I didn't end up using this
typedef struct
{
	float mag[2], dir[2], x[2],y[2];
	int trigger[2], dpad[4], shoulder[2];
	uint8 raw_trigger[2];
	uint16 raw_buttons;
} Input;
Input input;

// a temporary buffer for processing MIDI messages (used for the KORG nanoKontrol)
//  NUM_MESSAGES is the number of 3-byte messages we can process in one "frame"
//         (windows converts all MIDI events to 3-bytes)
#define NUM_MESSAGES  1000
static unsigned char midi_buffer[3 * NUM_MESSAGES];
static int offset;

// a routine to pull MIDI data out of the above buffer
int midi_poll(unsigned char *data, int max_buffer)
{
	if (offset > max_buffer)
	{
		max_buffer = (max_buffer/3) * 3;
	}
	else
	{
		max_buffer = offset;
	}
	memcpy(data, midi_buffer, max_buffer);
	memmove(midi_buffer, midi_buffer+max_buffer, offset - max_buffer);
	offset -= max_buffer;
	return max_buffer;
}

// some math I wrote up estimating how much it would cost to keep the
// images in memory, since as a 32-bit Windows app I'm limited to 2GB for all storage
/* 180MB 50 next frames (1 second)
 * 180MB 50 frames after that, every other (2 seconds)
 * 150MB 300 frames @ 500KB/frame (4x reduced): 100 every other (4 seconds), 200 every fourth (33 seconds)
 * 156MB 1280 frames @ 125KB/frame (16x reduced): every 4th frame (entire movie)
 */

// Each frame of the video is itself split up into a grid of "tiles",
// and tiles which are entirely empty (blank) are omitted; this reduces
// memory usage by a factor of about 2x, and speeds up rendering of the
// video fames by 2x (although it varies, some frames are sparser than others)

// the following describes all the tiles of one image
typedef struct
{
	uint8 tw,th;          // the size of each rectangular tile
	uint8 num_tiles;      // the number of tiles for this frame
	uint8 tilepos[1][2];  // where each tile should be positioned; tilepos[k][0] is the x pos of the k'th tile, tilepos[k][1] is the ypos
} TileInfo;

// The texture coordinates for a rectnagle
typedef struct
{
	float s0,t0,s1,t1;
} TextureInfo;

// Open and read the tile-header data file
// It contains all the TileInfo for ALL the frames already packed together
// (but we have more than one of these, because we have one for each mipmap scale)
TileInfo **read_headers(char *filename)
{
	int expected_frame=1;
	int len,i;
	uint8 *data = stb_file(filename, &len);
	TileInfo **ti = malloc(sizeof(*ti) * 5129);
	memset(ti,0,sizeof(*ti) * 5129);
	for (i=0; i < len; )
	{
		int frame = *(int *) (data+i);
		i += 4;
		assert(frame > 0 && frame < 5129);
		assert(frame == expected_frame);
		ti[frame] = (TileInfo *) (data+i);
		i += 3 + data[i+2]*2;
		++expected_frame;
	}
	return ti;
}

// given that we have to pack N same-sized rectangles into a larger rectangle,
// the following table precomputes what the best dimensions for the larger
// rectangle are, to minimize waste
uint8 best_grid[256][2];

// now compute the above by brute force trying every possible width and computing the required height, and pick the best
void compute_best_grid(int n)
{
	int best_a = 5000;
	int i,j;
	for (i=1; i <= n; ++i)
	{
		j = (n + i - 1) / i;
		if (j < i) break;
		if (j < 100 && i*8 > j && i *j <= best_a)
		{
			best_a = i*j;
			best_grid[n][0] = j;
			best_grid[n][1] = i;
		}
	}
	assert(best_a != 5000);
}

// and use the above routine to precompute the best result for every possible size
void compute_best_grids(void)
{
	int n;
	for (n=250; n >= 1; --n)
		compute_best_grid(n);
}

// OpenGL error-checking routine
void e(void)
{
	GLenum err = glGetError();
	if (err)
	{
		const char *error = gluErrorString(err);
		printf("%s\n", error);
		err = err;
	}
}

// when we build the texture, we need a temporary buffer that's as larger as
// possible to build it into; that's:
//           4 bytes per pixel (RGBA)
//           65 pixels horizontally per tile (64 + 1 padding for bilinear)
//           61 pixels vertically per tile (60 + 1 padding; tiles are 64x60 to evenly divide 1280x720
//           20 tiles max one dimension
//           12 tiles max the other dimension
uint8 tempbuf[(65*20)*(61*12)*4];

// we want to use premultiplied alpha, so we use Jim Blinn's routine
// for efficiently multiplying 8-bit numbers to get an 8-bit result
static int stb__Mul8Bit(int a, int b)
{
	int t = a*b + 128;
	return (t + (t >> 8)) >> 8;
}

// now, given a tile and an openGL texture handle, build it as a packed texture
TextureInfo *build_texture(GLuint tex, uint8 *pixels, TileInfo *t)
{
	TextureInfo *tc = NULL;
	int tex_w, tex_h;
	int w,h,x,y,i,j,k;

	if (t->num_tiles == 0) return NULL;

	// given the number of tiles, choose the best grid
	w = best_grid[t->num_tiles][0];
	h = best_grid[t->num_tiles][1];

	// compute the necessary texture size, adding padding between tiles
	tex_w = w * (t->tw+1);
	tex_h = h * (t->th+1);

	// place the tiles down in order left-right, top-bottom

	// starting at (0,0)
	x=0;
	y=0;

	// for every tile
	for (i=0; i < t->num_tiles; ++i)
	{

		// compute the texture coordinates appropriate for this tile
		// we make each tile extend from pixel center to pixel center,
		// which lets us use exactly one padding pixel but get seamless bilinear interpolation
		// at the tile boundaries

		TextureInfo ii;
		assert(y < tex_h);
		ii.s0 = (float) (x+0.5) / tex_w;
		ii.t0 = (float) (y+0.5) / tex_h;
		ii.s1 = (float) (x+0.5 + t->tw) / tex_w;
		ii.t1 = (float) (y+0.5 + t->th) / tex_h;

		// add that description (ii) to the the list (tc)
		stb_arr_push(tc, ii);

		// copy the pixel data into the appropriate place in the tempbuf
		for (k=0; k < t->th+1; ++k)
		{
			for (j=0; j < t->tw+1; ++j)
			{
				// premultiply the alpha... should have done this BEFORE mipmapping, whoops!
				unsigned char *out = tempbuf + (x+j)*4 + (y+k)*tex_w*4;
				out[0] = stb__Mul8Bit(pixels[0], pixels[3]);
				out[1] = stb__Mul8Bit(pixels[1], pixels[3]);
				out[2] = stb__Mul8Bit(pixels[2], pixels[3]);
				out[3] = pixels[3];
				pixels += 4;
			}
		}

		// advance the placement location left-right
		x += t->tw+1;
		if (x == tex_w)
		{
			// at end of row, return to beginning but advance to next row
			x = 0;
			y += t->th+1;
		}
	}

	// create the texture
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempbuf);

	// make the texture non-wraparound (shouldn't ever matter)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// make the texture use bilinear interpolation
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// and return the mapping of where the tiles are placed as texture coordinates
	return tc;
}

// for each of the three sizes of video image tiles, keep track of various
// texture information for it (this should have been an array of structures,
// instead of 4 parallel arrays, but whatever)

uint8 *usedtex[3];       // did it get used this frame?
TileInfo **tileinfo[3];  // the data from read_headers
TextureInfo **tiletc[3]; // the texture coordinates computed by build_texture
GLuint *tiletex[3];      // the OpenGL texture handle

// keep count of how many video frames we've loaded each frame
int cache_count;

// load a video frame at a specific size
void cache_texture(int frame, int mipmap)
{
	// if we've already got it loaded, mark it at used and we're done
	if (tiletex[mipmap][frame])
	{
		usedtex[mipmap][frame] = 2;
		return;
	}

#ifndef FINAL
	// if we've already loaded 8 images this "turn", don't load any more (this makes images load slowly if you fast-forward, which is fine)
	if (cache_count >= 8) return;
#endif

	// record that we've loaded another image this turn
	++cache_count;

	// now actually load it:
	{
		int n;
		char *dir[3] = { "f:/wtf/alpha_tiles", "f:/wtf/alpha_4x_tiles", "f:/wtf/alpha_16x_tiles" };
		// load the pixel data
		uint8 *data = stb_file(stb_sprintf("%s/wtf%04d.tile", dir[mipmap], frame), &n);
		GLuint tex;
		glGenTextures(1, &tex);  // create a new texture handle
		tiletex[mipmap][frame] = tex; // record the new texture handle
		tiletc[mipmap][frame] = build_texture(tex, data, tileinfo[mipmap][frame]); // build a packed texture into the opengl handle
		free(data); // deallocate the pixel data, since OpenGL copied it
	}
	// mark that it's in use
	usedtex[mipmap][frame] = 2;
}

// go through the cache of loaded images, and if we haven't used one
// in the last two frames, deallocate it
void update_cache(void)
{
	int i,j;
	for (j=0; j < 3; ++j)
	{
		for (i=1; i <= 5127; ++i)
		{
			if (usedtex[j][i])
			{
				--usedtex[j][i];
				if (!usedtex[j][i])
				{
					stb_arr_free(tiletc[j][i]);
					glDeleteTextures(1, &tiletex[j][i]);
					tiletex[j][i] = 0;
				}
			}
		}
	}
}

// draw a video frame at a specified location in 3d:
//     frame is the video frame nummber from the okgo video (after deleting frames introduced by 2:3 pulldown)
//     mipmap is the size of the video frame to use (0=original, 1=half in each dimension, 2=quarter in each dimension)
//     x,y,z is the top-left corner
//     sx,sy,sz is a vector to the top-right corner
//     tx,ty,tz is a vector to the bottom-left corner
//     has_a is a flag for whether there's a multiplicative alpha being applied which means we shouldn't ever use alpha-test
void draw_frame_core(int frame, int mipmap, float x, float y, float z, float sx, float sy, float sz, float tx, float ty, float tz, int has_a)
{
	int i;
	static float isize[3][2] = { { 1.0/20, 1.0/12 }, { 1.0/10,1.0/6 }, { 1.0/10, 1.0/6 } };
	TileInfo *t;
	TextureInfo *tc;
	cache_texture(frame, mipmap);
	t = tileinfo[mipmap][frame];
	tc = tiletc[mipmap][frame];
	if (tc == NULL) return;
	glDisable(GL_CULL_FACE);
	glBindTexture(GL_TEXTURE_2D, tiletex[mipmap][frame]);
	glEnable(GL_TEXTURE_2D);

	if (!computing_shadows)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		// for speed, turn on alpha test, but only if the alpha is totally transparent
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0);
	}
	else
	{
		// if we're computing shadows, we're just writing the depth buffer,
		// so we want to use alpha test
		glDisable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5);
	}

	// now draw all the tiles from this frame
	glBegin(GL_QUADS);
	for (i=0; i < t->num_tiles; ++i)
	{
		// get relative position of tile in tilespace, and scale it to imagespace
		float ps0 = (float) t->tilepos[i][0] * isize[mipmap][0];
		float pt0 = (float) t->tilepos[i][1] * isize[mipmap][1];
		float ps1 = (float) (t->tilepos[i][0]+1) * isize[mipmap][0];
		float pt1 = (float) (t->tilepos[i][1]+1) * isize[mipmap][1];

		glTexCoord2f(tc->s0, tc->t0);
		glVertex3f(x + ps0*sx + pt0*tx, y + ps0*sy + pt0*ty, z + ps0*sz + pt0*tz);
		glTexCoord2f(tc->s1, tc->t0);
		glVertex3f(x + ps1*sx + pt0*tx, y + ps1*sy + pt0*ty, z + ps1*sz + pt0*tz);
		glTexCoord2f(tc->s1, tc->t1);
		glVertex3f(x + ps1*sx + pt1*tx, y + ps1*sy + pt1*ty, z + ps1*sz + pt1*tz);
		glTexCoord2f(tc->s0, tc->t1);
		glVertex3f(x + ps0*sx + pt1*tx, y + ps0*sy + pt1*ty, z + ps0*sz + pt1*tz);
		++tc;
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
}

// hey look, ANOTHER way of storing the current camera location
vec campos;

// a description of a location for a video frame while we're drawing things
typedef struct
{
	int frame;
	int mipmap;
	vec origin;
	vec s_vec;
	vec t_vec;
	float rgba[4];
	float dist;
} FrameInstance;

// a list of all the video frames to draw this frame
FrameInstance *frame_queue;

// like draw_frame_raw, but we don't actually draw, we just add it to a list so we can sort it before drawing,
// which lets us avoid depth artifacts and lets us give priority to loading the ones closest to the camera
void draw_frame(int frame, int mipmap, vec where, vec ds, vec dt, float *rgb, float a, float dist)
{
	vec norm;
	FrameInstance f;

	// at the end of the video, we play the last 9 frames in a back-and-forth loop so
	// that there's still some "life" to it rather than totally static. I ended up fading
	// it out before we get to this point, so it doesn't matter.
	int n = 5114;
	if (frame >= n-7)
	{
		int f = (frame-(n-7)) & 15;
		if (f <= 8)
			frame = n-7 + f;
		else
			frame = n-7 + (9+7-f);
	}

	// record the frame info
	f.frame = frame;
	f.mipmap = mipmap;
	f.origin = where;
	f.s_vec = ds;
	f.t_vec = dt;
	if (rgb)
	{
		f.rgba[0] = rgb[0];
		f.rgba[1] = rgb[1];
		f.rgba[2] = rgb[2];
	}
	else
		f.rgba[0] = f.rgba[1] = f.rgba[2] = 1;
	f.rgba[3] = a;

	// compute the normal
	vec_cross(&norm, &ds, &dt);
	vec_normeq(&norm);

	// record the distance from the camera along the surface norm of the video frame,
	// which seems good enough for sorting the stuff I've made, even though you can easily construct
	// cases where it breaks (you're nearly in the plane of some far-off stuff, but
	// off-plane of some nearer stuff)
	f.dist = fabs(vec_dot(&norm, &where) - vec_dot(&norm, &campos));

	// add to the list
	stb_arr_push(frame_queue, f);
}

// when we've finished with "drawing" all the video frames, we call this function
// which actually renders them all
void draw_queue(void)
{
	int i;

	// sort by the distance computed above
	qsort(frame_queue, stb_arr_len(frame_queue), sizeof(*frame_queue), stb_floatcmp(offsetof(FrameInstance, dist)));

	// go through from nearest to furthest and force the frame into cache; this
	// guarantees that when it hits the cache limit for this turn, it will have
	// favored loading nearer images
	for (i=0; i < stb_arr_len(frame_queue); ++i)
		cache_texture(frame_queue[i].frame, frame_queue[i].mipmap);

	// now go through in order from furthest to nearest and actually draw them;
	// furthest to nearest is the natural "compositing" order
	for (i=stb_arr_len(frame_queue)-1; i >= 0; --i)
	{
		FrameInstance *f = &frame_queue[i];
		glColor4fv(f->rgba);
		draw_frame_core(f->frame, f->mipmap, f->origin.x, f->origin.y, f->origin.z,
		                f->s_vec.x, f->s_vec.y, f->s_vec.z, f->t_vec.x, f->t_vec.y, f->t_vec.z,
		                f->rgba[3] != 1.0);
	}
}

// clear the frame and cache_count so next turn will work from scratch
void flush_queue(void)
{
	stb_arr_setlen(frame_queue, 0);
	cache_count = 0;
}

//////////////////////////////////////////////////////////////////////////////

// the map represents the bumpy checkeboard ground, which only shows
// up significantly at the end
#define MAP_SIZE 256

float map[MAP_SIZE][MAP_SIZE]; // the map data from photoshop, blurred
float map2[MAP_SIZE][MAP_SIZE]; // a temporary buffer

// how to map from a position in the map to a position in the world.
// this is non-trivial because the map stuff was added *after* the
// video frame positions were laid out, so I had to shift the map
// around to match.
#define MAP_TO_WORLD(n)   (((n)-128)*50)
#define MAP_TO_WORLDX(n)  (((n)-160)*50)
#define WORLD_TO_MAP(n)   (((n)/50.0f)+128)
#define WORLD_TO_MAPX(n)   (((n)/50.0f)+160)

// compute the height of a point on the map, bilerping from the
// map sample points
float mapheight(float x, float y)
{
	float h0,h1;
	int i,j;
	x = WORLD_TO_MAPX(x);
	y = WORLD_TO_MAP(y);
	if (x <= 0 || y <= 0 || x >= 255 || y >= 255) return 0;
	i = (int) x;
	j = (int) y;
	x -= i;
	y -= j;
	h0 = stb_lerp(x, map[j][i], map[j][i+1]);
	h1 = stb_lerp(x, map[j+1][i], map[j+1][i+1]);
	return stb_lerp(y, h0, h1);
}

// uses central differences to compute a normal for a map square
void mapfacet(vec *v, int i, int j)
{
	// compute a surface normal for this facet
	v->x = -(map[j][i+1] - map[j][i-1]);
	v->y = -(map[j+1][i] - map[j-1][i]);
	v->z = 100;
	vec_normeq(v);
}

// compute the normal to the surface of the map at an arbitrary
// point, by bilerping the values at the corners.
void mapnorm(vec *v, float x, float y)
{
	int i,j;
	vec a,b,c,d;

	x = WORLD_TO_MAPX(x);
	y = WORLD_TO_MAP(y);

	// convert from corner coordinates to facet coordinates... facet 0,0 corresponds to vertex 0.5,0.5
	x -= 0.5;
	y -= 0.5;

	i = (int) x;
	j = (int) y;
	x -= i;
	y -= j;

	mapfacet(&a, i,j);
	mapfacet(&b, i+1,j);
	mapfacet(&c, i,j+1);
	mapfacet(&d, i+1,j+1);

	vec_scale(v, &a, (1-x)*(1-y));
	vec_addeq_scale(v, &b, x*(1-y));
	vec_addeq_scale(v, &c, (1-x)*y);
	vec_addeq_scale(v, &d, x*y);

	vec_normeq(v);
}

// an earlier attempt at the above which used offset differences
// rather than centered differences
void mapnorm2(vec *v, float x, float y)
{
	int i,j;
	float dx00,dx01,dx10,dx11;
	float dy00,dy01,dy10,dy11;
	float dx0,dx1,dy0,dy1;

	float dx = (mapheight(x+20,y) - mapheight(x-20,y));
	float dy = (mapheight(x,y+20) - mapheight(x,y-20));
	x = WORLD_TO_MAPX(x);
	y = WORLD_TO_MAP(y);

	// convert from corner coordinates to facet coordinates... facet 0,0 corresponds to vertex 0.5,0.5
	x += 0.5;
	y += 0.5;
	i = (int) x;
	j = (int) y;
	x -= i;
	y -= j;

	dx00 = (map[j-1][i] - map[j-1][i-1]);
	dx01 = (map[j-1][i+1] - map[j-1][i]);
	dx10 = (map[j][i] - map[j][i-1]);
	dx11 = (map[j][i+1] - map[j][i]);

	dy00 = (map[j][i-1] - map[j-1][i-1]);
	dy01 = (map[j][i] - map[j-1][i]);
	dy10 = (map[j+1][i-1] - map[j][i-1]);
	dy11 = (map[j+1][i] - map[j][i]);

	dx0 = dx00 + x * (dx01 - dx00);
	dx1 = dx10 + x * (dx11 - dx10);
	dy0 = dy00 + x * (dy01 - dy00);
	dy1 = dy10 + x * (dy11 - dy10);

	dx = dx0 + y * (dx1 - dx0);
	dy = dy0 + y * (dy1 - dy0);

	*v = vec3(-dx,-dy,50);
	vec_normeq(v);
}

// ground textures for the map
GLuint test_tex, test_tex2;

// is the video playback paused (e.g. for editing)?
int pause=1;

// the current time in the video, in frames
#if 1
float raw_when = 4120;
#else
float raw_when = 600;
#endif

// the current time in the video in frames?
float when;

// the state of various MIDI inputs
int state[128];

// map control inputs to numbers from 0..1
// (we use pairs of controls on the NanoKontrol to get more precision)
float xval(int a, int b)
{
	return (state[a] + (state[b]-63)/40.0)/127.0;
}

// like the above, but outputs -1 to 1
float xvals(int a, int b)
{
	return 2*xval(a,b) - 1;
}

// A data structure encoding the location of a frame of the video
typedef struct
{
	vec o,s,t,n, step;
} Reference;

// the initial video sequence
Reference initial[24*60];

// the data for the main sequence
Reference built[6000];


// the following variables control the very initial bit where
// we don't do the effect for a few frames. i think
int opening_offset;
int opening_end;

// this structure encodes keyframes for the camera position/angle
typedef struct
{
	int frame;
	float camloc[4];
	float camang[3];
	vec pos,goal;
	vec pos_tan, goal_tan;
	float dist;
} KeyFrame;

// all the current keyframes
KeyFrame *keyframes;

// save the keyframes to disk; this was disabled once I locked down
// the keyframes so I couldn't accidentally change them
void save_keyframes(void)
{
#if 0
	FILE *f = fopen("keyframes.dat", "w");
	int i;
	for (i=0; i < stb_arr_len(keyframes); ++i)
		fprintf(f, "%d %f %f %f %f %f %f %f\n", keyframes[i].frame,
		        keyframes[i].camloc[0],
		        keyframes[i].camloc[1],
		        keyframes[i].camloc[2],
		        keyframes[i].camloc[3],
		        keyframes[i].camang[0],
		        keyframes[i].camang[1],
		        keyframes[i].camang[2]);
	fclose(f);
#endif
}

// find the nearest keyframe before the current frame
int get_keyframe(int n)
{
	int i, best = -1, dist=99999;
	for (i=0; i < stb_arr_len(keyframes); ++i)
	{
		int dt = n - keyframes[i].frame;
		if (dt >= 0 && dt < dist)    // actually they're sorted so we can just grab the first one
		{
			best = i;
			dist = dt;
		}
	}
	return best;
}

// get the real frame for the video. this used to actually do
// real computation, e.g. when the system was spline based,
// but now it just loads from the pre-built list
void eval_frame(float w, vec *o, vec *s, vec *t, vec *step)
{
	int i = (int) w;
	// we have no sub-precision eval here
	if (i >= 700 && i < 6000)
	{
		*o = built[i].o;
		*s = built[i].s;
		*t = built[i].t;
		*step = built[i].step;
		return;
	}
	o->x = 0;
	o->y = 0;
	o->z = 36;
	s->x = 64;
	s->y = 0;
	s->z = 0;
	t->x = 0;
	t->y = 0;
	t->z = -36;
	step->x = step->y = step->z = 0;
}

// as above, but returns the result relative to the center of
// the video, instead of the bottom left
void eval_frame_bottom_center(float w, vec *o, vec *s, vec *t)
{
	vec dummy;
	eval_frame(w,o,s,t, &dummy);
	vec_addeq_scale(o,s,0.5);
	vec_addeq_scale(o,t,1);
}

// vector rotation
void vec_rotate_vec(vec *out, vec *axis, float ang, vec *in)
{
	mat3 m;
	mat3_rotation_around_vec(&m, axis, ang);
	mat3_vec_mul(out, &m, in);
}

// given a keyframe, compute the camera for that frame (which
// means evaluating the positiong of the video frames, which
// are used for the 'lookat' target, and then the camera is derived
// from that)
void compute_keyframe_lookat_raw(int f, vec *cam, vec *target, vec *step)
{
	vec o,s,t,dir;

	KeyFrame *k = &keyframes[f];

	eval_frame(k->frame, &o, &s, &t, step);
	vec_add_scale(target, &o, &s, 0.5);
	vec_addeq_scale(target, &t, 0.5);

	vec_cross(&dir, &s, &t);
	vec_normeq(&dir);

	vec_addeq_scale(target, &dir, -k->camloc[1]);
	target->z += k->camloc[2];

	vec_rotate_vec(&dir, &s, k->camang[0]*3.141592/180, &dir);
	vec_rotate_vec(&dir, &t, k->camang[2]*3.141592/180, &dir);

	vec_add_scale(cam, target, &dir, -k->camloc[3]);
}

// as above, but hey, the data is just loaded from the keyframe
void compute_keyframe_lookat(int f, vec *cam, vec *goal, vec *cam_tan, vec *goal_tan)
{
	*cam = keyframes[f].pos;
	*cam_tan = keyframes[f].pos_tan;

	*goal = keyframes[f].goal;
	*goal_tan = keyframes[f].goal_tan;
}

// compute the "natural cubic spline" for the set of camera keyframes
// ( http://en.wikipedia.org/wiki/Spline_interpolation#Algorithm_to_find_the_interpolating_cubic_spline )
void solve_keyframes(void)
{
	vec *m;
	int i, n = stb_arr_len(keyframes);

	// compute the actual data needed for the evaluation
	for (i=0; i < n; ++i)
	{
		vec dummy;
		compute_keyframe_lookat_raw(i, &keyframes[i].pos, &keyframes[i].goal, &dummy);
		keyframes[i].dist = (i+1 < n ? keyframes[i+1].frame - keyframes[i].frame : 1);
	}

	// now solve tridiagonal linear equation for 1..n-2
	m = malloc(sizeof(*m) * n);

	// store a, b, c' at ends to simplify
	m[0].x = 0;
	m[0].y = 0;
	m[0].z = 0;
	m[n-1].x = 0;
	m[n-1].y = 0;
	m[n-1].z = 0;

	keyframes[0].pos_tan = keyframes[0].goal_tan
	                       = keyframes[n-1].pos_tan = keyframes[n-1].goal_tan = vec_zero();

	// compute d values for 1..n-2
	// d(i) = 6*((y(i+1)-y(i))/h(i) - ((y(i)-y(i-1))/h(i-1))
	for (i=1; i < n-1; ++i)
	{
		float left = 6.0 / keyframes[i].dist;
		float right = 6.0 / keyframes[i-1].dist;

		vec t;

		vec_sub(&t, &keyframes[i+1].pos, &keyframes[i].pos);
		vec_scale(&keyframes[i].pos_tan, &t, left);
		vec_sub(&t, &keyframes[i].pos, &keyframes[i-1].pos);
		vec_addeq_scale(&keyframes[i].pos_tan, &t, -right);

		vec_sub(&t, &keyframes[i+1].goal, &keyframes[i].goal);
		vec_scale(&keyframes[i].goal_tan, &t, left);
		vec_sub(&t, &keyframes[i].goal, &keyframes[i-1].goal);
		vec_addeq_scale(&keyframes[i].goal_tan, &t, -right);
	}

	// do first row modify

	// a(i) = h(i-1)
	// b(i) = 2*(h(i-1) + h(i))
	// c(i) = h(i)

	m[1].x = 0; // a
	m[1].y = 2*(keyframes[0].dist + keyframes[1].dist); // b
	m[1].z = keyframes[1].dist / m[1].y; // c'

	// compute a,b,c' for remaining rows
	for (i=2; i < n-1; ++i)
	{
		m[i].x = keyframes[i-1].dist;
		m[i].y = 2*(keyframes[i-1].dist + keyframes[i].dist);
		m[i].z = keyframes[i].dist / (m[i].y - m[i-1].z*m[i].x);
	}

	// now compute forward sweep
	for (i=1; i < n-1; ++i)
	{
		float div = (i==1 ? m[i].y : (m[i].y - m[i-1].z*m[i].x));
		float scale = 1.0 / div;
		assert(div != 0);
		if (i == 1)
		{
			vec_scaleeq(&keyframes[i].pos_tan, scale);
			vec_scaleeq(&keyframes[i].goal_tan, scale);
		}
		else
		{
			vec_addeq_scale(&keyframes[i].pos_tan, &keyframes[i-1].pos_tan, -m[i].x);
			vec_addeq_scale(&keyframes[i].goal_tan, &keyframes[i-1].goal_tan, -m[i].x);
			vec_scaleeq(&keyframes[i].pos_tan, scale);
			vec_scaleeq(&keyframes[i].goal_tan, scale);
		}
	}

	// now perform back substitution
	for (i=n-3; i >= 1; --i)
	{
		vec_addeq_scale(&keyframes[i].pos_tan , &keyframes[i+1].pos_tan , -m[i].z);
		vec_addeq_scale(&keyframes[i].goal_tan, &keyframes[i+1].goal_tan, -m[i].z);
	}

	free(m);
}

// load the keyframes from disk
void load_keyframes(void)
{
	FILE *f = fopen("keyframes.dat", "r");
	keyframes = NULL;
	if (!f) return;
	while (!feof(f))
	{
		KeyFrame k;
		if (fscanf(f, "%d %f %f %f %f %f %f %f\n", &k.frame,
		           &k.camloc[0],
		           &k.camloc[1],
		           &k.camloc[2],
		           &k.camloc[3],
		           &k.camang[0],
		           &k.camang[1],
		           &k.camang[2])==8)
		{
			stb_arr_push(keyframes, k);
		}
	}
	fclose(f);

	solve_keyframes();
}

// while editing the keyframes, this is which keyframe # and which frame #
int editing_frame = -1, editing_keyframe = -1;

// start editing a particular frame; creates a keyframe if needed
void edit_keyframe(int n)
{
	int z = get_keyframe(n);
	if (z < 0 || keyframes[z].frame != n)
	{
		KeyFrame k;
		k.frame = n;
		memcpy(k.camloc, camloc, sizeof(k.camloc));
		memcpy(k.camang, camang, sizeof(k.camang));
		stb_arr_push(keyframes, k);
		qsort(keyframes, stb_arr_len(keyframes), sizeof(*keyframes), stb_intcmp(offsetof(KeyFrame,frame)));
		z = get_keyframe(n);
		assert(n >= 0 && keyframes[z].frame == n);
	}
	memcpy(camloc, keyframes[z].camloc, sizeof(camloc));
	memcpy(camang, keyframes[z].camang, sizeof(camang));
	editing_frame = n;
	editing_keyframe = z;
	raw_when = n;
}

// advance to next keyframe
void goto_next_keyframe(void)
{
	int n = get_keyframe((int) when);
	if (n >= 0)
	{
		// if frame[n] < when, then we're in between, so advance to next;
		// if frame[n] == when, we're on n, so advance
		++n;
		if (n < stb_arr_len(keyframes))
		{
			edit_keyframe(keyframes[n].frame);
		}
	}
}

// go back to previous key frame
void goto_prev_keyframe(void)
{
	int n = get_keyframe((int) when);
	if (n >= 0)
	{
		if (keyframes[n].frame == (int) when)
			--n;
		if (n >= 0)
			edit_keyframe(keyframes[n].frame);
	}
}



// keep track of whether any particular MIDI control has changed;
// since the controller positions don't match the stored values
// for keyframes, we only want to use the controller positions if
// they've changed
uint8 changed[128];

#define delay_start 80

// do processing given "time passing", e.g. possibly a new raw_when
// value, and possibly some MIDI input
void update_state(void)
{
	when = (int) raw_when;
	if (when < 0) when = 0;

	if (pause)
	{
		// Korg nanoKontrol sliders/knobs
		if (changed[15] || changed[3 ]) camloc[1] = 100 * xvals(15,3);// + when*SPACING;
		if (changed[22] || changed[10]) camloc[2] = 80 * xval(22,10) - 20;
		if (changed[14] || changed[2 ]) camloc[3] = 200 * xval(14,2) + 0.5;

		if (changed[16] || changed[4 ]) camang[0] = xvals(16,4)*90;
		if (changed[17] || changed[5 ]) camang[2] = xvals(17,5)*180;

		if (editing_keyframe >= 0)
		{
			memcpy(keyframes[editing_keyframe].camloc, camloc, sizeof(keyframes[0].camloc));
			memcpy(keyframes[editing_keyframe].camang, camang, sizeof(keyframes[0].camang));
			solve_keyframes();
		}
		memset(changed, 0, sizeof(changed));
	}

	if (when < delay_start)
	{
		opening_offset = when + 1;
		opening_end = 0;
	}
	else
	{
		opening_offset = delay_start+1;
		opening_end = when-delay_start;

		if (opening_end > 660)
			opening_end = 660;
	}
}

// rate at which we're fast-forwarding or rewinding
float advancing, rewinding;

// update video time based on ffwd/rewind controls
void ui_sim(float dt)
{
	raw_when += advancing * dt;
	raw_when -= rewinding * dt;
	update_state();
}

// update video time based on real wall time advancing
void simulate(float dt)
{
	//when += dt * 0.5;
	//when += dt * 4;
	raw_when += dt * 24.0; // 23.98

	update_state();
}

// store information about various kinds of beats being triggered on various frames
// (we only actually have data for type '23' and type '33', which numbers arise from
// the MIDI controllers I used to input them, I think).
uint8 beats[6000][128];

// the display quality setting; if quality is 0, we draw the video frames
// at lower quality, to make the loading and display faster
int quality=0;

// these are offsets to correct for errors in the input
int delta_33 = -2;
int delta_23 = 0;

// a variable used for various runtime tweaking
int hack;

// this is some number whose purpose is lost in the sands of time
#define switch_offset 350


// s&t vectors should be computed implicitly from path
// (and then a little extra rotation can be applied)

// computes a cubic bezier on a 3-vector
void compute_vec_bezier(vec p[4], float t, vec *out)
{
	int i;
	float s = 1-t;
	for (i=0; i < 3; ++i)
	{
		(&out->x)[i] =   s*s*s*(&p[0].x)[i]
		                 + 3*t*s*s*(&p[1].x)[i]
		                 + 3*t*t*s*(&p[2].x)[i]
		                 +   t*t*t*(&p[3].x)[i];
	}
}

// computes the derivative with respect to time of a cubic bezier on a 3-vector
void compute_vec_bezier_derivative(vec p[4], float t, vec *out)
{
	int i;
	float s = 1-t;
	// (1-t)^3 => -3*(1-t)^2
	// (1-t)^2*t => -2*(1-t)*t + (1-t)^2
	// (1-t)^1*t^2 => t*t + 2*(1-t)*t
	// t^3 = 3*t^2
	for (i=0; i < 3; ++i)
	{
		(&out->x)[i] = -3*s*s*(&p[0].x)[i]
		               + 3*(-2*s*t+s*s)*(&p[1].x)[i]
		               + 3*( 2*s*t-t*t)*(&p[2].x)[i]
		               +  3*t*t*(&p[3].x)[i];
	}
}

// stores a "keyframe" in the path of the video frames; video frames aren't
// splined, but follow along simple paths with constant velocity and
// constant rotational velocity, plus brief transition regions
typedef struct
{
	int when;
	int transition;
	float rot;
	vec step; // in relative coordinates
	float w,h;
} PathNode;

// the following has the path of all the video frames of the "main sequence"
// (the path where you see video frames into the future, not from the past)

PathNode main_path[20] =
{
	{ 700,0, 0, { 0,2,0 }, 64,36},
	{ 720,0, 0, { 0,2,0 } },
	{ 740,50, 0, { 0,2,0 } },

	{ 940,25, 0, { 0,1,0 } }, // slow down

	{ 1182,15, 0, { 0,2,0 } }, // speed back up
	{ 1382,15, 0, { 0,1,0 } }, // slow back down
	{ 1623,40, 0.25, { 0,1.4,0 } }, // first turn
	{ 1815,40, 0, { 0,1,0 } },

// note to self: to descend, do 360 frames of 0,0,-0.15
	{ 2360,75, 0, { 0,1.5,-0.3 } },
	{ 2540,75, 0, { 0,2.5,0 } }, // speed up after descending

	{ 3300,50, 0.25, { 0,1.5,0 } },
	{ 3400,25, 0.25, { 0,1.0,0 } },

	{ 3500,35, 0, { 0,1,0 } },

	{ 4400,50, 0, { 0,4,0 } }, // speed up at end

	{ 6000,0, 0, { 0,1,0 } },
};

// convert the above data into the list of built info storing
// where all the video frames are located spatially (in the
// array 'built')
void walk_frames(int when)
{
	int i = 700;
	// starting position
	vec origin = { 964+200-54/2, 323-350-34/2, 36*1.5 };
	vec s = { 64, 0, 0 };//54, 34, 0 };
	vec t = { 0,0,-36 };
	float z = 0;
	// this routine supports generating multiple simulatenous paths, unused feature
	PathNode *a[3] = { main_path, NULL, NULL };
	float rot = 0;
	vec_normeq(&s);
	vec_normeq(&t);
	// iterate i through frames
	for (i=700; i < 6000; ++i)
	{
		int j;
		float w[2],h[2],z;
		vec n;
		vec step[2];
		float rot[2];
		// iterate j through
		for (j=0; a[j]; ++j)
		{
			if (a[j][0].when > i)
			{
				step[j] = step[0];
				rot[j] = rot[0];
				w[j] = w[0];
				h[j] = 0;
			}
			else
			{
				if (a[j][1].when <= i)
				{
					++a[j];
					if (a[j][0].w == 0) a[j][0].w = a[j][-1].w;
					if (a[j][0].h == 0) a[j][0].h = a[j][-1].h;
				}
				// when <= i < when+trans
				// if we're during the transition region for this node, then linear interpolate the velocities from the previous node
				if (a[j]->when + a[j]->transition > i)
				{
					float z = (i - a[j]->when) / (float) a[j]->transition;
					assert(i >= a[j]->when && i < a[j]->when + a[j]->transition);
					vec_lerp(&step[j], &a[j][-1].step, &a[j][0].step, z);
					z = 3*z*z - 2*z*z*z; // should this apply to step as well?
					rot[j] = stb_lerp(z, a[j][-1].rot, a[j][0].rot);
					w[j] = stb_lerp(z, a[j][-1].w, a[j][0].w);
					h[j] = stb_lerp(z, a[j][-1].h, a[j][0].h);
					assert(rot[0] >= 0);
				}
				else
				{
					// otherwise just use this node
					assert(i >= a[j]->when && i < a[j][1].when);
					step[j] = a[j]->step;
					rot[j] = a[j]->rot;
					w[j] = a[j]->w;
					h[j] = a[j]->h;
					assert(rot[0] >= 0);
				}
			}
		}
		// if we have two paths, blend them
		if (a[1])
		{
			vec_lerp(&step[0], &step[0], &step[1], z);
			rot[0] = stb_lerp(z, rot[0], rot[1]);
			w[0] = stb_lerp(z, w[0], w[1]);
			h[0] = stb_lerp(z, h[0], h[1]);
		}
		assert(rot[0] >= 0);
		// apply rot and step

		// compute the final "build" position
		vec_scale(&built[i].s, &s, w[0]);
		vec_scale(&built[i].t, &t, h[0]);

		built[i].o = origin;
		z = mapheight(origin.x, origin.y);
		// if position would be underground, update position to be above ground
		// and change orientation so it tilts to match the ground
		if (built[i].o.z < z)
		{
			built[i].o.z = z;
			mapnorm(&n, origin.x, origin.y);
			vec_scale(&built[i].t, &n, -h[0]);
			vec_cross(&n, &s, &n);
			vec_cross(&built[i].s, &n, &built[i].t);
			vec_normeq(&built[i].s);
			vec_scaleeq(&built[i].s, w[0]);
		}
		vec_addeq_scale(&built[i].o, &built[i].t, -1);
		vec_addeq_scale(&built[i].o, &built[i].s, -0.5);

		vec_cross(&n, &s, &t);
		built[i].step = origin;
		vec_addeq_scale(&origin, &n,  step[0].y);
		vec_addeq_scale(&origin, &s,  step[0].x);
		vec_addeq_scale(&origin, &t, -step[0].z);
		vec_sub(&built[i].step, &origin, &built[i].step);

		// apply the rotation for this frame to the "cursor"
		if (rot[0])
			vec_rotate_z(&s, &s, rot[0]*3.141592/180);
	}
}

// we want to do animated morphs between various interesting looking shapes
// we use two canonical representations:
//    the sphere (just normalize a vector)
//    the cube (a face selector and a 2D coordinate)
//
// Cube faces:
//    (1,u,v)      +X
//    (-1,-u,-v)   -X
//    (v,1,u)      +Y
//    (-v,-1,-u)   -Y
//    (u,v,1)      +Z
//    (-u,-v,-1)   -Z

// not used
void sphere_to_cube(int *face, float *u, float *v, vec *point)
{
	float x = fabs(point->x);
	float y = fabs(point->y);
	float z = fabs(point->z);
	if (x >= y && x >= z)
	{
		*u = point->y / point->x;
		*v = point->z / point->x;
		*face = (x > 0) ? 0 : 1;
	}
	else if (y >= x && y >= z)
	{
		*u = point->z / point->y;
		*v = point->x / point->y;
		*face = (y > 0) ? 2 : 3;
	}
	else
	{
		*u = point->x / point->z;
		*v = point->y / point->z;
		*face = (z > 0) ? 4 : 5;
	}
}

// given the parametric description, generate the cube faces
// this is easy since the parametric description describes a cube
void parametric_to_cube(vec *point, vec *normal, int face, float u, float v)
{
	normal->x = normal->y = normal->z = 0;
	u = (u*2)-1;
	v = (v*2)-1;
	if (face == 0 || face == 1)
	{
		point->x = (face == 0) ? 1 : -1;
		point->y = point->x * u;
		point->z = v;
		normal->x = point->x;
	}
	else if (face == 2 || face == 3)
	{
		point->y = (face == 2) ? 1 : -1;
		point->z = point->y * u;
		point->x = v;
		normal->y = point->y;
	}
	else
	{
		point->z = (face == 4) ? 1 : -1;
		point->x = point->z * u;
		point->y = v;
		normal->z = point->z;
	}
}

// given the parametric description, generate a sphere
// this is easy since we just normalize the point
void parametric_to_sphere(vec *point, vec *normal, int face, float u, float v)
{
	parametric_to_cube(point, normal, face, u, v);
	vec_normeq(point);
	*normal = *point;
}

// given the parametric description, generate a cylinder.
// the top and bottom faces expand out to a disk, and the
// sides expand out to something with a circular cross-section,
// which is like the sphere but only along two axes
void parametric_to_cylinder(vec *point, vec *normal, int face, float u, float v)
{
	parametric_to_cube(point, normal, face, u, v);
	if (face < 4)
	{
		// do a 2d normalize to get a cylinder
		float d = sqrt(point->x*point->x + point->y * point->y);
		point->x /= d;
		point->y /= d;
		*normal = *point;
		normal->z = 0;
	}
	else
	{
		// do same normalization as above along edges, but
		// equivalent rescaling along vertices
		float d = sqrt(point->x*point->x + point->y * point->y);
		float m = stb_max(fabs(point->x), fabs(point->y));
		if (d)
		{
			point->x *= m/d;
			point->y *= m/d;
		}
		// normal points up or down
		normal->x = 0;
		normal->y = 0;
		normal->z = point->z;
	}
}

float capsule_rounding = 1;

// (notused)
// given the parametric representation, generate a capsule (a
// cylinder with semispheres as endcaps)
void parametric_to_capsule(vec *point, vec *normal, int face, float u, float v)
{
	float d;
	parametric_to_cylinder(point, normal, face, u, v);
	// project out to a hemisphere on each side
	if (face == 4)
	{
		// x^2 + y^2 + z^2 = 1
		// z^2 = 1 - x^2 - y^2
		// z = sqrt(1 - x^2 - y^2)
		d = 1 - point->x * point->x - point->y * point->y;
		if (d < 0) d = 0;
		point->z = 1 + sqrt(d) * capsule_rounding;
		*normal = *point;
		normal->z = sqrt(d);
		vec_normeq(normal);
	}
	else if (face == 5)
	{
		d = 1 - point->x * point->x - point->y * point->y;
		if (d < 0) d = 0;
		point->z = -1 - sqrt(d) * capsule_rounding;
		*normal = *point;
		normal->z = -sqrt(d);
		vec_normeq(normal);
	}
}


// given the parametric representation, generate a cone;
// first generate a cylinder, then rescale it from top to
// bottom. we stop a little short from the top to avoid
// a singularity, so you get a little visible chopped off tip
void parametric_to_cone(vec *point, vec *normal, int face, float u, float v)
{
	float w;
	parametric_to_cylinder(point, normal, face, u, v);
	w = stb_linear_remap(point->z, -1, 1, 1, 0.125);
	point->x *= w;
	point->y *= w;
	// hacked approximate normal
	if (normal->z == 0)
	{
		normal->z =  0.5;
		vec_normeq(normal);
	}
}

// (notused)
// given the parametric representation, generate a four-sided
// pyramid; this works just like the above, but using a cube
// instead of a cone
void parametric_to_pyramid(vec *point, vec *normal, int face, float u, float v)
{
	float w;
	parametric_to_cube(point, normal, face, u, v);
	w = stb_linear_remap(point->z, -1, 1, 1, 1.0/32);
	point->x *= w;
	point->y *= w;
	// hacked approximate normal
	if (normal->z == 0)
	{
		normal->z =  0.5;
		vec_normeq(normal);
	}
}

typedef struct
{
	vec n;
	float w;
} plane;

// (notused)
// given the parametric representation, generate an arbitrary
// convex polyhedron, by projecting to a sphere and then mapping
// each point on the sphere to the nearest plane. the exact
// positions of the planes (the size of the polyhedron) affects
// the results somehow
void parametric_to_solid(vec *point, vec *normal, int face, float u, float v, int num_planes, plane *p)
{
	float best_d, t;
	int i, best = 0;
	parametric_to_sphere(point, normal, face, u, v);
	best_d = 100000;
	for (i=0; i < num_planes; ++i)
	{
		float d = fabs(vec_dot(point, &p[i].n) + p[i].w);
		if (d < best_d)
		{
			best_d = d;
			best = i;
		}
	}
	t = vec_dot(point, &p[best].n) + p[best].w;
	vec_addeq_scale(point, &p[best].n, -t);
	vec_norm(normal, &p[best].n);
}

// (notused)
// define the planes for the 5 platonic solids except the cube
plane tetrahedron[4] =
{
	{ 1,1,1, 0.8 },
	{ 1,-1,-1, 0.8 },
	{ -1,1,-1, 0.8 },
	{ -1,-1, 1, 0.8 },
};
plane octahedron[8];
plane icosahedron[20], icosahedron2[20];
plane dodecahedron[12];

float pdist;
plane make_plane(float x, float y, float z)
{
	plane p;
	p.n.x = x;
	p.n.y = y;
	p.n.z = z;
	vec_normeq(&p.n);
	p.w = -pdist;
	return p;
}

// (notused)
// define the planes for the 5 platonic solids except the cube
// we make two icosahedrons with different plane distances because
// the output results are interesting
void init_platonics(void)
{
	int i;
	float phi,a,b,c;
	for (i=0; i < 4; ++i)
		vec_normeq(&tetrahedron[i].n);

	pdist = 0.85;
	octahedron[0] = make_plane(1,1,1);
	octahedron[1] = make_plane(1,1,-1);
	octahedron[2] = make_plane(1,-1,1);
	octahedron[3] = make_plane(1,-1,-1);
	octahedron[4] = make_plane(-1,1,1);
	octahedron[5] = make_plane(-1,1,-1);
	octahedron[6] = make_plane(-1,-1,1);
	octahedron[7] = make_plane(-1,-1,-1);

	phi = (1 + sqrt(5)) / 2;
	b = 1.0 / phi;
	c = 2- phi;

	pdist = 0.95;
	icosahedron[ 0] = make_plane(c,0,1);
	icosahedron[ 1] = make_plane(-c,0,1);
	icosahedron[ 2] = make_plane(c,0,-1);
	icosahedron[ 3] = make_plane(-c,0,-1);
	icosahedron[ 4] = make_plane(0,1,c);
	icosahedron[ 5] = make_plane(0,1,-c);
	icosahedron[ 6] = make_plane(0,-1,-c);
	icosahedron[ 7] = make_plane(0,-1,-c);
	icosahedron[ 8] = make_plane(1,c,0);
	icosahedron[ 9] = make_plane(-1,c,0);
	icosahedron[10] = make_plane(1,-c,0);
	icosahedron[11] = make_plane(-1,-c,0);
	icosahedron[12] = make_plane(b,b,b);
	icosahedron[13] = make_plane(b,b,-b);
	icosahedron[14] = make_plane(b,-b,b);
	icosahedron[15] = make_plane(b,-b,-b);
	icosahedron[16] = make_plane(-b,b,b);
	icosahedron[17] = make_plane(-b,b,-b);
	icosahedron[18] = make_plane(-b,-b,b);
	icosahedron[19] = make_plane(-b,-b,-b);

	pdist = 0.65;
	icosahedron2[ 0] = make_plane(c,0,1);
	icosahedron2[ 1] = make_plane(-c,0,1);
	icosahedron2[ 2] = make_plane(c,0,-1);
	icosahedron2[ 3] = make_plane(-c,0,-1);
	icosahedron2[ 4] = make_plane(0,1,c);
	icosahedron2[ 5] = make_plane(0,1,-c);
	icosahedron2[ 6] = make_plane(0,-1,-c);
	icosahedron2[ 7] = make_plane(0,-1,-c);
	icosahedron2[ 8] = make_plane(1,c,0);
	icosahedron2[ 9] = make_plane(-1,c,0);
	icosahedron2[10] = make_plane(1,-c,0);
	icosahedron2[11] = make_plane(-1,-c,0);
	icosahedron2[12] = make_plane(b,b,b);
	icosahedron2[13] = make_plane(b,b,-b);
	icosahedron2[14] = make_plane(b,-b,b);
	icosahedron2[15] = make_plane(b,-b,-b);
	icosahedron2[16] = make_plane(-b,b,b);
	icosahedron2[17] = make_plane(-b,b,-b);
	icosahedron2[18] = make_plane(-b,-b,b);
	icosahedron2[19] = make_plane(-b,-b,-b);

	a = 0.5;
	b = 1 / (2 * phi);
	pdist = 1;
	dodecahedron[ 0] = make_plane(0,b,a);
	dodecahedron[ 1] = make_plane(0,-b,a);
	dodecahedron[ 2] = make_plane(0,b,-a);
	dodecahedron[ 3] = make_plane(0,-b,-a);
	dodecahedron[ 4] = make_plane(b,a,0);
	dodecahedron[ 5] = make_plane(-b,a,0);
	dodecahedron[ 6] = make_plane(b,-a,0);
	dodecahedron[ 7] = make_plane(-b,-a,0);
	dodecahedron[ 8] = make_plane(a,0,b);
	dodecahedron[ 9] = make_plane(a,0,-b);
	dodecahedron[10] = make_plane(-a,0,b);
	dodecahedron[11] = make_plane(-a,0,-b);
}

// (notused)
// define the functions to generate the various platonic solids
void parametric_to_tetrahedron(vec *point, vec *normal, int face, float u, float v)
{
	parametric_to_solid(point, normal, face, u, v, 4, tetrahedron);
}

void parametric_to_octahedron(vec *point, vec *normal, int face, float u, float v)
{
	parametric_to_solid(point, normal, face, u, v, 8, octahedron);
}

void parametric_to_icosahedron(vec *point, vec *normal, int face, float u, float v)
{
	parametric_to_solid(point, normal, face, u, v, 20, icosahedron);
}

void parametric_to_icosahedron2(vec *point, vec *normal, int face, float u, float v)
{
	parametric_to_solid(point, normal, face, u, v, 20, icosahedron2);
}

void parametric_to_dodecahedron(vec *point, vec *normal, int face, float u, float v)
{
	parametric_to_solid(point, normal, face, u, v, 12, dodecahedron);
}

// deformers bend space; rather than trying to map along a spline,
// which is kind of a PITA as seen elsewhere, we just hand-author
// a few deformations

// (notused)
// this deformer pushes a cube into a sphere at 1 and a sphere into a cube at -1
void deform_cube_to_sphere(vec *point, float amount)
{
	float maxdist;
	float euclidean;
	float rescale;

	if (amount == 0) return;

	maxdist = fabs(point->x);
	maxdist = stb_max(maxdist, fabs(point->y));
	maxdist = stb_max(maxdist, fabs(point->z));
	euclidean = vec_mag(point);

	if (euclidean == 0 || maxdist == 0) return;

	if (amount > 0)
	{
		// at amount=1, normalize a cube to a sphere

		rescale = stb_lerp(amount, 1, maxdist / euclidean);
	}
	else
	{
		// at amount=-1, normalize a sphere to a cube
		rescale = stb_lerp(-amount, 1, euclidean / maxdist);
	}

	vec_scaleeq(point, rescale);
}

// twist space around the x axis
void deform_twist_x(vec *point, float amount)
{
	float a = point->x * amount;
	float s = sin(a);
	float c = cos(a);
	float t;
	t        = point->y * c - point->z * s;
	point->z = point->y * s + point->z * c;
	point->y = t;
}

// twist space around the y axis
void deform_twist_y(vec *point, float amount)
{
	float a = point->y * amount;
	float s = sin(a);
	float c = cos(a);
	float t;
	t        = point->z * c - point->x * s;
	point->x = point->z * s + point->x * c;
	point->z = t;
}

// twist space around the z axis
void deform_twist_z(vec *point, float amount)
{
	float a = point->z * amount;
	float s = sin(a);
	float c = cos(a);
	float t;
	t        = point->x * c - point->y * s;
	point->y = point->x * s + point->y * c;
	point->x = t;
}

// deform a point according to a deformer. we also
// need to compute a new normal, which we do by
// computing two tangent vectors and computing
// deformed finite differences with them
void deform(vec *point, vec *normal, void (*func)(vec *point, float amount), float amount)
{
	vec dx,dy;
	if (fabs(normal->x) > 0.5)
		dx = vec3(0,1,0);
	else
		dx = vec3(1,0,0);
	vec_cross(&dy, normal, &dx);
	vec_cross(&dx, normal, &dy);

	vec_add_scale(&dx, point, &dx, 0.01);
	vec_add_scale(&dy, point, &dy, 0.01);

	func(point, amount);
	func(&dx, amount);
	func(&dy, amount);

	vec_subeq(&dx, point);
	vec_subeq(&dy, point);

	vec_cross(normal, &dy, &dx);
	vec_normeq(normal);
}


// A shape is a single object which is defined by a mapping
// from parametric to real space, scale factors for each of
// the axes, and a deformer that deforms space after scaling
typedef struct
{
	void (*func)(vec *point, vec *normal, int face, float u, float v);
	vec scale, norm_scale;
	void (*deform)(vec *point, float amount);
	float deform_amount;
} Shape;

// compute a final output point for a shape given the parametric values
void compute_shape(vec *point, vec *normal, Shape *s, int face, float u, float v)
{
	s->func(point, normal, face, u, v);
	point->x *= s->scale.x;
	point->y *= s->scale.y;
	point->z *= s->scale.z;
	normal->x *= s->norm_scale.x;
	normal->y *= s->norm_scale.y;
	normal->z *= s->norm_scale.z;
	vec_normeq(normal);
	if (s->deform)
	{
		deform(point, normal, s->deform, s->deform_amount);
	}
}

// interpolate between two points and normals while morphing
void morph(vec *point, vec *normal, float t, vec *p1, vec *n1, vec *p2, vec *n2)
{
	vec_lerp(point, p1, p2, t);
	// when morphing between shapes, we lag the perturbation of the normal a little bit for slerp-vs-lerp reasons
	vec_lerp(normal, n1, n2, 3*t*t - 2*t*t*t);
}

// a ShapeInstance is an actual renderable thing. it consists
// of two shapes
typedef struct
{
	// the first shape of the morph
	Shape shape_1;
	int frame_1;

	// the second shape of the morph
	Shape shape_2;
	int frame_2;

	// a global scale to apply after everything else
	vec scale;

	// (notused):
	void (*deform)(vec *point, float amount);
	vec deform_displace;
	float deform_shear_z_x;
	float deform_shear_z_y;
	float def_amount_1;
	float def_amount_2;
	int def_frame_1;
	int def_frame_2;
} ShapeInstance;

// compute the correct scale factor to apply to a normal
// given a scale factor to apply to a point
void compute_normal_scale(vec *nscale, vec *scale)
{
	// since normal is cross-product of dx,dy, then a normal along axis
	// i is actually the cross-product of j and k. so if j and k get
	// scaled larger, then the i normal gets scaled larger
	nscale->x = scale->y * scale->z;
	nscale->y = scale->z * scale->x;
	nscale->z = scale->x * scale->y;
	vec_normeq(nscale);
}

// to render a shape, we'll build up a set of all
// the points first, then draw them. we do this
// one parametric face at a time, so we need enough
// storage to store all the points of one face
#define MAX_SLICES  128
vec draw_pt[MAX_SLICES][MAX_SLICES];
vec draw_nm[MAX_SLICES][MAX_SLICES];
vec draw_tc[MAX_SLICES][MAX_SLICES];

// now draw a shape specified by a ShapeInstance
void draw_shape(ShapeInstance *s, int slices, float frame)
{
	int f,i,j;
	float t;
	vec nscale;

	// clamp the parametric space to the storage available
	if (slices+1 > MAX_SLICES)
		slices = MAX_SLICES-1;

	// compute how to scale normals
	compute_normal_scale(&nscale, &s->scale);

	// if morphing, compute the interpolation factor
	if (s->shape_2.func)
	{
		float k = 1.0;
		t = stb_linear_remap(frame, s->frame_1, s->frame_2, 0,1);
		t = (3*t*t - 2*t*t*t)*k + t*(1-k);
	}
	else
		t = 0;

	// iterate through the six parametric faces
	for (f=0; f < 6; ++f)
	{
		// iterate through the slices * slices points
		for (j=0; j <= slices; ++j)
		{
			float v = (float) j / slices;
			for (i=0; i <= slices; ++i)
			{
				float u = (float) i / slices;
				vec p,n;
				if (s->shape_2.func)
				{
					// compute both first and second shapes and morph between them
					vec p1,n1,p2,n2;
					compute_shape(&p1, &n1, &s->shape_1, f,u,v);
					compute_shape(&p2, &n2, &s->shape_2, f,u,v);
					morph(&p, &n, t, &p1,&n1, &p2,&n2);
				}
				else
				{
					// compute the first shape
					compute_shape(&p, &n, &s->shape_1, f,u,v);
				}
				// (notused)
				// compute a final post-morph deformation
				if (s->deform)
				{
					p.x += p.z * s->deform_shear_z_x;
					p.y += p.z * s->deform_shear_z_y;
					vec_subeq(&p, &s->deform_displace);
					deform(&p, &n, s->deform, stb_linear_remap(frame, s->def_frame_1, s->def_frame_2, s->def_amount_1, s->def_amount_2));
					vec_addeq(&p, &s->deform_displace);
					p.x -= p.z * s->deform_shear_z_x;
					p.y -= p.z * s->deform_shear_z_y;
				}
				// apply the final scale
				p.x *= s->scale.x;
				p.y *= s->scale.y;
				p.z *= s->scale.z;
				n.x *= nscale.x;
				n.y *= nscale.y;
				n.z *= nscale.z;
				draw_pt[j][i] = p;
				draw_nm[j][i] = n;
				draw_tc[j][i].x = u;
				draw_tc[j][i].y = v;
				draw_tc[j][i].z = 0;
			}
		}
		// draw the polygons for this face
		for (j=0; j < slices; ++j)
		{
			glBegin(GL_TRIANGLE_STRIP);
			for (i=0; i <= slices; ++i)
			{
				glTexCoord2fv(&draw_tc[j  ][i].x);
				glNormal3fv  (&draw_nm[j  ][i].x);
				glVertex3fv  (&draw_pt[j  ][i].x);

				glTexCoord2fv(&draw_tc[j+1][i].x);
				glNormal3fv  (&draw_nm[j+1][i].x);
				glVertex3fv  (&draw_pt[j+1][i].x);
			}
			glEnd();
		}
	}
}

// the location of the light -- very far away approximates
// infinitely far away
vec light_dir = { 0.65*8000, 0.3*8000, 0.5*8000 };

void prep_lighting(void)
{
	glEnable(GL_LIGHTING);
	stbgl_SimpleLight(0, 1, light_dir.x, light_dir.y, light_dir.z);
	//stbgl_GlobalAmbient(0.3,0.3,0.5);
}

// a white texture
GLuint dummy_tex;

// a map of where we should draw the above shapes
uint8 *object_map;

// the previous lookat location; we use this to approximate
// the camera to determine how much accuracy to draw each shape
// with, so the real-time rendering gets LOD for performance
vec last_lookat;

// pseudo-random number generator used to generate coherent
// values for the shape animations so we can render them statelessly
// (allows the editor to wind back and forth freely over them)
float prand[4096];
int prandi[4096];

// the pixel shader for rendering stuff that receives shadows
GLuint shadow_prog;

// if using the above pixel shader, we need to set the color
// specially so the shader can compute lighting properly
void setcolor(float r, float g, float b)
{
	GLint result = glGetUniformLocationARB(shadow_prog, "mat_color");
	assert(result >= 0);
	glUniform4fARB(result, r,g,b,1);
}

// when doing shadows, especially cascaded shadows, we end up
// drawing all the objects as many as 4 times. so we want to
// cache the current state of the shapes and render that cached
// shape efficiently. we use OpenGL display lists to do this,
// simply caching the results from one point in time and reusing
// that if possible.
static float cached_when = -999;
GLuint cached_list;

// draw all the active shapes
void draw_shapes(void)
{
	int base = (int) raw_when;
	float phase = raw_when - base;
	float trans = 0;
	int prev,next;
	int i,j;
	int thresh;

	prep_lighting();
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, dummy_tex);
	glEnable(GL_TEXTURE_2D);

	// check if we need to really rebuild them, or we can use the cached version
	if (raw_when != cached_when)
	{
		// rebuild, so delete the old one
		if (cached_list)
			glDeleteLists(cached_list, 1);
		// start a new one
		cached_list = glGenLists(1);
		glNewList(cached_list, GL_COMPILE);

		// at various times through the video, more objects become visible. we
		// determine this by checking what color they were painted in the object
		// map, and using that color to determine when they become visible
		if (raw_when < 2300)
			thresh = 100;
		else if (raw_when < 3187)
			thresh = 160;
		else if (raw_when < 3516)
			thresh = 176;
		else if (raw_when < 4330)
			thresh = 192;
		else
			thresh = 256;

		// 'prev' and 'next' are the previous and next "morph keyframes" (which
		// occur at measure boundaries). we'll use these numbers to generate
		// the pseudo-random data to determine what shape/color to use; by
		// using prev/next, then when we cross a morph point, the old 'next'
		// becomes the new 'prev' and everything stays cohere

		// we seed them with an early frame
		prev = next = 4003;

		// starting at frame 4045, we have objects morph, so prev/next might
		// need to be different
		if (when >= 4045)
		{
			// find previous and next morph keyframes
			prev = when;
			next = when+1;
			while (!beats[prev+delta_33][33])
				--prev;
			while (next < 4950 && !beats[next+delta_33][33])
				++next;

			// now, if we're within 7 frames of prev, we want
			// to transition from prev to next
#define MORPH_FRAMES 7
			if (raw_when < prev+MORPH_FRAMES)
			{
				// lerp from prev/next over 7 frames
				trans = stb_linear_remap(raw_when, prev, prev+MORPH_FRAMES, 0,1);
			}
			else
			{
				// otherwise, we want to already be in the 'next' state
				prev = next;
				trans = 0;
			}
		}


		// note to self:
		//   interesting region:
		//     -15*250,10*250  == -3750,2500 == -4000,2000
		//      -5*250,20*250  == -1250,5000 == -1000,5000

		// iterate over the object map and generate one object per map square
		// if necessary
		for (j=0; j < 64; j += 1)
		{
			for (i=0; i < 64; i += 1)
			{
				//int k, m;
				int animating=0;
				float x,y,z,t, scale, time;
				int bits = prandi[i*64+j], bits2;
				ShapeInstance s= {0};
				float r = 0.65;
				float g = 0.5;
				float b = 0.65;

				int slices;

				// get the color of the object map at this point
				int type = object_map[64*(63-j)+i];

				// if black, don't draw
				if (type == 0)
					continue;
				// a quick hack to allow a debug color in the file: if nearly black, don't draw
				if (type < 40)
					continue;
				// if "later" than the current threshhold, don't draw
				if (type > thresh)
					continue;

				// check if its the kind that morphs
				if (type > 192)
				{
					animating = 1;
				}

				if (animating)
				{
					// if morphable, set its color

					// determine at what frame this object gets color
					int colorize = 4540 + (bits % 88);

					// grab some random bits for the two states
					bits2 = prandi[(bits ^ next) & 4095];
					bits = prandi[(bits ^ prev) & 4095];

					// if the keyframe is after the colorize-frame, apply that part of the color
					if (prev > colorize)
					{
						r = (prand[(bits>>16)&4095]*2+1)/3;
						g = (prand[(bits>>18)&4095]*2+1)/3;
						b = (prand[(bits>>20)&4095]*2+1)/3;
					}
					if (next > colorize)
					{
						r = stb_lerp(trans, r, (prand[(bits2>>16)&4095]*2+1)/3);
						g = stb_lerp(trans, g, (prand[(bits2>>18)&4095]*2+1)/3);
						b = stb_lerp(trans, b, (prand[(bits2>>20)&4095]*2+1)/3);
					}
				}
				else
					bits2 = bits;

				// get a random float for this object (doesn't morph)
				t = prand[i*20+j*10];
				s.frame_1 = 0;
				s.frame_2 = 100;

				// compute the random type of this object
				switch ((bits >> 3) & 3)
				{
				case 0:
					s.shape_1.func = parametric_to_cube;
					s.shape_1.scale = vec3(6+t*8,6,6);
					break;
				case 1:
					s.shape_1.func = parametric_to_cylinder;
					s.shape_1.scale = vec3(7,7,6+t*8);
					break;
				case 2:
					s.shape_1.func = parametric_to_sphere;
					s.shape_1.scale = vec3(8,8,8+t*4);
					break;
				case 3:
					s.shape_1.func = parametric_to_cone;
					s.shape_1.scale = vec3(9,9,6+t*8);
					break;
				}
				s.shape_2.func = 0;

				// if morphing, also compute the type of this object post-morph
				if (animating)
				{
					switch ((bits2 >> 3) & 3)
					{
					case 0:
						s.shape_2.func = parametric_to_cube;
						s.shape_2.scale = vec3(6+t*8,6,6);
						break;
					case 1:
						s.shape_2.func = parametric_to_cylinder;
						s.shape_2.scale = vec3(7,7,6+t*8);
						break;
					case 2:
						s.shape_2.func = parametric_to_sphere;
						s.shape_2.scale = vec3(8,8,8+t*4);
						break;
					case 3:
						s.shape_2.func = parametric_to_cone;
						s.shape_2.scale = vec3(9,9,6+t*8);
						break;
					}
				}

				// determine the size of the object
				if (type < 176)
					scale = 2.5; // objects at the beginning are big because we're up off the ground so they need some size to fit in
				else if (type < 192)
					scale = 1.5; // later objects are smaller
				else
					scale = 2 + prand[j*11+i*13]; // final objects are randomly sized and bigger

				// later objects can have twist deformers as part of the object type
				if (type > 176)
				{
					// compute the twist deformer for the default state
					switch (bits & 7)
					{
					case 0:
						s.shape_1.deform = deform_twist_x;
						s.shape_1.deform_amount = 0.2/scale;
						break;
					case 1:
						s.shape_1.deform = deform_twist_y;
						s.shape_1.deform_amount = 0.2/scale;
						break;
					case 2:
						s.shape_1.deform = deform_twist_z;
						s.shape_1.deform_amount = 0.2/scale;
						break;
					}
					// if morphing, compute the twist deformer for the other state
					if (animating)
					{
						switch (bits2 & 7)
						{
						case 0:
							s.shape_2.deform = deform_twist_x;
							s.shape_2.deform_amount = 0.2/scale;
							break;
						case 1:
							s.shape_2.deform = deform_twist_y;
							s.shape_2.deform_amount = 0.2/scale;
							break;
						case 2:
							s.shape_2.deform = deform_twist_z;
							s.shape_2.deform_amount = 0.2/scale;
							break;
						}
					}
				}

#if 0
				// original morph test code written when most of the above logic didn't exist
				k = ((int) raw_when);
				m = k % 20;
				if (m < 10)
				{
					s.frame_1 = k-m;
					s.frame_2 = s.frame_1 + 10;
				}
				else
				{
					s.frame_2 = k-m+10;
					s.frame_1 = s.frame_2 + 10;
				}
#endif

				if (prev != next)
				{
					// if morphing, set up the morph frame info for computing the shape
					s.frame_1 = prev;
					s.frame_2 = prev+MORPH_FRAMES;
					time = raw_when;
				}
				else
				{
					// otherwise force it to 0
					s.frame_1 = 0;
					s.frame_2 = next;
					time = 0;
				}
				// compute the correct normal scale factor for each shape
				compute_normal_scale(&s.shape_1.norm_scale, &s.shape_1.scale);
				compute_normal_scale(&s.shape_2.norm_scale, &s.shape_2.scale);

				// compute the final scale
				s.scale = vec3(scale,scale,scale);

				// apply the handclap effect to the final scale--note that this only
				// works for 24fps output, with higher-speed output it doesn't interpolate
				// the scale factors
				if (beats[base+delta_23][23])
				{
					s.scale.x *= 1.3;
					s.scale.y *= 1.3;
					s.scale.z *= 1.3;
				}
				else if (beats[base+delta_23-1][23] || beats[base+delta_23+1][23])
				{
					s.scale.x *= 1.15;
					s.scale.y *= 1.15;
					s.scale.z *= 1.15;
					// beats[base+delta_33][33]
				}

				// original test positioning
				//glTranslatef(-4000 + i*3000/20, 2000 + j*3000/20, 6);

				// compute the position of the object, with some pseudo-random jitter so it doesn't look like a grid
				x = MAP_TO_WORLDX(i*4 + prand[i*64+j]*2);
				y = MAP_TO_WORLD(j*4 + prand[j*64+i]*2);

				// compute the number of slices to use for the parametric rendering
#ifdef FAST
				if (raw_when > 4330)
					slices = 4;  // once it gets late in the video and we have tons of objects, drop down to very low quality
				else
					slices = 8;  // normally moderate quality
				if ((x-last_lookat.x)*(x-last_lookat.x) + (y-last_lookat.y)*(y-last_lookat.y) < 300*300)
					slices = 10; // if the object is near the camera, higher quality
#else
				slices = 16;  // non-fast mode is even higher quality
#endif

#ifdef FINAL
				slices = 32;  // final rendering is very high quality
#endif

				// compute the positiong for the object
				z = mapheight(x,y) + 6*s.scale.z;

				// add leaping effect to some objects
				if (type > 176)
				{
					// this type leaps up every ~5 seconds == 120 frames
					float leap_time = prand[j*9+i*21]*100;  // time of the leaps mod 120
					float leap_force = (1+prand[j*11+i*7]) * 4;
					// only apply the leaping if the leap wouldn't have overlapped the start point--
					// this keeps objects from being in midleap when we cross 4600
					if (raw_when > 4600 + leap_time)
					{
						float leap;
						float leap_point = raw_when - (4600 + leap_time);
						leap_point = fmod(leap_point, 120);

						// compute a leap parabolic position based on time (leap_point), velocity (leap_force), acceleration due to gravity (100)
						leap = leap_force*leap_point - 100 * leap_point/24.0 * leap_point/24.0;

						// if parabola goes underground, then it's resting on the ground
						if (leap < 0) leap = 0;

						// add the leap position to the height
						z += leap;
					}
				}

				// position the object
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glTranslatef(x,y,z);

				// later objects spin randomly
				if (type > 176)
				{
					// compute spin animation time
					float t2 = (raw_when - 4430) / 100.0;

					// clamp time so there's no animation before 4430
					if (t2 < 0) t2 = 0;

					// but "no animation" has a time of 1 so that even with no animation, they're rotated
					t2 += 1;
					glRotatef((t2*prand[j*25+i*13])*180,1,0,0);
					glRotatef((t2*prand[j*13+j*25])*180,0,1,0);
				}

				setcolor(r,g,b);
				draw_shape(&s, slices, time);

				glPopMatrix();
			}
		}
		glEndList();
		cached_when = raw_when;
	}

	// now draw the cached objects
	glCallList(cached_list);


	glDisable(GL_LIGHTING);
	glColor3f(1,1,1);
}


// "objects" are an older system. the only things this is used
// for in the end are for the supports for the raised video frames

typedef struct
{
	float mat[4][4];
	GLuint list;
	GLuint tex;
} Object;

Object *objects;

// draw all the objects in the list
void draw_objects(void)
{
	int i;
	glEnable(GL_TEXTURE_2D);
	for (i=0; i < stb_arr_len(objects); ++i)
	{
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glMultMatrixf(objects[i].mat[0]);
		glBindTexture(GL_TEXTURE_2D, objects[i].tex);
		glCallList(objects[i].list);
#if 0
		// draw a rectangle to debug their position
		glBegin(GL_QUADS);
		glVertex3f(-10,0,0);
		glVertex3f(-10,0,150);
		glVertex3f( 10,0,150);
		glVertex3f( 10,0,0);
		glEnd();
#endif
		glPopMatrix();
	}
	glDisable(GL_TEXTURE_2D);
}

// add a new object to the list
void make_object(vec *p, vec *s, vec *t, GLuint list, GLuint tex)
{
	Object o;
	vec a,b,c;

	o.list = list;
	o.tex = tex;

	vec_norm(&a, s);
	vec_norm(&b, t);
	vec_cross(&c, s, t);
	vec_normeq(&c);
	o.mat[0][0] = a.x;
	o.mat[0][1] = a.y;
	o.mat[0][2] = a.z;
	o.mat[3][0] = p->x;

	o.mat[1][0] = c.x;
	o.mat[1][1] = c.y;
	o.mat[1][2] = c.z;
	o.mat[3][1] = p->y;

	o.mat[2][0] = -b.x;
	o.mat[2][1] = -b.y;
	o.mat[2][2] = -b.z;
	o.mat[3][2] = p->z;

	o.mat[0][3] = o.mat[1][3] = o.mat[2][3] = 0;
	o.mat[3][3] = 1;
	stb_arr_push(objects, o);
}

// next we have a whole custom system to draw the reflective sphere

// draw a point on the sphere
void draw_sphere_point(vec *p, float r, float x, float y, float z)
{
	glNormal3f(x,y,z);
	glVertex3f(p->x + x*r, p->y + y*r, p->z + z*r);
}

// draw the whole sphere
#define SLICES 64
#define LAYERS 48
void draw_sphere(vec *p, float r)
{
	int i,j;
	float csj[SLICES+1], snj[SLICES+1], csi[LAYERS+1], sni[LAYERS+1];
	float z0,z1;
	float r0,r1;

	// compute all the trig values first so we don't compute them in the inner loop
	for (i=0; i <= LAYERS; ++i)
	{
		sni[i] = sin(i*3.141592*2/LAYERS);
		csi[i] = cos(i*3.141592*2/LAYERS);
	}
	for (j=0; j < SLICES; ++j)
	{
		snj[j] = sin(j*3.141592*2/LAYERS);
		csj[j] = cos(j*3.141592*2/LAYERS);
	}
	snj[j] = snj[0];
	csj[j] = csj[0];

	glBegin(GL_TRIANGLE_FAN);
	draw_sphere_point(p,r, 0,0,1);
	glVertex3f(p->x,p->y,p->z + r);
	z0 = csi[1];
	r0 = sni[1];
	for (j=0; j <= SLICES; ++j)
		draw_sphere_point(p, r, r0*snj[j], r0*csj[j], z0);
	glEnd();

	for (i=2; i < LAYERS; ++i)
	{
		z1 = csi[i];
		r1 = sni[i];
		glBegin(GL_QUAD_STRIP);
		for (j=0; j <= SLICES; ++j)
		{
			draw_sphere_point(p, r, r0*snj[j], r0*csj[j], z0);
			draw_sphere_point(p, r, r1*snj[j], r1*csj[j], z1);
		}
		glEnd();
		r0 = r1;
		z0 = z1;
	}
}

// The reflections are drawn with a cubemap

//RGBA8 Cubemap texture, 24 bit depth texture, 256x256
#ifdef FINAL
#define CUBESIZE 256
#else
#ifdef FAST
#define CUBESIZE 128
#else
#define CUBESIZE 256
#endif
#endif

// the cubemap is rendered dynamically so we need an fbo etc.
GLuint cube_tex, fb, depth_rb;

void make_cubemap(void)
{
	glGenTextures(1, &cube_tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cube_tex);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	e();

	//reserve texture memory for the cubemaps
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+0, 0, GL_RGBA, CUBESIZE, CUBESIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+1, 0, GL_RGBA, CUBESIZE, CUBESIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+2, 0, GL_RGBA, CUBESIZE, CUBESIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+3, 0, GL_RGBA, CUBESIZE, CUBESIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+4, 0, GL_RGBA, CUBESIZE, CUBESIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+5, 0, GL_RGBA, CUBESIZE, CUBESIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	e();

	// generate the framebuffer object for rendering the cubemap faces
	glGenFramebuffersEXT(1, &fb);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
	e();

	// attach some cubemap face to the fbo
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cube_tex, 0);
	e();

	// generate the depth buffer for this face
	glGenRenderbuffersEXT(1, &depth_rb);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_rb);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT32, CUBESIZE, CUBESIZE);
	e();

	// attach it
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth_rb);
	e();

	// clean up
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	e();
}

// set up for rendering to a side of the cubemap
void pick_face(int f)
{
	GLenum status;
	GLenum side=0;
	e();

	switch (f)
	{
	case 0:
		side = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
		break;
	case 1:
		side = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
		break;
	case 2:
		side = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
		break;
	case 3:
		side = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
		break;
	case 4:
		side = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
		break;
	case 5:
		side = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
		break;
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fb);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, side, cube_tex, 0);
	e();

	// check to make sure the fbo is ok
	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	switch(status)
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
		break;
	default:
		stb_fatal("whoops");
	}
	e();
}

// shadowmap support
// we use cascaded shadow maps, with storage support for as many as 4 shadow maps
typedef struct
{
	GLuint tex;
	float mat[4][4];
	int w,h;
} ShadowMap;

#define NUM_SHADOW_MAPS 4
ShadowMap smap[NUM_SHADOW_MAPS];

// setup the shadow rendering texture coordinates for a particular map
// into a particular set of texture coordinates/texture unit
void setup_shadow_texcoord(ShadowMap *m, int tex)
{
	glActiveTextureARB(GL_TEXTURE0_ARB + tex);

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_S, GL_EYE_PLANE, m->mat[0]);
	glEnable(GL_TEXTURE_GEN_S);

	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_T, GL_EYE_PLANE, m->mat[1]);
	glEnable(GL_TEXTURE_GEN_T);

	glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_R, GL_EYE_PLANE, m->mat[2]);
	glEnable(GL_TEXTURE_GEN_R);

	glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv(GL_Q, GL_EYE_PLANE, m->mat[3]);
	glEnable(GL_TEXTURE_GEN_Q);

	glBindTexture(GL_TEXTURE_2D, m->tex);
	glEnable(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE_ARB, GL_LUMINANCE);

	glActiveTextureARB(GL_TEXTURE0_ARB);
}

// disable the shadow rendering stuff
void end_shadow_texcoord(int tex)
{
	glActiveTextureARB(GL_TEXTURE0_ARB  + tex);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glDisable(GL_TEXTURE_GEN_R);
	glDisable(GL_TEXTURE_GEN_Q);
	glDisable(GL_TEXTURE_2D);
	glActiveTextureARB(GL_TEXTURE0_ARB);
}

// display lists for drawing the map
GLuint mapdraw, mapdraw2;

// hardcoded to support exactly one reflective sphere
vec sphereloc = { 1350,-200,30 };

// draw everything in the scene; with_sphere determines whether to draw
// the sphere or not (we don't draw the sphere when we're *building* the
// sphere); 'computing_shadows' is true if we're building a shadow map
// (in which case we do some things differently)
void draw_world(int with_sphere)
{
	int base = (int) when;
	int i;

	glEnable(GL_DEPTH_TEST);

	glShadeModel(GL_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);

	glColor3f(1,1,1);

	// if we're not computing the shadowmap, then set up for *reading*
	// the shadow map
	if (!computing_shadows)
	{
		// we have two cascaded shadows
		setup_shadow_texcoord(&smap[0], 1);
		setup_shadow_texcoord(&smap[1], 2);
#ifdef FINAL
		// except for final renderings, when we have three
		setup_shadow_texcoord(&smap[2], 3);
#endif
		e();
		glUseProgramObjectARB(shadow_prog);
		e();
	}

	// set a color for the map that looks reasonable given the lighting
	setcolor(0.65,0.65,0.65);

	// draw the map
	glBindTexture(GL_TEXTURE_2D, test_tex);
	glEnable(GL_TEXTURE_2D);
	glCallList(mapdraw);
	glBindTexture(GL_TEXTURE_2D, test_tex2);
	glCallList(mapdraw2);
	glDisable(GL_TEXTURE_2D);

	// draw the video-frame supports
	prep_lighting();
	draw_objects();
	glDisable(GL_LIGHTING);

	// draw the "shapes"
	// don't bother drawing the little objects in the sphere, and
	// don't bother drawing until we get past the first sequence
	if (with_sphere && when > 600)
	{
		draw_shapes();
	}

	// now we're done drawing things that receive shadows
	if (!computing_shadows)
	{
		e();
		end_shadow_texcoord(1);
		end_shadow_texcoord(2);
#ifdef FINAL
		end_shadow_texcoord(3);
#endif
		glUseProgramObjectARB(0);
		e();
	}

	e();

	// if drawing the sphere, draw it
	if (with_sphere)
	{
		float mat[4][4], t;

		// enable reflection texture coordinate generation
		glTexGeni(GL_S,GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_EXT);
		glTexGeni(GL_T,GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_EXT);
		glTexGeni(GL_R,GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_EXT);
		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glEnable(GL_TEXTURE_GEN_R);

		// compute a texture matrix that will make it account for the eye
		glGetFloatv(GL_MODELVIEW_MATRIX, mat[0]);
		// inverse required, we just transpose since we don't scale
		mat[0][3] = mat[1][3] = mat[2][3] = 0;
		mat[3][0] = mat[3][1] = mat[3][2] = 0;
		t = mat[0][1];
		mat[0][1] = mat[1][0];
		mat[1][0] = t;
		t = mat[0][2];
		mat[0][2] = mat[2][0];
		mat[2][0] = t;
		t = mat[1][2];
		mat[1][2] = mat[2][1];
		mat[2][1] = t;

		glMatrixMode(GL_TEXTURE);
		glLoadMatrixf(mat[0]);

		// render it
		glBindTexture(GL_TEXTURE_CUBE_MAP_EXT, cube_tex);
		glEnable(GL_TEXTURE_CUBE_MAP_EXT);
		glColor3f(1,1,1);
		draw_sphere(&sphereloc, sphereloc.z);

		// clean up state
		glLoadIdentity();
		glDisable(GL_TEXTURE_CUBE_MAP_EXT);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
	}
	e();

	// this was used to determine the best value for delta_23
	//delta_23 = hack;

	// if we haven't computed the video frames needed this frame, compute them
	if (!stb_arr_len(frame_queue))
	{
		// draw the initial-sequence frames
		for (i=0; i <= opening_end; ++i)
		{
			int mipmap = quality ? 0 : 2;
			int n = i + opening_offset;
#ifdef FINAL
			mipmap = i < opening_end - 60 ? 1 : 0;
			if (when > 800)
				mipmap = 2;
#else
			if (!quality && when > 800 && (i & 3)) continue;
#endif
			draw_frame(n, mipmap, initial[i].o, initial[i].s, initial[i].t, NULL, 1, 707-n);
		}
		e();

		// draw the second-sequence frames
		if (when > 695 || when > 550)
		{
			for (i=720; i <= 5600; ++i)
			{
				vec dummy;
				int pos = abs(i - base);
				// determine which one to use based on pos
				if (when <= 695)
				{
					if (pos > 500) continue;
					if (pos > 200 && (i & 3)) continue;
				}
				// we do some simple LOD to drop some frames that are far in
				// the future to reduce the amount of texture memory we use
#ifdef FINAL
				if (pos > 650 && (i & 1)) continue;
				if (pos > 1200 && (i & 3)) continue;
				if (pos > 1800 && (i & 15)) continue;
				if (pos < 3000 && i >= base)
#else
				if (pos > 500 && (i & 1)) continue;
				if (pos > 1000 && (i & 3)) continue;
				if (pos < 1500 && i >= base)
#endif
				{
					vec b_s = { 54,34,0 };
					vec b_o = { 964 - 2.0*(i-switch_offset-750)/4-54, 323 + (i-switch_offset-750)*3.5/4-34, 36 };
					vec b_t = { 0,0,-36 };

					//int x;
					float rgb[3]= {1,1,1},a=1;
					vec o,s,t;
					int mipmap;
					int jstep = (pos < 10 ? 1 : pos < 30 ? 2 : pos < 200 ? 4 : 8);
					int j;

					// compute which mipmap to use for this frame

#ifdef FINAL
					if (pos < 80) mipmap = 0;
					else if (pos < 160) mipmap = 1;
					else mipmap = 2;
#else
					if (pos < 20) mipmap = 0;
					else if (pos < 100) mipmap = 1;
					else mipmap = 2;
					if (!quality) mipmap = 2;
#endif

					// if when <= 695, then we're only seeing reflections in the sphere,
					// so use the lowest mipmap level
					if (when <= 695) mipmap = 2;

#if 0
					// we used to make the frames flash red/purple on the handclaps
					if (beats[base+delta_23][23] || beats[base+delta_23-1][23])
						rgb[1] = 0;
#endif

					//if (beats[i+delta_33][33] == 0) continue;
					eval_frame(i, &o, &s, &t, &dummy);

#if 0
					// we used to make the frames pulse larger on the measure boundaries
					if (when >= 4045)
					{
						if (beats[base+delta_33][33])
						{
							vec_addeq_scale(&o, &s, 0.5);
							vec_addeq_scale(&o, &t, 1.0);
							vec_scaleeq(&s, 1.125);
							vec_scaleeq(&t, 1.125);
							vec_addeq_scale(&o, &s, -0.5);
							vec_addeq_scale(&o, &t, -1.0);
						}
						else if (beats[base-1+delta_33][33] || beats[base+1+delta_33][33])
						{
							vec_addeq_scale(&o, &s, 0.5);
							vec_addeq_scale(&o, &t, 1.0);
							vec_scaleeq(&s, 1.0625);
							vec_scaleeq(&t, 1.0625);
							vec_addeq_scale(&o, &s, -0.5);
							vec_addeq_scale(&o, &t, -1.0);
						}
					}
#endif

					draw_frame(i,mipmap, o, s, t, rgb,a, i);

					// used to try drawing each frame multiple times to make it look more solid,
					// but this looked worse because we were seeing fewer pixels, and because the
					// edge pixels have more greenscreen bleed in them so we saw lots of green
					// edges (although this was before I switched to premultiplied alpha, which
					// probably made it worse)
					jstep = 8;
					for (j=jstep; j < 8; j += jstep)
					{
						eval_frame(i + j / 8.0, &o, &s, &t, &dummy);
						draw_frame(i,mipmap, o,s,t, rgb,a, i + j/8.0);
					}
				}
			}
		}
	}
	// draw the queued frames
	draw_queue();
	e();
}

// record the matrix needed for shadowmap projection, based on the matrix
// the shadowmap is rendered with
void capture_mat(ShadowMap *map)
{
	float lightproj[4][4], lightview[4][4];
	static float project_bias[4][4] =
	{
		0.5,0,0,0,
		0,0.5,0,0,
		0,0,0.5,0,
		0.5,0.5,0.5,1,
	};

	glGetFloatv(GL_PROJECTION_MATRIX, lightproj[0]);
	glGetFloatv(GL_MODELVIEW_MATRIX, lightview[0]);

	memcpy(map->mat, project_bias, sizeof(map->mat));
	float44_mul(map->mat, map->mat, lightproj);
	float44_mul(map->mat, map->mat, lightview);
	float44_transpose(map->mat);
}

int show_light_view=0;
int bias_1=4, bias_2=16;
GLuint shadow_fbo;

// start computing a shadowmap for a light
void start_light(ShadowMap *map)
{
	computing_shadows = 1;

	if (shadow_fbo)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, map->tex, 0);
	}
}

// finish computing a shadowmap for a light
void end_light(ShadowMap *map)
{
	computing_shadows = 0;

	if (shadow_fbo)
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
	else
	{
		glBindTexture(GL_TEXTURE_2D, map->tex);
		glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,map->w,map->h);
	}
}

// compute a light
void do_light(ShadowMap *map)
{
	// set the shadowmap framebuffer
	start_light(map);

	// set up shadowmap rendering
	glDepthMask(GL_TRUE);
	glViewport(0,0,map->w,map->h);
	glClear(GL_DEPTH_BUFFER_BIT);

	// record the matrix
	capture_mat(map);

	// set up the rendering state... should mask out colors?
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_LIGHTING);

	// set the polygon offset to something that looks reasonable
	// the bias 0,0 stuff here was an experiment to try to fix the buggy shadow in the reflection
	if (raw_when < 650)
		glPolygonOffset(0,0);
	else
		glPolygonOffset(bias_1, bias_2);
	glEnable(GL_POLYGON_OFFSET_FILL);

	// draw the scene into the shadowmap
	draw_world(1);

	// and done
	end_light(map);
	glDisable(GL_POLYGON_OFFSET_FILL);
}

// draw the world for a normal camera
void draw_scene(void)
{
	draw_world(1);
}

int mouse_x, mouse_y;
int left_down;

// camera positioning logic starts here
float real_time;
int rstart = 347-5 - delay_start;
int rend   = 490-2 - delay_start;
int t1 = 538-2 - delay_start;
int t2 = 633-2 - delay_start;
int t3 = 730-2 - delay_start;

void compute_lookat(float mat[4][4], vec *cam, vec *target, vec *up)
{
	vec x,y,z;
	vec_sub(&z, target, cam);
	if (left_down)
	{
		float as = sin(-mouse_x * 3.141592 * 4 / 1024);
		float ac = cos(-mouse_x * 3.141592 * 4 / 1024);
		float bs = sin(mouse_y * 3.141592 * 4 / 1024);
		float bc = cos(mouse_y * 3.141592 * 4 / 1024);
		z.z = bs;
		z.x = bc*ac;
		z.y = bc*as;
	}
	vec_normeq(&z);
	vec_cross(&x, &z, up);
	vec_normeq(&x);
	vec_cross(&y, &x, &z);

	mat[0][0] = x.x;
	mat[1][0] = x.y;
	mat[2][0] = x.z;
	mat[3][0] = -vec_dot(&x, cam);

	mat[0][1] = z.x;
	mat[1][1] = z.y;
	mat[2][1] = z.z;
	mat[3][1] = -vec_dot(&z, cam);

	mat[0][2] = y.x;
	mat[1][2] = y.y;
	mat[2][2] = y.z;
	mat[3][2] = -vec_dot(&y, cam);

	mat[0][3] = 0;
	mat[1][3] = 0;
	mat[2][3] = 0;
	mat[3][3] = 1;
}

// the frame number on which we start fading out by pulling the fog in
#define FOG_FADE 4840

// during final rendering, this controls which output frame we're writing
// and provides storage for writing it
int frame_number = 0;
unsigned char screenbuffer[4096*4096*4];

// this was used for various hacks to try to fix the broken reflection shadow
int first_time=50;

// draw the entire frame
void draw(float dt)
{
	float mymat[4][4] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
	vec pos, target, up;

	float color[16][3] = { { 1,1,1 } };

	// do nothing if we haven't initialized yet
	if (!initialized) return;
	e();
	// compute the position of all frames--this allows us to
	// make the frame path vary dynamically, which I didn't end up using
	// (I was picturing having the end sequence have the video frames
	// going into the distance start animating)
	walk_frames(when);
	update_cache();
	e();

	// render light shadowmap #0
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(4, 1.0, 6000.0, 16000.0); // 4

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(last_lookat.x+light_dir.x, last_lookat.y+light_dir.y, last_lookat.z+light_dir.z, last_lookat.x, last_lookat.y, last_lookat.z, 0,0,1);

	do_light(&smap[0]);
	e();

	// render light shadowmap #1
#ifndef FAST
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(35.0f, 1.0, 6000.0, 16000.0); // 35

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(last_lookat.x+light_dir.x, last_lookat.y+light_dir.y, last_lookat.z+light_dir.z, last_lookat.x, last_lookat.y, last_lookat.z, 0,0,1);

	do_light(&smap[1]);
#endif

	// render light shadowmap #2
#ifdef FINAL
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(12.0, 1.0, 6000.0, 16000.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(last_lookat.x+light_dir.x, last_lookat.y+light_dir.y, last_lookat.z+light_dir.z, last_lookat.x, last_lookat.y, last_lookat.z, 0,0,1);

	do_light(&smap[2]);
#endif

	// setup the fog
	glFogfv(GL_FOG_COLOR, fcolor);
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_EXP);
	glFogf(GL_FOG_START, 120);
	glFogf(GL_FOG_END  , 500);
	glFogf(GL_FOG_DENSITY, 0.0005);
	// animate the fog in at the end of the video for a fadeout
	if (raw_when > FOG_FADE)
	{
		float fade = stb_linear_remap(raw_when, FOG_FADE, FOG_FADE+24*8, 0, 1);
		if (fade > 1) fade = 1;
		glFogf(GL_FOG_START, 120-119*fade);
		glFogf(GL_FOG_END, 500-400*fade);
		glFogf(GL_FOG_DENSITY, 0.0005 + 0.2*fade*fade*fade*fade);
	}

	// during the time when the sphere is visible, update the sphere reflection
	if (when >= 560 && when <= 694)
	{
		int i,j;
		e();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(90, 1, 0.5, 15000);
		glBindTexture(GL_TEXTURE_CUBE_MAP_EXT, cube_tex);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_CUBE_MAP_EXT, 0);

		e();

		// iterate through the reflection cubemap faces
		for (j=0; j < 6; ++j)
		{
			static int order[6] = { 5,0,1,2,3,4 };//

			// experimenting to workaround bug where shadows don't work for face 5 correctly
			i = order[j];

#if 0
			if (first_time)
			{
				if (i != 5)
					continue;
			}
			else
			{
				if (i == 5)
					continue;
			}

			if (i != 5)
				continue;
#endif

			// choose the face to render into
			pick_face(i);

			// setup the camera to point in the direction of that face
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			stbgl_initCamera_zup_facing_y();
			switch(i)
			{
			case 0:
				glRotatef(-90,0,1,0);
				glRotatef(90, 0,0,1);
				break;
			case 1:
				glRotatef(90,0,1,0);
				glRotatef(-90, 0,0,1);
				break;
			case 2:
				break;
			case 3:
				glRotatef(180, 0,1,0);
				glRotatef(180,0,0,1);
				break;
			case 4:
				glRotatef(-90, 1,0,0);
				break;
			case 5:
				glRotatef(90, 1,0,0);
				break;
			}

			// move the camera to the sphere location
			glTranslatef(-sphereloc.x, -sphereloc.y, -sphereloc.z);
			e();

			// render the scene from the POV of the sphere
			glClearColor(fcolor[0], fcolor[1], fcolor[2], fcolor[3]);
			//glClearColor(i / 6.0, 1- i/6.0, 0.5, 1);
			glViewport(0,0,CUBESIZE,CUBESIZE);
			e();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			draw_world(0);
			e();
		}
		if (first_time)
			--first_time;

		// finish rendering the cubemap
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
		glBindTexture(GL_TEXTURE_CUBE_MAP_EXT, cube_tex);
		// generate mipmaps for the cubemap faces
		glGenerateMipmapEXT(GL_TEXTURE_CUBE_MAP_EXT);
		// set the blend modes again just in case
		glBindTexture(GL_TEXTURE_CUBE_MAP_EXT, cube_tex);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// and done
		glBindTexture(GL_TEXTURE_CUBE_MAP_EXT, 0);
		e();
	}

	// now we're ready to draw the actual scene. setup the framebuffer
	glClearColor(fcolor[0], fcolor[1], fcolor[2], fcolor[3]);
	glViewport(0,0,sx,sy);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// set the camer aprojection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	stbgl_Perspective(1.0, 90, 70, 0.5, 15000);

	// now compute where the camera should be
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	stbgl_initCamera_zup_facing_y();

	// during the opening sequence?
	if (when < 24*60)
	{
		vec s,t,n;
		float dist = 32;
		// we track right along with the video so it doesn't look 3D
		s = initial[opening_end].s;
		t = initial[opening_end].t;
		n = initial[opening_end].n;

		vec_add_scale(&target, &initial[opening_end].o, &s, 0.5);
		vec_addeq_scale(&target, &t, 0.5);
		vec_add_scale(&pos, &target, &n, dist);
		vec_norm(&up,&t);
		vec_scaleeq(&up, -1);

		// at a certain point in time the camera moves away
		if (when > t2-30)
		{
			vec goal = vec3(1160,-150,30);
			float t;
			// camera descends from 60..90
			t = stb_linear_remap(raw_when, t2+60, t2+90, 0, 1);
			t = stb_clamp(t,0,1);
			t = 3*t*t - 2*t*t*t;
			goal.z += 20 * t;

#if 0
			if (when > 720)
			{
				goal.x -= (when - 720) * 4;
			}
#endif

			// warp between original camera and new camera from -30 to 30
			t = stb_linear_remap(raw_when, t2-30, t2+30, 0, 1);
			t = stb_clamp(t,0,1);
			// use smooth cubic
			t = 3*t*t - 2*t*t*t;
			// square it so motion goes from slow to fast
			t *= t;
			// move camera from default track to new position.
			// also adjust "up" vector to tilt from the default
			// to up being normal up
			vec_lerp(&pos, &pos, &goal, t);
			{
				vec z = { 0,0,1 };
				vec_lerp(&up, &up, &z, t);
			}
		}
	}


	// if we're onto the main sequence, we compute the cameras differently
	if (when > 730)
	{
		// compute a parametr that goes from 0 to 1 as we transition between the
		// opening sequence camera and the main sequence camera
		float w = stb_linear_remap(when, 730,795, 0,1);
		int n;
		vec cpos;
		vec o,s,t;
		vec ngoal,dir;

		// lerp between keyframes
		n = get_keyframe((int) when);
		if (n+1 < stb_arr_len(keyframes))
		{
			// if we're within the keyframes, we want to evaluate the cubic spline

#if 0
			// (notused)
			vec cam1,cam2,target1,target2, step1, step2;
			float len = (keyframes[n+1].frame - keyframes[n].frame);
			float p = (float) (when - keyframes[n].frame) / len, a;

			compute_keyframe_lookat(n, &cam1, &target1, &step1);
			compute_keyframe_lookat(n+1, &cam2, &target2, &step2;

			                        // spline from #1 to #2, but use 'step' as the endpoint velocities
			                        // so this is just a cubic hermite spline

			                        a = 3*p*p - 2*p*p*p;

			                        vec_lerp(&cpos, &cam1, &cam2, a);
			                        vec_lerp(&ngoal, &target1, &target2, a);

			                        // we have to shift velocity to be relative to p
			                        step1 = tan1; //vec_scaleeq(&step1, len);
			                        step2 = tan2; //vec_scaleeq(&step2, len);

			                        // velocity #1:
			                        vec_addeq_scale(&cpos, &step1, p*(1-p)*(1-p));
			                        vec_addeq_scale(&ngoal, &step1, p*(1-p)*(1-p));

			                        // velocity #2:
			                        vec_addeq_scale(&cpos, &step2, p*p*(p-1));
			                        vec_addeq_scale(&ngoal, &step2, p*p*(p-1));

#else
			// here is the actual implementation
			vec cam1,cam2,target1,target2;
			float len = (keyframes[n+1].frame - keyframes[n].frame);
			float p = (float) (when - keyframes[n].frame), q;
			vec cam1_tan, cam2_tan, target1_tan, target2_tan;

			compute_keyframe_lookat(n, &cam1, &target1, &cam1_tan, &target1_tan);
			compute_keyframe_lookat(n+1, &cam2, &target2, &cam2_tan, &target2_tan);

			// spline from #1 to #2, but use 'step' as the endpoint velocities
			// so this is just a cubic hermite spline

			// write out the wikipedia formula in terms of y1,y2,z1,z2
			// let p = (x-xi), q = (x(i+1)-x), len = hi

			// z2*p^3/6/len + z1*q^3/6/len + y2/len*p - len/6*z2*p + y1/len*q - len/6*z1*q

			// write with simple factors:

			// y1*(q/len) + y2*(p/len) + z1*(q^3/6/len - q*len/6) + z2*(p^3/6/len - p*len/6)
			// this is nice and symmetric, but wow does it not match the traditional formula.
			// i guess all the smarts are in the 'tangents' which aren't actually tangents...
			// from another paper i found it looks like they're the 2nd derivatives

			q = len - p;

			// first two lines are just a simple lerp:
			vec_scale(&cpos, &cam1, q/len);
			vec_addeq_scale(&cpos, &cam2, p/len);
			// the second two lines add in a cubic curve of form q*(A+B*q*q) and for b.
			// if len were 1, then the form would be q*q*q-q; this is 0 at both q=0 and q=1.
			// if we think about the first derivative relative to t:
			//      p=x-x0; f(x) = p*p*p/len - p*len, f'(x) = 3*p^2/len - len
			//      q=x1-x0; g(x) = q*q*q/len - q*len, g'(x) = -3*p^2/len + len
			vec_addeq_scale(&cpos, &cam1_tan, (q*q*q/len - q*len)/6);
			vec_addeq_scale(&cpos, &cam2_tan, (p*p*p/len - p*len)/6);

			vec_scale(&ngoal, &target1, q/len);
			vec_addeq_scale(&ngoal, &target2, p/len);
			vec_addeq_scale(&ngoal, &target1_tan, (q*q*q/len - q*len)/6);
			vec_addeq_scale(&ngoal, &target2_tan, (p*p*p/len - p*len)/6);
#endif

		}
		                        else if (n < 0 || editing_frame == (int) when)
		{
			// if we have no keyframes, use this logic
			vec step;
			eval_frame(when, &o, &s, &t, &step);
			vec_add_scale(&ngoal, &o, &s, 0.5);
			vec_addeq_scale(&ngoal, &t, 0.5);

			vec_cross(&dir, &s, &t);
			vec_normeq(&dir);

			vec_addeq_scale(&ngoal, &dir, -camloc[1]);
			ngoal.z += camloc[2];

			vec_rotate_vec(&dir, &s, camang[0]*3.141592/180, &dir);
			vec_rotate_vec(&dir, &t, camang[2]*3.141592/180, &dir);

			vec_add_scale(&cpos, &ngoal, &dir, -camloc[3]);
		}
		else
		{
			// otherwise use this fallback
			vec step;
			compute_keyframe_lookat_raw(n, &cpos, &ngoal, &step);
			vec_addeq_scale(&cpos, &step, when - keyframes[n].frame);
			vec_addeq_scale(&ngoal, &step, when - keyframes[n].frame);
		}

		// now, use w to lerp between the original sequence
		// camera and the main-sequence camera
		if (w >= 1)
		{
			pos = cpos;
			target = ngoal;
		}
		else
		{
			w = stb_clamp(w,0,1);
			w = 3*w*w - 2*w*w*w;
			//w = 3*w*w - 2*w*w*w;
			vec_lerp(&pos, &pos, &cpos, w);
			vec_lerp(&target, &target, &ngoal, w);
		}

		up.x = 0;
		up.y = 0;
		up.z = 1;
	}

	// compute this as a "look-at" operation so that we
	// can move the camera and look at point smoothly
	campos = pos;
	compute_lookat(mymat, &pos, &target, &up);

	// save away the last lookat for use in other computations, like positioning next frames shadowmap
	last_lookat = target;

	// position the camera and render it
	glMatrixMode(GL_MODELVIEW);
	glMultMatrixf(mymat[0]);
	e();
	draw_scene();
	e();

	// now we're all done for this frame, so update the cache of video frames
	// so it knows which ones it can free
	flush_queue();
	e();

	// if doing a final rendering, write it to disk
	// this is disabled so I won't accidentally overwrite the existing final render
#ifdef FINAL
	glReadPixels(0,0,sx,sy, GL_RGBA, GL_UNSIGNED_BYTE, screenbuffer);
	//stbi_write_png(stb_sprintf("f:/wtf/3d/wtf3d_%04d.png", frame_number+1), sx,sy, 4, screenbuffer, 4*sx);
	//stbi_write_bmp(stb_sprintf("c:/x/wtf/wtf3d_%04d.bmp", frame_number+1), sx,-sy, 4, screenbuffer);
	++frame_number;
#endif

	// display the frame
	stbwingraph_SwapBuffers(NULL);
	e();
}

// process MIDI input
void do_midi(uint8 *buffer, int len)
{
	int i;
	for (i=0; i < len; i += 3)
	{
		OutputDebugString(stb_sprintf("%d %d %d\n", buffer[i], buffer[i+1], buffer[i+2]));
		if (buffer[i] == 176)
		{
			int control = buffer[i+1];
			int value = buffer[i+2];
#if 0
			if (val == 127)
			{
				FILE *f = fopen("beats.txt", "a");
				fprintf(f, "%d %d\n", (int) when, chan);
				fclose(f);
			}
#endif
			state[control] = value;
			changed[control] = 1;
			// parse Korg nanoKontrol
			switch (control)
			{
			case 45: // start
				if (value == 127) pause = 0;
				break;
			case 46: // stop
				if (value == 127) pause = 1;
				break;
			case 47: // rewind
				editing_frame = -1;
				editing_keyframe = -1;
				if (advancing)
				{
					if (value == 127) advancing = 250;
					if (value == 0) advancing = pause ? 10 : 50;
				}
				else
				{
					if (value == 127) rewinding = pause ? 10 : 50;
					if (value == 0) rewinding = 0;
				}
				break;
			case 48: // ffwd
				editing_frame = -1;
				editing_keyframe = -1;
				if (rewinding)
				{
					if (value == 127) rewinding = 250;
					if (value == 0) rewinding = pause ? 10 : 50;
				}
				else
				{
					if (value == 127) advancing = pause ? 10 : 50;
					if (value == 0) advancing = 0;
				}
				break;
			}
		}
	}
	if (!pause)
	{
		editing_frame = -1;
		editing_keyframe = -1;
	}
}

// system for building objects (as noted previously, this is
// only used for the T-objects that support the raised video
// frames)

// create an object by rendering it into a display list
float otsc;
GLuint obj_begin(float tex_scale)
{
	GLuint c = glGenLists(1);
	glNewList(c, GL_COMPILE);
	otsc = tex_scale;
	return c;
}

void obj_end(void)
{
	glEndList();
}

// each polygon is texture mapped by projecting from some axis,
// so record that axis
int proj_axis;
void poly_begin(int axis)
{
	proj_axis = axis;
	glBegin(GL_POLYGON);
}

void poly_end(void)
{
	glEnd();
}

// draw a vertex, and compute the texture projection for it
void vertex(float x, float y, float z)
{
	float s,t;
	switch (proj_axis)
	{
	case 0:
		s = y;
		t = z;
		break;
	case 1:
		s = z;
		t = x;
		break;
	case 2:
		s = x;
		t = y;
		break;
	}
	glTexCoord2f(s*otsc, t*otsc);
	glVertex3f(x,y,z);
}

// build an axis-aligned box
void build_box(float x0, float y0, float z0, float x1, float y1, float z1)
{
	float t;
	if (x0 > x1)
	{
		t=x0;
		x0=x1;
		x1=t;
	}
	if (y0 > y1)
	{
		t=y0;
		y0=y1;
		y1=t;
	}
	if (z0 > z1)
	{
		t=z0;
		z0=z1;
		z1=t;
	}

	glNormal3f(-1,0,0);
	poly_begin(0);
	vertex(x0,y0,z0);
	vertex(x0,y1,z0);
	vertex(x0,y1,z1);
	vertex(x0,y0,z1);
	poly_end();

	glNormal3f(1,0,0);
	poly_begin(0);
	vertex(x1,y1,z0);
	vertex(x1,y0,z0);
	vertex(x1,y0,z1);
	vertex(x1,y1,z1);
	poly_end();

	glNormal3f(0,-1,0);
	poly_begin(1);
	vertex(x1,y0,z0);
	vertex(x0,y0,z0);
	vertex(x0,y0,z1);
	vertex(x1,y0,z1);
	poly_end();

	glNormal3f(0,1,0);
	poly_begin(1);
	vertex(x0,y1,z0);
	vertex(x1,y1,z0);
	vertex(x1,y1,z1);
	vertex(x0,y1,z1);
	poly_end();

	glNormal3f(0,0,-1);
	poly_begin(2);
	vertex(x0,y0,z0);
	vertex(x1,y0,z0);
	vertex(x1,y1,z0);
	vertex(x0,y1,z0);
	poly_end();

	glNormal3f(0,0,1);
	poly_begin(2);
	vertex(x1,y0,z1);
	vertex(x0,y0,z1);
	vertex(x0,y1,z1);
	vertex(x1,y1,z1);
	poly_end();
}

// build a box which is rotated 45-degrees along the z axis
void build_dbox(float x0, float y0, float z0, float x1, float y1, float z1)
{
	float t;
	float x,y,z;
	float r2 = (float) sqrt(2.0);
	if (x0 > x1)
	{
		t=x0;
		x0=x1;
		x1=t;
	}
	if (y0 > y1)
	{
		t=y0;
		y0=y1;
		y1=t;
	}
	if (z0 > z1)
	{
		t=z0;
		z0=z1;
		z1=t;
	}

	x = (x0 + x1)/2;
	y = (y0 + y1)/2;
	z = (z0 + z1)/2;

	glNormal3f(-r2,-r2,0);
	poly_begin(0);
	vertex(x ,y0,z0);
	vertex(x0,y ,z0);
	vertex(x0,y ,z1);
	vertex(x ,y0,z1);
	poly_end();

	glNormal3f(r2,r2,0);
	poly_begin(0);
	vertex(x ,y1,z0);
	vertex(x1,y ,z0);
	vertex(x1,y ,z1);
	vertex(x ,y1,z1);
	poly_end();

	glNormal3f(r2,-r2,0);
	poly_begin(0);
	vertex(x1,y ,z0);
	vertex(x ,y0,z0);
	vertex(x ,y0,z1);
	vertex(x1,y ,z1);
	poly_end();

	glNormal3f(-r2,r2,0);
	poly_begin(0);
	vertex(x0,y ,z0);
	vertex(x ,y1,z0);
	vertex(x ,y1,z1);
	vertex(x0,y ,z1);
	poly_end();

	glNormal3f(0,0,-1);
	poly_begin(2);
	vertex(x ,y0,z0);
	vertex(x1,y ,z0);
	vertex(x ,y1,z0);
	vertex(x0,y ,z0);
	poly_end();

	glNormal3f(0,0,1);
	poly_begin(2);
	vertex(x ,y0,z1);
	vertex(x0,y ,z1);
	vertex(x ,y1,z1);
	vertex(x1,y ,z1);
	poly_end();
}

// compute the blurred version of a grid of data
// the filter is:
//           1  2  1
//           2 20  2
//           1  2  1
// which was chosen for giving the most reasonable
// results for the ground
void blur(float dest[256][256], float src[256][256], int y, int x)
{
	int i,j;
	for (i=0; i < 255; ++i)
	{
		dest[i][0] = src[i][0];
		dest[i][255] = src[i][255];
		dest[0][i] = src[0][i];
		dest[255][i] = src[255][i];
	}
	for (j=1; j < 254; ++j)
	{
		for (i=1; i < 254; ++i)
		{
			dest[j][i] = (src[j][i]*20 + src[j-1][i]*2 + src[j+1][i]*2 + src[j][i-1]*2 + src[j][i+1]*2 + src[j-1][i-1] + src[j+1][i-1] + src[j-1][i+1] + src[j+1][i+1])/32.0;
		}
	}
}

// load the ground height map and convert it into polygon data
#define TCSCALE  0.5
void loadMap(void)
{
	int a,b, pass;
	int x,y,i,j;
	// load the PNG file containing the map
	uint8 *rawmap = stbi_load("map.png", &x, &y, NULL, 1);
	// flip it vertically to make it easier to use
	for (j=0; j < y; ++j)
		for (i=0; i < x; ++i)
			map2[j][i] = rawmap[(y-1-j)*x+i]*3;
	free(rawmap);

	// blur it to smooth out the 8-bit-ness
	blur(map, map2, x,y);

	// we need to build two display lists; one for the red-white
	// checkerboard, and one for the blue-grey checkerboard.
	// so we traverse the map twice
	for (pass=0; pass < 2; ++pass)
	{

		if (pass == 0)
		{
			if (!mapdraw)
				mapdraw = glGenLists(1);
			glNewList(mapdraw, GL_COMPILE);
		}
		else
		{
			if (!mapdraw2)
				mapdraw2 = glGenLists(1);
			glNewList(mapdraw2, GL_COMPILE);
		}

		// turn it into blocks so subsections can frustum culled automatically
		for (b=0; b < y; b += 16)
		{
			for (a=0; a < x; a += 16)
			{
				// approximate frustum of camera after it first moves away,
				// and use that to decide where to place red-white:

				int in_main=0;
				if (a < 150 + b/6) in_main = 1;
				//if (a < 144+32) continue;
				//if (b > 120) continue;
				if (b > 0 + a*2/3) in_main = 1;
				if (b > 110 + a/8) in_main = 1;
				if (b > 120) in_main = 1;

				// skip buiding polygons if it's the wrong pass
				if (pass == in_main)
					continue;

				// check if a grid of 17x17 samples is all flat
				for (j=b; j <= b+16; ++j)
				{
					if (j >= y) continue;
					for (i=a; i <= a+16; ++i)
					{
						if (i >= x) continue;
						if (map[j][i] != map[b][a])
							break;
					}
					if (i <= a+16) break;
				}

				// if a block is all flat, optimize it to a single polygon
				// this lets us use only a quarter of our 256x256 map without paying for
				// rendering all 256x256 quads
				if (j == b+16)
				{
					int q=0;
					float height = map[b][a];
					i = a+16;
					glBegin(GL_TRIANGLE_FAN);
#if 1
					// draw a "polygon" with 16 vertices on each side to avoid t-junctions
					for (q=0; q < 64; ++q)
					{
						float c,d;
						if (q < 16)
							c = a+q, d=b;
						else if (q < 32)
							c = i, d=b+(q-16);
						else if (q < 48)
							c = i-(q-32), d=j;
						else
							c = a, d = j - (q-48);
						glTexCoord2f(c*TCSCALE,d*TCSCALE);
						glVertex3f(MAP_TO_WORLDX(c), MAP_TO_WORLD(d), height);
					}
#else
					glTexCoord2f(a*TCSCALE,b*TCSCALE);
					glVertex3f(MAP_TO_WORLDX(a), MAP_TO_WORLD(b), height);
					glTexCoord2f(i*TCSCALE,b*TCSCALE);
					glVertex3f(MAP_TO_WORLDX(i), MAP_TO_WORLD(b), height);
					glTexCoord2f(i*TCSCALE,j*TCSCALE);
					glVertex3f(MAP_TO_WORLDX(i), MAP_TO_WORLD(j), height);
					glTexCoord2f(a*TCSCALE,j*TCSCALE);
					glVertex3f(MAP_TO_WORLDX(a), MAP_TO_WORLD(j), height);
#endif
					glEnd();
				}
				else
				{
					// block is not flat, so draw it normally, as 16 triangle strips
					for (j=b; j < b+16; ++j)
					{
						if (j+1 >= y) continue;
						glBegin(GL_TRIANGLE_STRIP);
						for (i=a; i <= a+16; ++i)
						{
							if (i >= x) continue;
							glTexCoord2f(i*TCSCALE,j*TCSCALE);
							glVertex3f(MAP_TO_WORLDX(i), MAP_TO_WORLD(j), map[j][i]);
							glTexCoord2f(i*TCSCALE,(j+1)*TCSCALE);
							glVertex3f(MAP_TO_WORLDX(i), MAP_TO_WORLD(j+1), map[j+1][i]);
						}
						glEnd();
					}
				}
			}
		}
		glEndList();
	}

	// load the locations of the shapes
	object_map = stbi_load("objects.png", &x, &y, NULL, 1);
}

// create a shadow texture
void make_shadow_map(int slot, int w, int h, char *prop)
{
	smap[slot].tex = stbgl_TexImage2D(0, w,h, NULL, prop);
	smap[slot].w = w;
	smap[slot].h = h;
}

// cascaded shadowmap shader
char *shader_source =
    "uniform sampler2D main_tex;"
    "uniform sampler2DShadow shadow1;"
    "uniform sampler2DShadow shadow2;"
    "uniform sampler2DShadow shadow3;"
    "uniform vec4 mat_color;"
// compute the "weight" for a cascade shadow map, which is whether it's valid
// or not in this region (0 for invalid, 1 for valid, intermediate values as
// we transition)
    "float weight(vec4 tc)"
    "{"
    "   vec2 proj = vec2(tc.x / tc.w, tc.y / tc.w);" // project the coordinate manually
    "   proj = proj*2.0 - 1.0;"                      // remap to -1 to 1
    "   proj = abs(proj);"                           // remap to 1..0..1
// at this point, proj is 0..1, with 1 being at the edge of usable
    "   proj = (1.0-proj)*8.0;"  // remap to 0..8, with 0 being at the edge of usable
    "   proj = clamp(proj,0.0,1.0);" // clamp to 0..1
    "   return min(proj.x,proj.y);" // min, or multiply?
    "}"
    ""
    "void main()"
    "{"
// compute the surface texture color
    "   vec4 tex = texture2D(main_tex, gl_TexCoord[0].st);"
#ifndef FAST
// compute the shadow and weighting for the outermost shadow cascade
    "   float s2 = shadow2DProj(shadow2, gl_TexCoord[2]).r;"
    "   float w2 = weight(gl_TexCoord[2]);"
// now mix that with 1.0 (no shadow) in places where the shadow doesn't reach
    "   s2 = mix(1.0, s2, w2);"
#else
// in fast mode, the outer shadow just is 1.0 (no shadow)
    "   float s2 = 1.0;"
#endif
#ifdef FINAL
// in final mode, compute the middlemost shadow
    "   float s3 = shadow2DProj(shadow3, gl_TexCoord[3]).r;"
    "   float w3 = weight(gl_TexCoord[3]);"
// and override the above shadow computation with it according to the weight
    "   s2 = mix(s2, s3, w3);"
#endif
// compute the shadow and weighting for the innermost shadow cascade
    "   float s1 = shadow2DProj(shadow1, gl_TexCoord[1]).r;"
    "   float w1 = weight(gl_TexCoord[1]);"
// now weight between the middle/outer computed previously and the shadow just computed
    "   s1 = mix(s2, s1, w1);"
// the light color is computed as a vertex light and passed in from gl_color, then scaled by the shadow factor
    "   vec4 light_color = s1 * clamp(gl_Color,0.0,1.0);"
// ambient color is hardcoded here
    "   vec4 ambient_color = vec4(0.4,0.4,0.45,0);"
// gamma corrected addition of the light values
    "   light_color = sqrt(light_color * light_color + ambient_color * ambient_color);"
// final lit color is the product of the light color, the texture, and a material color
    "   vec4 color = light_color * tex * mat_color;"
// then apply the fog using the opengl fog rules
    "   float fog_factor = clamp(exp2(-gl_Fog.density * gl_FragCoord.z / gl_FragCoord.w * 1.442695), 0.0, 1.0);"
    "   color.rgb = mix(gl_Fog.color.rgb, color.rgb, fog_factor);"
    "   gl_FragColor = color;"
    "}";

// build a "reference" which is a position of a video frame
void make_ref(Reference *r, vec *o, vec *s, vec *t, vec *n)
{
	r->o = *o;
	r->s = *s;
	r->t = *t;
	r->n = *n;
}

// compute a surface normal from tangent vectors
void make_norm(vec *n, vec *s, vec *t)
{
	vec_cross(n, s, t);
	vec_normeq(n);
}

// initialize everything
static void init(void)
{
	GLhandleARB shader;
	int i, len, result;
	// load the file with the table of beats
	char **text = stb_stringfile("beats.txt", &len);
	// unpack it into an easy-to-use data strcture
	for (i=0; i < len; ++i)
	{
		int when, beat;
		if (sscanf(text[i], "%d %d", &when, &beat) == 2)
		{
			beats[when][beat] = 1;
		}
	}

	// initialize the (unused) platonic solids
	init_platonics();

	// generate random numbers for use in our pseudo-random stuff
	for (i=0; i < 4096; ++i)
	{
		prand[i] = stb_frand();
		prandi[i] = stb_rand();
	}

	e();
	// initialize the grid stuff used for packing video frames into textures
	compute_best_grids();

	// a now-irrelevant initial camera positiong
	camloc[0] = 0;
	camloc[1] = -15;
	camloc[2] = 12*4/2;
	camloc[3] = 20;
	camang[0] = -20;
	camang[1] = 0;
	camang[2] = 0;

	// build memory for storing the video frames
	for (i=0; i < 3; ++i)
	{
		tiletex[i] = malloc(sizeof(*tiletex[i]) * 5129);
		memset(tiletex[i], 0, sizeof(*tiletex[i]) * 5129);
		tiletc [i] = malloc(sizeof(*tiletc [i]) * 5129);
		memset(tiletc [i], 0, sizeof(*tiletc [i]) * 5129);
		usedtex[i] = malloc(sizeof(*usedtex[i]) * 5129);
		memset(usedtex[i], 0, sizeof(*usedtex[i]) * 5129);
	}
	// set the MIDI inputs to default states
	for (i=2; i < 100; ++i)
		state[i] = 64;

	// generate the blue-grey ground checkerboard texture
	test_tex = stbgl_TestTexture(2048);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);

	// generate the red-white ground checkerboard texture
	{
		char *tex = malloc(256*256*4);
		int i,j,k=0;
		for (i=0; i < 256; ++i)
			for (j=0; j < 256; ++j)
				if ((i^j)&128)
					tex[k++]=255,tex[k++]=255,tex[k++]=255,tex[k++]=255;
				else
					tex[k++]=255,tex[k++]=0,tex[k++]=0,tex[k++]=255;
		test_tex2 = stbgl_TexImage2D(0, 256, 256, tex, "rgbam");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
		free(tex);
	}

	// generate a dummy texture for the shapes and objects
	{
		unsigned char data[4] = { 255,255,255,255 };
		dummy_tex = stbgl_TexImage2D(0, 1,1, data, "rgba");
	}


	// compute path for first 24*20 frames
	// first 5 seconds are frozen on first frame

	{
		vec o,s,t,n;

		// set up the position for the first frame
		o.x = 0;
		o.y = -2000;
		o.z = 1834;

		s.x = -16*4;
		s.y =  0;
		s.z = 0;

		t.x = 0;
		t.y = 0;
		t.z = -9*4;

		n.x = 0;
		n.y = 1;
		n.z = 0;

		//why doesn't the following just reorient the whole path? confusing
		//vec_rotate_vec(&s, &t, 3*3.141592/180, &s);
		vec_rotate_vec(&t, &s, -45*3.141592/180, &t);

		vec_rotate_vec(&s, &n, 0.54, &s);
		vec_rotate_vec(&t, &n, 0.54, &t);

		for (i=0; i < 24*60; ++i)
		{
			float dist=1;
			make_norm(&n, &t, &s);
			if (i >= 740)
			{
				// straighten path onto simple closed form path to match other logic...
				// actually, no, we end up jumping to a different path entirely anyway,
				// but this is good since it brings it down to the surface in a smooth way
				vec b_s = { -54,-34,0 };
				vec b_o = { 964 - 2*(i-750), 323 + (i-750)*3.5, 36 };
				vec b_t = { 0,0,-36 };
				if (i <= 760)
				{
					float w = stb_linear_remap(i, 740,760, 0,1);
					w = stb_clamp(w,0,1);
					w = 3*w*w - 2*w*w*w;
					vec_lerp(&b_s, &s, &b_s, w);
					vec_lerp(&b_t, &t, &b_t, w);
					vec_lerp(&b_o, &o, &b_o, w);
				}
				make_norm(&n, &b_t, &b_s);
				make_ref(&initial[i], &b_o, &b_s, &b_t, &n);
			}
			else
				make_ref(&initial[i], &o, &s, &t, &n);

			// translate along video normal so successive frames appear in front of prior frames
			if (i >= 0 && i <= 24*3)
			{
				dist = 3;
				if (i > 60)
					dist = stb_linear_remap(i,60,72,3,1);
			}
			vec_addeq_scale(&o, &n, 4.0*dist);

			if (i >= 0 && i <= 175)
			{
				// during first bit, advancing frames move left (so prior frames appear to be to the right)
				vec_addeq_scale(&o, &s, -dist/32);
				vec_addeq_scale(&o, &t, dist/32);
			}
			else if (i < rstart)
			{
				// during the second bit, drift frames to the right
				vec_addeq_scale(&o, &s, dist/32);
				vec_addeq_scale(&o, &t, dist/64);
			}
			// from rstart to rend, we rotate by some amount
			if (i >= rstart && i <= rend)
			{
				float a;
				int trans=25;
				// compute scale factor during transition, to smooth the beginning/ending of the rotation
				if (i < rstart + trans)
					a = (float) (i - rstart) / trans;
				else if (i > rend - trans)
					a = (float) (rend - i) / trans;
				else
					a = 1;
				// smoothstep
				a = 3*a*a - 2*a*a*a;
				// we also use this to control the translation (discontinuous with the prior phase)
				vec_addeq_scale(&o, &s, dist/16*a);
				vec_addeq_scale(&o, &t, dist/16*a);

				// and rotate the position by a bit
				a = -a * 3 * 3.141592/180;
				vec_rotate_vec(&s, &n, a, &s);
				vec_rotate_vec(&t, &n, a, &t);
			}
			// as we come in for a landing, straighten out
			if (i >= t1 && i < t2)
			{
				vec_rotate_vec(&s, &t, 0.008, &s);
				vec_rotate_vec(&t, &s, 0.003, &t);
			}
			else if (i >= t2 && i < t3)
			{
				vec_rotate_vec(&t, &s, 0.0065, &t);
			}
		}
		i=i;
		// when we get to the end, the s,t,n set that we have tells us how we
		// need to rotate the whole object
	}
	e();

	loadMap();

	// build reference frames needed for object placements
	walk_frames(750);

	// load the camera keyframes from disk
	load_keyframes();

	// build objects
	{
		vec o,s,t;
		int i;

//      GLuint t_left, t_right;
		GLuint base1, b;

		// build the t-supports
		base1 = obj_begin(1.0/64);
//      build_dbox(-1,-0.5,0, 0,0.5,36*1.5);
//      build_dbox(64,-0.5,0, 65,0.5,36*1.5);
		build_dbox(30,-2,0, 34,2,36*1.5);
		build_box(2,-0.5,36*1.5, 62,0.5,36*1.5 - 1);
		//build_box(31.5,-32,36*1.5, 32.5,32,36*1.5-1);
		obj_end();

		// place the t-supports on measure boundaries (probably nobody
		// would ever notice this happens except by reading this comment)

		// @WTF, why is this off by ~5-10 frames? that makes no sense,
		// since it's placing it based on the eval frame with numbers
		// that match beats.txt, which is used to make the "pulsing"
		// happen...
		for (i=728+7; i < 2200; i += 48)
		{
			vec step;
			if (i == 1160+7 ) i = 1190+7 ; // long measure
			eval_frame(i, &o, &s, &t, &step);
			o.z = 0;
			make_object(&o,&s,&t, base1, dummy_tex);
		}

		// (notused)
		// make a gate
		b = obj_begin(1.0/64);
		build_box(-40,-10,0,40,10,52);
		build_box(-40,-10,98,40,10,104);
		build_box(-40,-10,52,-32,10,98);
		build_box(32,-10,52,40,10,98);
		obj_end();

		// place a few gates (notused)
		eval_frame_bottom_center(890,&o,&s,&t);
		o.z=0;
		//make_object(&o,&s,&t,b, test_tex);

		eval_frame_bottom_center(1839,&o,&s,&t);
		o.z=0;
		//make_object(&o,&s,&t,b, test_tex);

		eval_frame_bottom_center(1935,&o,&s,&t);
		o.z=0;
		//make_object(&o,&s,&t,b, test_tex);
	}

	e();
	make_cubemap();
	e();

	// allocate the shadow maps
#ifdef FINAL
	make_shadow_map(0, 4096,2048, "D32bC");
	make_shadow_map(1, 2048,2048, "D32bC");
	make_shadow_map(2, 4096,2048, "D32bC");
	//make_shadow_map(0, 1024,1024, "D32bC");
	//make_shadow_map(1, 1024,1024, "D32bC");
	//make_shadow_map(2, 1024,1024, "D32bC");
#else
#ifdef FAST
	make_shadow_map(0, 1024,1024, "D32bC");
	make_shadow_map(1, 1024,1024, "D32bC");
#else
	make_shadow_map(0, 2048,2048, "D32bC");
	make_shadow_map(1, 2048,2048, "D32bC");
#endif
#endif

	// generate the shadowmap helper objects
	if (glGenFramebuffersEXT)
	{
		glGenFramebuffersEXT(1, &shadow_fbo);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shadow_fbo);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, smap[0].tex, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
	}

	// make the shadowmapped shader
	shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	glShaderSourceARB(shader, 1, &shader_source, NULL);
	glCompileShaderARB(shader);
	glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &result);
	if (!result)
	{
		char buffer[1024];
		glGetInfoLogARB(shader, sizeof(buffer), &result, buffer);
		result = result;
		exit(1);
	}
	shadow_prog = glCreateProgramObjectARB();
	glAttachObjectARB(shadow_prog, shader);
	glLinkProgramARB(shadow_prog);
	glGetObjectParameterivARB(shadow_prog, GL_OBJECT_COMPILE_STATUS_ARB, &result);
	if (!result)
	{
		char buffer[1024];
		glGetInfoLogARB(shadow_prog, sizeof(buffer), &result, buffer);
		result = result;
		exit(1);
	}
	else
	{
		// fill out the sampler values now since they'll always be the same later
		glUseProgramObjectARB(shadow_prog);
		result = glGetUniformLocationARB(shadow_prog, "main_tex");
		assert(result >= 0);
		glUniform1iARB(result, 0);
		result = glGetUniformLocationARB(shadow_prog, "shadow1");
		if (result >= 0) glUniform1iARB(result, 1);
		result = glGetUniformLocationARB(shadow_prog, "shadow2");
		if (result >= 0) glUniform1iARB(result, 2);
		result = glGetUniformLocationARB(shadow_prog, "shadow3");
		if (result >= 0) glUniform1iARB(result, 3);
		glUseProgramObjectARB(0);
	}
}

static float last_dt;

// process a display/editor frame
int loopmode(float dt, int real)
{
#ifndef FINAL
	int n;
	uint8 mbuffer[3*100];
#endif

	// compute the amount of time to simulate passing (based on wall-clock time unless we're rendering a final draft)
	float actual_dt = dt;
#ifdef FINAL
	pause = 0;
	dt = actual_dt = 1.0/FINAL;
#endif
	// dt is used for ffwd/rewind; don't allow it to skip too fast
	if (dt > 0.05f) dt = 0.05f;

	// real time is the actual time that's passed
	real_time += actual_dt;

	// process MIDI input controls
#ifndef FINAL
	n = midi_poll(mbuffer, 300);
	do_midi(mbuffer, n);
	ui_sim(dt);
#endif

#ifdef FINAL
	// if rendering a final draft, advance based on the simulated time
	simulate(actual_dt);
	// nah, let's just rewrite to an effective exact time that matches
	// the frame rate, so we don't get accumulated error
	real_time = frame_number / (float) FINAL;
	raw_when = frame_number * (FINAL/24.0);
	update_state();
#else
	// if not rendering a final draft, advance by actual_dt if we're not paused
	if (!pause)
	{
		simulate(actual_dt);
	}
	update_state();
#endif

	// do all the rendering
	e();
	draw(dt);
	e();

	// if doing final rendering, exit when done
#ifdef FINAL
	if (raw_when > 5030)
		exit(0);
#endif

	return 0;
}

// windows event processing
int winproc(void *data, stbwingraph_event *e)
{
	static int scroll;
	static int change=0;
	switch (e->type)
	{
	case STBWGE_create:
		break;
	case STBWGE_destroy:
		//save_keyframes();  // commented out to avoid accidental overwrites
		break;
	case STBWGE_char:
		switch(e->key)
		{
		case 27:
		{
			//save_keyframes();  // commented out to avoid accidental overwrites
			exit(0);
		}
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			break;

			// edit a keyframe at this moment in time
		case 'k':
		{
			edit_keyframe((int) when);
			break;
		}

		// goto next and previous keyframe
		case '.':
			pause = 1;
			goto_next_keyframe();
			break;
		case ',':
			pause = 1;
			goto_prev_keyframe();
			break;

		case 'S'-64:
			break;//save_keyframes(); break;  // commented out to avoid accidental overwrites

			// advance by a frame
		case 's': /*editing_keyframe = -1;*/
			raw_when += 1;
			break;
			// step back by a frame
		case 'r': /*editing_keyframe = -1;*/
			raw_when -= 1;
			break;

			// rewind to beginning
		case 'R':
			editing_keyframe = -1;
			raw_when = 0;
			break;

			// reload objects
		case 'O':
		{
			int x,y;
			object_map = stbi_load("objects.png", &x, &y, NULL, 1);
			break;
		}
		// reload map
		case 'm':
			loadMap();
			break;

			// update hack variable used during developemtn
		case '+':
		case '=':
			hack += 1;
			break;
		case '-':
			hack -= 1;
			break;
			// print current frame number
		case 'w':
			OutputDebugString(stb_sprintf("When: %f\n", when));
			break;

			// toggle paused state
		case 'p':
			pause = !pause;
			break;

			// toggle video frame rendering quality
		case 'q':
			quality = !quality;
			break;
		}
		break;

	case STBWGE_size:
		sx = e->width;
		sy = e->height;
		loopmode(0,1);
		break;

	case STBWGE_draw:
		if (initialized)
			loopmode(0,1);
		break;

	case STBWGE_mousemove:
		mouse_x = e->mx, mouse_y = e->my;
		break;
	case STBWGE_leftdown:
		mouse_x = e->mx, mouse_y = e->my;
		left_down = 1;
		break;
	case STBWGE_leftup:
		mouse_x = e->mx, mouse_y = e->my;
		left_down = 0;
		break;
	case STBWGE_middledown:
		break;
	case STBWGE_middleup:
		break;
	case STBWGE_mousewheel:
	{
		break;
	}

	default:
		return STBWINGRAPH_unprocessed;
	}
	return 0;
}

// this is only called when messages are pumped
static void CALLBACK midi_callback(HMIDIIN hmid, UINT wMsg, DWORD instance, DWORD p1, DWORD p2)
{
	switch (wMsg)
	{
	case MIM_MOREDATA:
	case MIM_DATA:
		if (offset < 3*NUM_MESSAGES)
		{
			midi_buffer[offset++] = (unsigned char) p1;
			p1 >>= 8;
			midi_buffer[offset++] = (unsigned char) p1;
			p1 >>= 8;
			midi_buffer[offset++] = (unsigned char) p1;
			p1 >>= 8;
		}
		break;
	}
}

void midi_setup(void)
{
	UINT x = midiInGetNumDevs();
	unsigned int i;
	for (i = 0; i < x; ++i)
	{
		HMIDIIN midih;
		MMRESULT m = midiInOpen(&midih, i, (DWORD) (void *) midi_callback, 0, CALLBACK_FUNCTION);
		if (m == MMSYSERR_NOERROR)
		{
			midiInStart(midih);
		}
	}
}




void stbwingraph_main(void)
{
// create window
#ifdef FINAL
	// for final rendering, we render a little large so we can downsample to 720p and slightly reduce aliasing
	stbwingraph_CreateWindow(31, winproc, NULL, "VolG", 1440,840, 0, 1, 0, 0);
#else
	// normal rendering is straight 720p
	stbwingraph_CreateWindow(31, winproc, NULL, "VolG", 1280, 720, 0, 1, 0, 0);
#endif
	e();

// various initializations
	stbgl_initExtensions(); // load opengl extensions
	e();
	midi_setup();
	tileinfo[0] = read_headers("f:/wtf/alpha_tiles/wtf_headers.bin");
	tileinfo[1] = read_headers("f:/wtf/alpha_4x_tiles/wtf_headers.bin");
	tileinfo[2] = read_headers("f:/wtf/alpha_16x_tiles/wtf_headers.bin");
	init();
	// now that we're done initializing, set initialized flag
	initialized = 1;
	e();

#ifdef FINAL
	// set which frame to start rendering the final images at
	frame_number = 650;
	raw_when = frame_number / (float) FINAL;
#endif

	// run the main loop, with a frame rate limit of 75fps
	stbwingraph_MainLoop(loopmode, 1.0f/75);
}




#if 0
// splines are too hard to control given the kind of simple
// curves we want for the path of the video frames
typedef struct
{
	int i;
	float rate;
	float w,h;
	vec pos, up;
	float force_up;
} SplineRef;

#define OFFX (-2  * 25)
#define OFFY (3.5 * 25)

SplineRef main_spline[20] =
{
	{  400, 0, 64,36, { 964 + 300 - 54, 323 - 150*3.5 - 34, 36*2.5}, { 0,0,1 }, 1 },
	{  700, 0, 64,36, { 964 + 200 - 54, 323 - 350 - 34, 36*2.5 }, { 0,0,1 }, 1 },
	{ 1000, 0, 64,36, { 964 +  50 - 54 + OFFX, 323 - 25*3.5 - 34 + OFFY, 36 }, { 0,0,1 }, 1 },
	{ 1200, 0, 64,36, { 964 + -50 - 54 + OFFX, 323 + 25*3.5 - 34 + OFFY, 36 }, { 0,0,1 }, 1 },
	{ 1400, 0, 64,36, { 700 + OFFX, 500 + OFFY, 36 }, { 0,0,1 } },
	{ 2400, 0, 64,36, { -1000 + OFFX, 500 + OFFY, 36 }, { 0,0,1 } },
//   { 5600, 0, 64,36, { -6800 + OFFX, 500 + OFFY, 36 }, { 0,0,1 } },
//   { 8000, 0, 64,36, { -12000 + OFFX, 500 + OFFY, 36 }, { 0,0,1 } },
};

//   compute_vec_bezier(e->pos, t, pos);
//   compute_vec_bezier_derivative(e->pos, t, &forward);
//   compute_vec_bezier(e->up, t, &up);

void eval_spline(SplineRef *sr, float i, vec *o, vec *s, vec *t)
{
	float w;
	float dist;
	vec pos[4],up[4], forward;
	vec tup;
	vec dir;
	pos[0] = sr[0].pos;
	up[0] = sr[0].up;
	pos[3] = sr[1].pos;
	up[3] = sr[3].up;
	dist = vec_dist(&pos[0], &pos[3]);
	vec_sub(&dir, &sr[1].pos, &sr[-1].pos);
	vec_normeq(&dir);
	vec_add_scale(&pos[1], &pos[0], &dir, dist/4);
	vec_sub(&dir, &sr[1].up, &sr[-1].up);
	vec_add_scale(&up[1], &up[0], &dir, 1/4);

	if (sr[2].i == 0)
	{
		pos[2] = pos[3];
		up[2] = up[3];
	}
	else
	{
		vec_sub(&dir, &sr[2].pos, &sr[0].pos);
		vec_normeq(&dir);
		vec_add_scale(&pos[2], &pos[3], &dir, -dist/4);

		// should be quat instead of vector? shouldn't average and extrapolate?
		vec_sub(&dir, &sr[2].up, &sr[0].up);
		vec_add_scale(&up[2], &up[3], &dir, -1/4);
	}


	w = stb_linear_remap(i, sr[0].i, sr[1].i, 0, 1);
	// stretch w according to 'rate'... if rate is 0 unsqueezed at that end
	// so subtract rates, and use that as "power" of spread
	w = pow(w, pow(2, sr[0].rate - sr[1].rate));

	compute_vec_bezier(pos, w, o);
	compute_vec_bezier(up, w, &tup);
	compute_vec_bezier_derivative(pos, w, &forward);
	vec_cross(s, &forward, &tup);
	vec_cross(t, &forward, s);
	vec_normeq(s);
	vec_normeq(t);
	vec_scaleeq(&tup, -1);
	vec_lerp(t, t, &tup, stb_lerp(w, sr[0].force_up, sr[1].force_up));
	vec_normeq(t);

	vec_scaleeq(s, stb_lerp(w, sr[0].w, sr[1].w));
	vec_scaleeq(t, stb_lerp(w, sr[0].h, sr[1].h));
}
#endif

