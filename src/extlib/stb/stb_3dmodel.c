#include <math.h>
#include <memory.h>
#include "stb_3dmodel.h"

static void float16_mul(float out[16], float mat1[16], float mat2[16]);
static void float16_from_quat_vec(float mat[16], float *q, float *v);
void float16_transposeeq(float *m);
void float16_transpose(float *d, float *m);

void model_convert_bone2world_to_skin2world(float bones[][16], STB3D_Model *model)
{
	int i;
	for (i=0; i < model->num_bones; ++i)
	{
		float temp[16];
		memcpy(temp, bones[i], sizeof(temp));
		float16_mul(bones[i], model->bones[i].bind_xform, temp);
	}
}

void model_convert_bonelocal_to_bone2world(float bones[][16], STB3D_Model *model, STB3D_ModelTransform *raw_bones)
{
	int i;
	for (i=0; i < model->num_bones; ++i)
	{
		int n = model->bones[i].parent;
		float local[16];
		float16_from_quat_vec(local, raw_bones[i].rot_quat, raw_bones[i].translate);
		if (n >= 0)
		{
			float16_mul(bones[i], local, bones[n]);
		}
		else
		{
			memcpy(bones[i], local, sizeof(bones[i]));
		}
	}
}

void model_convert_bonelocal_to_skin2world(float bones[][16], STB3D_Model *model, STB3D_ModelTransform *raw_bones)
{
	model_convert_bonelocal_to_bone2world(bones, model, raw_bones);
	model_convert_bone2world_to_skin2world(bones, model);
}

// @TODO: at endpoints of non-looped, directly control the slope so it goes to 0?
// t is in between p1 and p2
static float stb__cubic_interpolate(float p0, float p1, float p2, float p3, float t)
{
	float a,b;
	a = p1 + (p2-p0)/2 * t;
	b = p2 + (p3-p1)/2 * (t-1);
	return a + t*(b-a);
}

static void xform_cubic_interpolate(STB3D_ModelTransform *dest, STB3D_ModelTransform *x0, STB3D_ModelTransform *x1, STB3D_ModelTransform *x2, STB3D_ModelTransform *x3, float t)
{
	int i;
	float *d = (float *) dest;
	float *p0 = (float *) x0;
	float *p1 = (float *) x1;
	float *p2 = (float *) x2;
	float *p3 = (float *) x3;
	for (i=0; i < 7; ++i)
		d[i] = stb__cubic_interpolate(p0[i],p1[i],p2[i],p3[i],t);
}

static void xform_linear_interpolate(STB3D_ModelTransform *dest, STB3D_ModelTransform *x1, STB3D_ModelTransform *x2, float t)
{
	int i;
	float *d = (float *) dest;
	float *p1 = (float *) x1;
	float *p2 = (float *) x2;
	for (i=0; i < 7; ++i)
		d[i] = p1[i] + (p2[i]-p1[i]) * t;
}


static keyframe(int n, int frames, int looped)
{
	if (n >= frames)
		if (looped)
			return n - frames;
		else
			return frames-1;
	if (n < 0)
		if (looped)
			return n + frames;
		else
			return 0;
	return n;
}

void get_animation_bones(STB3D_ModelTransform *bones, STB3D_ModelAnimationSet *anim, int number, float atime, int looped, int interpolation)
{
	STB3D_ModelBoneAnimation *alist = anim->anim[number];
	int i, frames = anim->anim[number][0].num_frames;
	int a1,a2, a0,a3;
	float lerp_factor;

	a1 = (int) floor(atime);
	lerp_factor = atime - a1;
	a1 = keyframe(a1, frames, looped);

	if (interpolation == STB3D_MODEL_interpolate_nearest)
	{
		for (i=0; i < anim->num_bones; ++i)
		{
			STB3D_ModelBoneAnimation *z = &anim->anim[number][i];
			memcpy(bones+i, &z->frame[a1].rest_xform, sizeof(bones[0]));
		}
	}
	else
	{
		if (interpolation == STB3D_MODEL_interpolate_linear)
		{
			a2 = keyframe(a1+1, frames, looped);
			for (i=0; i < anim->num_bones; ++i)
			{
				STB3D_ModelBoneAnimation *z = &anim->anim[number][i];
				xform_linear_interpolate(bones+i, &z->frame[a1].rest_xform, &z->frame[a2].rest_xform, lerp_factor);
			}
		}
		else
		{
			a2 = keyframe(a1+1, frames, looped);
			a0 = keyframe(a1-1, frames, looped);
			a3 = keyframe(a2+1, frames, looped);
			for (i=0; i < anim->num_bones; ++i)
			{
				STB3D_ModelBoneAnimation *z = &anim->anim[number][i];
				xform_cubic_interpolate(bones+i, &z->frame[a0].rest_xform, &z->frame[a1].rest_xform, &z->frame[a2].rest_xform, &z->frame[a3].rest_xform, lerp_factor);
			}
		}
	}
}

void anim_get_bones(STB3D_ModelTransform *bones, STB3D_AnimationState *as, int interpolation)
{
	get_animation_bones(bones, as->set, as->anim_number, as->anim_time, as->state.loop, interpolation);
}

void animabstract_init(STB3D_AnimationStateAbstract *as, float length, float move[3], float rot, int loop)
{
	as->length = length;
	as->movement[0] = move[0] / length;
	as->movement[1] = move[1] / length;
	as->movement[2] = move[2] / length;
	as->rotation = rot / length;
	as->rotation = rot;
	as->loop = loop;

	as->anim_scale = 1;
	as->anim_time = 0;
}

void animstate_init(STB3D_AnimationState *as, STB3D_ModelAnimationSet *set, int anim_number, float fps, float move[3], float rot, int loop, float phase)
{
	as->set = set;
	as->anim_number = anim_number;
	as->num_frames = as->set->anim[anim_number][0].num_frames;
	as->fps = fps;
	as->phase = phase / fps;
	animabstract_init(&as->state, as->num_frames / fps, move, rot, loop);
}

void animabstract_sim(STB3D_AnimationStateAbstract *as, float dt, float *dpos, float *dori)
{
	if (!as->sync)
	{
		dt *= as->anim_scale;
		as->anim_time += dt;
		if (as->anim_time >= as->length)
		{
			if (!as->loop)
				as->anim_time = as->length;
			else do
				{
					as->anim_time -= as->length;
				}
				while (as->anim_time >= as->length);
		}
	}
	else
	{
		// synchronize to parent
		float old_time = as->anim_time;
		as->anim_time  = as->sync->anim_time * as->sync_rate + as->sync_phase;
		if (as->anim_time >= as->length) as->anim_time -= as->length;
		// reconstruct delta dt based on how far we change so that
		// rate computations below are valid
		dt = as->anim_time - old_time;
		if (dt < 0)
		{
			dt += as->length;
			if (dt < 0) while (dt < 0) dt += as->length;
		}
		else if (dt >= as->length)
		{
			dt -= as->length;
			if (dt >= as->length) while (dt >= as->length) dt -= as->length;
		}
		// make sure if we 'detach' the sync we stay at the same time/rate
		as->anim_scale = as->sync->anim_scale * as->sync_rate;
	}

	*dori = as->rotation * dt; // radians/sec, so radians output

	dpos[0] = as->movement[0] * dt;
	dpos[1] = as->movement[1] * dt;
	dpos[2] = as->movement[2] * dt;
}

void anim_sim(STB3D_AnimationState *as, float dt, float *dpos, float *dori)
{
	animabstract_sim(&as->state, dt, dpos, dori);
	as->anim_time = as->state.anim_time * as->fps;
}

void anim_sync(STB3D_AnimationState *as, STB3D_AnimationState *parent)
{
	if (!parent == 0)
		as->state.sync = 0;
	else
	{
		as->state.sync = &parent->state;
		as->state.sync_rate = as->state.length / parent->state.length;
		as->state.sync_phase = as->phase - parent->phase * as->state.sync_rate;
		while (as->state.sync_phase < 0) as->state.sync_phase += as->state.length;
		while (as->state.sync_phase >= as->state.length) as->state.sync_phase -= as->state.length;
	}
}

// @todo: include in each bone-bounds any vertices from triangles that
//          have any vertices in the bone (i.e. handle triangles that span bones)
void model_compute_bone_bounds(STB3D_Model *model)
{
	int i,j,k;
	STB3D_ModelBone *b = model->bones;

	unsigned char bone_touched[512];
	memset(bone_touched, 0, model->num_bones);

	for (i=0; i < model->num_bones; ++i)
	{
		for (j=0; j < 3; ++j)
			b[i].bbox[0][j] =  1e20f,
			                   b[i].bbox[1][j] = -1e20f;
	}
	for (i=0; i < model->num_meshes; ++i)
	{
		STB3D_ModelMesh *m = &model->meshes[i];
		for (j=0; j < m->num_vertices; ++j)
		{
			float *pos = m->vertices[j].pos;
			for (k=0; k < STB3D_MAX_VERTEX_BONES; ++k)
			{
				if (m->vertices[j].weight[k])
				{
					int z = m->vertices[j].bone[k];
					float bp[3], *xf = b[z].bind_xform;
					bone_touched[z] = 1;
					bp[0] = xf[0] * pos[0] + xf[1] * pos[1] + xf[ 2] * pos[2] + xf[ 3];
					bp[1] = xf[4] * pos[0] + xf[5] * pos[1] + xf[ 6] * pos[2] + xf[ 7];
					bp[2] = xf[8] * pos[0] + xf[9] * pos[1] + xf[10] * pos[2] + xf[11];
					if (bp[0] < b[z].bbox[0][0]) b[z].bbox[0][0] = bp[0];
					if (bp[0] > b[z].bbox[1][0]) b[z].bbox[1][0] = bp[0];
					if (bp[1] < b[z].bbox[0][1]) b[z].bbox[0][1] = bp[1];
					if (bp[1] > b[z].bbox[1][1]) b[z].bbox[1][1] = bp[1];
					if (bp[2] < b[z].bbox[0][2]) b[z].bbox[0][2] = bp[2];
					if (bp[2] > b[z].bbox[1][2]) b[z].bbox[1][2] = bp[2];
				}
			}
		}
	}
	for (i=0; i < model->num_bones; ++i)
	{
		if (!bone_touched[i])
			for (j=0; j < 3; ++j)
				b[i].bbox[0][j] = 0,
				                  b[i].bbox[1][j] = 0;
	}
}

static void float16_mul(float out[16], float mat1[16], float mat2[16])
{
	int i,j;
	for (j=0; j < 3; ++j)
		for (i=0; i < 4; ++i)
			out[4*j+i] = mat1[4*0+i]*mat2[4*j+0]
			             + mat1[4*1+i]*mat2[4*j+1]
			             + mat1[4*2+i]*mat2[4*j+2]
			             + mat1[4*3+i]*mat2[4*j+3];
	out[12] = out[13] = out[14] = 0;
	out[15] = 1;
}

static void float16_from_quat_vec(float mat[16], float *q, float *v)
{
	float x2    = 2 * q[0];
	float qx2_2 = x2 * q[0];
	float qxy_2 = x2 * q[1];
	float qxz_2 = x2 * q[2];
	float qxw_2 = x2 * q[3];

	float y2    = 2 * q[1];
	float qy2_2 = y2 * q[1];
	float qyz_2 = y2 * q[2];
	float qyw_2 = y2 * q[3];

	float z2    = 2 * q[2];
	float qz2_2 = z2 * q[2];
	float qzw_2 = z2 * q[3];

	mat[4*0+0] = 1 - qy2_2 - qz2_2;
	mat[4*0+1] =     qxy_2 - qzw_2;
	mat[4*0+2] =     qxz_2 + qyw_2;
	mat[4*0+3] = v[0];

	mat[4*1+0] =     qxy_2 + qzw_2;
	mat[4*1+1] = 1 - qx2_2 - qz2_2;
	mat[4*1+2] =     qyz_2 - qxw_2;
	mat[4*1+3] = v[1];

	mat[4*2+0] =     qxz_2 - qyw_2;
	mat[4*2+1] =     qyz_2 + qxw_2;
	mat[4*2+2] = 1 - qx2_2 - qy2_2;
	mat[4*2+3] = v[2];

	mat[4*3+0] = 0;
	mat[4*3+1] = 0;
	mat[4*3+2] = 0;
	mat[4*3+3] = 1;
}

#ifndef STB_GL
#if defined(__gl_h_) || defined(__GL_H__) || defined(GL_VERSION_1_1) || defined(GL_VERSION_1_2) || defined(GL_VERSION_1_3) || defined(GL_VERSION_1_4) || defined(GL_VERSION_2_0) || defined(GL_VERSION_2_1) || defined(GL_VERSION_2_2) || defined(GL_VERSION_2_3) || defined(GL_VERSION_3_0)
#define STB_GL
#endif
#endif
