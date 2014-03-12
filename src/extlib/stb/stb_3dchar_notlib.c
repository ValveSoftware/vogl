#include "stb_3dmodel.h"

#define NEIGHBORHOOD_WHEN_BLENDING 1

void draw_smesh_blend_vert(STB3D_ModelMesh *mesh, float (*bones)[16])
{
	int i;
	if (mesh->mat)
	{
		glBindTexture(GL_TEXTURE_2D, mesh->mat->texid);
		glColor4fv(mesh->mat->color);
	}
	glBegin(GL_TRIANGLES);
	{
		STB3D_ModelVertex *v = mesh->vertices;
		for (i=0; i < mesh->num_vertex_indices; ++i)
		{
			int j;
			int k = mesh->vertex_index_list[i];
			float matrix[16] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
			float pos[4];
			for (j=0; j < STB3D_MAX_VERTEX_BONES; ++j)
			{
				if (v[k].weight[j])
				{
					int q;
					float w = v[k].weight[j] / 255.0f;
					float *b = bones[v[k].bone[j]];
					for (q=0; q < 16; ++q)
						matrix[q] += w * b[q];
				}
			}

			glTexCoord2fv(v[k].tc);
			pos[0] = matrix[0] * v[k].norm[0] + matrix[4] * v[k].norm[1] + matrix[ 8] * v[k].norm[2];
			pos[1] = matrix[1] * v[k].norm[0] + matrix[5] * v[k].norm[1] + matrix[ 9] * v[k].norm[2];
			pos[2] = matrix[2] * v[k].norm[0] + matrix[6] * v[k].norm[1] + matrix[10] * v[k].norm[2];
			glNormal3fv(pos);
			pos[0] = matrix[0] * v[k].pos[0] + matrix[4] * v[k].pos[1] + matrix[ 8] * v[k].pos[2] + matrix[12];
			pos[1] = matrix[1] * v[k].pos[0] + matrix[5] * v[k].pos[1] + matrix[ 9] * v[k].pos[2] + matrix[13];
			pos[2] = matrix[2] * v[k].pos[0] + matrix[6] * v[k].pos[1] + matrix[10] * v[k].pos[2] + matrix[14];
			glVertex3fv(pos);
		}
	}
	glEnd();
}

void draw_smesh_prevert(STB3D_ModelMesh *mesh, float *verts, STB3D_Model *model, int voff, int ioff)
{
	STB3D_ModelVertex *v = mesh->vertices;
	if (mesh->mat)
	{
		glBindTexture(GL_TEXTURE_2D, mesh->mat->texid);
		glColor4fv(mesh->mat->color);
	}

	if (glBindBufferARB)
	{
		if (model->static_partial_vertex_buffer)
		{
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, model->static_partial_vertex_buffer);
			glTexCoordPointer(2,GL_FLOAT,8, (char *)0 + voff*8);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}
		else
		{
			glTexCoordPointer(2,GL_FLOAT,(char *)v[1].tc - (char *)v[0].tc, &v[0].tc);
		}
		if (model->streaming_vertex_buffer)
		{
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, model->streaming_vertex_buffer);
			glNormalPointer(GL_FLOAT,6*4, (char *)0 + voff*24);
			glVertexPointer(3,GL_FLOAT,6*4, (char *)0 + voff*24 + 12);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}
		else
		{
			glNormalPointer(GL_FLOAT,6*4, verts);
			glVertexPointer(3,GL_FLOAT,6*4, verts+3);
		}
	}
	else
	{
		glTexCoordPointer(2,GL_FLOAT,(char *)v[1].tc - (char *)v[0].tc, &v[0].tc);
		glNormalPointer(GL_FLOAT,6*4, verts);
		glVertexPointer(3,GL_FLOAT,6*4, verts+3);
	}

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);


	if (model->static_index_buffer)
	{
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, model->static_index_buffer);
		glDrawElements(GL_TRIANGLES, mesh->num_vertex_indices, GL_UNSIGNED_SHORT, (void *) (ioff*2));
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}
	else
	{
		glDrawElements(GL_TRIANGLES, mesh->num_vertex_indices, GL_UNSIGNED_SHORT, mesh->vertex_index_list);
	}
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void draw_smesh_prevert_shadow(STB3D_ModelMesh *mesh, float *verts)
{
	STB3D_ModelVertex *v = mesh->vertices;
	if (mesh->mat)
	{
		glBindTexture(GL_TEXTURE_2D, mesh->mat->texid);
		glColor4fv(mesh->mat->color);
	}
	glVertexPointer(3,GL_FLOAT,6*4, verts+3);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDrawElements(GL_TRIANGLES, mesh->num_vertex_indices, GL_UNSIGNED_SHORT, mesh->vertex_index_list);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void compute_mesh_verts(float *verts, STB3D_ModelMesh *mesh, float (*bones)[16])
{
	int j,k;
	STB3D_ModelVertex *v = mesh->vertices;
	for (k=0; k < mesh->num_vertices; ++k)
	{
		float matrix[16] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
		for (j=0; j < STB3D_MAX_VERTEX_BONES; ++j)
		{
			if (v[k].weight[j])
			{
				int q;
				float w = v[k].weight[j] / 255.0f;
				float *b = bones[v[k].bone[j]];
				for (q=0; q < 16; ++q)
					matrix[q] += w * b[q];
			}
		}

		verts[0] = matrix[0] * v[k].norm[0] + matrix[1] * v[k].norm[1] + matrix[ 2] * v[k].norm[2];
		verts[1] = matrix[4] * v[k].norm[0] + matrix[5] * v[k].norm[1] + matrix[ 6] * v[k].norm[2];
		verts[2] = matrix[8] * v[k].norm[0] + matrix[9] * v[k].norm[1] + matrix[10] * v[k].norm[2];
		verts[3] = matrix[0] * v[k].pos[0] + matrix[1] * v[k].pos[1] + matrix[ 2] * v[k].pos[2] + matrix[ 3];
		verts[4] = matrix[4] * v[k].pos[0] + matrix[5] * v[k].pos[1] + matrix[ 6] * v[k].pos[2] + matrix[ 7];
		verts[5] = matrix[8] * v[k].pos[0] + matrix[9] * v[k].pos[1] + matrix[10] * v[k].pos[2] + matrix[11];
		verts += 6;
	}
}

typedef struct
{
	char name[32];
	vec move;
	float ang_z;
	float fps;
	int loop;
	float phase; // used to synchronize looped animations of similar type
} AnimationData;

typedef struct st_CharAnimControl CharAnimControl;

#define NUM_CHAR_ANIM   8
typedef struct
{
	STB3D_Model *model;
	vec loc;
	float facing;
	STB3D_AnimationState anim[NUM_CHAR_ANIM];
	AnimationData *anim_data;
	float anim_weight[NUM_CHAR_ANIM];

	float bone_mat[64][16]; // cached bone matrices to allow redrawing multiple times more efficiently
	float bone_debug[64][16];
	int is_cached;

	float *verts;
	int vert_cached;

	CharAnimControl *control;
} Character;

// each character can have NUM_CHAR_ANIM animations running
// simultaneously. we call each such animation-instance a 'bus'
// for lack of a better name. (I'd like to call them a 'channel',
// but that already means a per-bone thing, so I'll call them
// 'bus' by analogy to audio mixing's channels and buses.)

// Each animation bus group is in a group of one or more busses that
// are controlled as a single entity. Each such group can have
// ease-in and ease-out curves applied to its start and end, as
// well as an initial delay before the animation actually starts.

enum
{
	BLEND_start_delay,
	BLEND_wait,
	BLEND_blendin,
	BLEND_full,
	BLEND_blendout,
};

typedef struct
{
	float t;
	float start_delay;
	float initial_wait_time;
	float blend_in_time;
	float full_time;
	float blend_out_time;
	float max_weight;
	float cur_weight;
	float desired_scale;
	float time_left;
	unsigned char phase;
	signed char sync;              // cross-set synchronization is speed-adaptive as they blend
	unsigned char sync_count;
	char *name;
} AnimGroupProperties;

typedef struct st_CharAnimControl
{
	Character *c;
	signed char which_group[NUM_CHAR_ANIM]; // which group a given animation bus is on
	float group_weight[NUM_CHAR_ANIM]; // how much weight this animation accounts for of the bus
	AnimGroupProperties group[NUM_CHAR_ANIM]; // various animation properties

	float time_left; // time left until all animations end
};

float ease(float x)
{
	return (3 - 2*x)*x*x;
}
float superease(float x)
{
	return ease(x);
}

void animcontrol_sim(CharAnimControl *a, float dt)
{
	int i;
	int sync_count[NUM_CHAR_ANIM];
	a->time_left = 0;

	// for each active-animation-set, update the ease-in-out curves
	for (i=0; i < NUM_CHAR_ANIM; ++i)
	{
		if (a->which_group[i] == i)
		{
			AnimGroupProperties *as = &a->group[i];
			as->t += dt;
			as->time_left -= dt;
			if (as->time_left > a->time_left)
				a->time_left = as->time_left;
			if (as->phase == BLEND_start_delay && as->t >= as->start_delay)
			{
				as->t -= as->start_delay;
				a->c->anim[i].state.anim_scale = as->desired_scale;
			}
			if (as->phase == BLEND_wait && as->t >= as->initial_wait_time)
			{
				++as->phase;
				as->t -= as->initial_wait_time;
			}
			if (as->phase == BLEND_blendin)
				if (as->t >= as->blend_in_time)
				{
					++as->phase;
					as->t -= as->blend_in_time;
					as->cur_weight = as->max_weight;
				}
				else
					as->cur_weight = superease(as->t / as->blend_in_time);
			if (as->phase == BLEND_full && as->t >= as->full_time)
			{
				++as->phase;
				as->t -= as->full_time;
			}
			if (as->phase == BLEND_blendout)
				if (as->t > as->blend_out_time)
				{
					// deallocate bus
					a->which_group[i] = -1;
					a->c->anim[i].set = NULL;
				}
				else
					as->cur_weight = superease(1 - as->t / as->blend_out_time);
		}
		else if (a->which_group[i] >= 0 && a->which_group[a->which_group[i]] < 0)
		{
			// parent bus just stopped, so deallocate bus
			a->c->anim[i].set = NULL;
			a->which_group[i] = -1;
		}

		if (a->which_group[i] >= 0)
			a->c->anim_weight[i] = a->group_weight[i] * a->group[a->which_group[i]].cur_weight;
	}

	memset(sync_count, 0, sizeof(NUM_CHAR_ANIM));
	for (i=0; i < NUM_CHAR_ANIM; ++i)
		if (a->which_group[i] == i)
			if (a->group[i].sync >= 0)
				++sync_count[i];

	for (i=0; i < NUM_CHAR_ANIM; ++i)
	{
		if (sync_count[i] > 1)
		{
			int j;
			float w=0, scale=0;
			for (j=0; j < NUM_CHAR_ANIM; ++j)
			{
				if (a->which_group[j] == j && a->group[j].sync == j)
				{
					scale += a->group[j].desired_scale / a->c->anim[j].state.length * a->group[j].cur_weight;
					w += a->group[j].cur_weight;
				}
			}
			if (w)
			{
				scale /= w;
				for (j=0; j < NUM_CHAR_ANIM; ++j)
				{
					if (a->which_group[j] == j && a->group[j].sync == j)
					{
						a->c->anim[j].state.anim_scale = a->group[j].desired_scale * scale * a->c->anim[j].state.length;
					}
				}
			}
		}
	}
}


void animcontrol_make_group(CharAnimControl *a, int s, char *name,
                            float start_delay, float initial_wait_time, float blend_in_time, float full_time, float blend_out_time,
                            float weight_of_group, float weight_in_group, float desired_scale)
{
	a->which_group[s] = s;
	a->group[s].name = name;
	a->group[s].start_delay = start_delay;
	a->group[s].blend_in_time = blend_in_time;
	a->group[s].cur_weight = 0;
	a->group[s].desired_scale = desired_scale;
	a->group[s].initial_wait_time = initial_wait_time;
	a->group[s].full_time = full_time;
	a->group[s].max_weight = weight_of_group;
	a->group[s].sync = s;
	a->group[s].t = 0;
	a->group[s].time_left = start_delay + blend_in_time + initial_wait_time + full_time + blend_out_time;
	a->group[s].blend_out_time = blend_out_time;
	a->group_weight[s] = weight_in_group;

	if (start_delay)
	{
		a->group[s].phase = BLEND_start_delay;
		a->c->anim[s].state.anim_scale = 0;
		a->c->anim_weight[s] = 0;
	}
	else
	{
		a->group[s].phase = BLEND_wait;
		a->c->anim[s].state.anim_scale = desired_scale;
		a->c->anim_weight[s] = 0;
	}
}

void animcontrol_add_to_group(CharAnimControl *a, int anim, int group, float weight_in_group)
{
	a->which_group[anim] = group;
	a->group_weight[anim] = weight_in_group;
}

void animcontrol_stop_group(CharAnimControl *a, int s, float fade_out_time)
{
	assert(a->which_group[s] == s);
	if (fade_out_time == 0)
	{
		a->which_group[s] = -1;
		a->c->anim[s].set = NULL;
	}
	else
	{
		if (a->group[s].time_left > fade_out_time)
		{
			if (a->group[s].phase != BLEND_blendout)
			{
				// easy case
				a->group[s].phase = BLEND_blendout;
				a->group[s].t = 0;
				a->group[s].blend_out_time = fade_out_time;
			}
			else
			{
				// hard case - compute the existing lerp time
				float lerp = a->group[s].t / a->group[s].blend_out_time;
				if (lerp < 1)
				{
					// now compute a new overall time such that the remaining time is fade_out_time
					//   new_t / new_time = t              => new_t = t * new_time
					//   new_time - new_t = fade_out_time  => new_t = new_time - fade_out_time
					//   new_time - fade_out_time = t * new_time
					//   new_time - t*new_time = fade_out_time
					//  (1-t)*new_time = fade_out_time
					a->group[s].blend_out_time = fade_out_time / (1-lerp);
					a->group[s].t = lerp * a->group[s].blend_out_time;
				}
			}
		}
	}
}

// fade out all existing animations
void animcontrol_stop_all(CharAnimControl *a, float fade_out_time)
{
	int i;
	for (i=0; i < NUM_CHAR_ANIM; ++i)
		if (a->which_group[i] == i)
			animcontrol_stop_group(a, i, fade_out_time);
}

int animcontrol_free_slot(CharAnimControl *a)
{
	int i;
	for (i=0; i < NUM_CHAR_ANIM; ++i)
		if (a->c->anim[i].set == NULL)
			return i;
	return -1;
}

int animcontrol_play(CharAnimControl *a, int anim, float trans_time, float weight, float scale)
{
	int i = animcontrol_free_slot(a);
	// if we don't get a free bus, go ahead and stop everything anyway,
	// because we obviously need to free up busses
	animcontrol_stop_all(a, trans_time);
	if (i >= 0)
	{
		float duration;
		animstate_init(&a->c->anim[i], a->c->model->anim, anim, a->c->anim_data[anim].fps, &a->c->anim_data[anim].move.x, a->c->anim_data[anim].ang_z, a->c->anim_data[anim].loop, a->c->anim_data[anim].phase);
		if (a->c->anim[i].state.loop)
			duration = 3600; // characters will DEFINITELY reconsider what to do once an hour
		else
			duration = a->c->anim[i].state.length / scale - trans_time + 0.1f; // extra time at end
		animcontrol_make_group(a, i, a->c->anim_data[anim].name, 0,0,trans_time, duration - 0.25f, 0.25f, weight, 1, scale);
	}
	return i;
}

Character ch;

void sim_char_old(Character *c, float dt)
{
	int i;
	float dfacing, dw;
	vec step,move;

	dt *= anim_scale;
	step = vec_zero();
	dfacing = 0;
	dw = 0;

	if (c->anim[1].set)
	{
		c->anim_weight[0] = hack_weight;
		c->anim_weight[1] = 1-hack_weight;
	}

	for (i=0; i < NUM_CHAR_ANIM; ++i)
	{
		if (c->anim[i].set)
		{
			float afacing;
			vec astep;
			anim_sim(&c->anim[i], dt, (float *) &astep, &afacing);
			vec_addeq_scale(&step, &astep, c->anim_weight[i]);
			dfacing += afacing * c->anim_weight[i];
			dw += c->anim_weight[i];
		}
	}

	if (dw)
	{
		c->facing += dfacing/dw;
		vec_scaleeq(&step, 1.0/dw);
		vec_rotate_z(&move, &step, c->facing);
		vec_addeq(&c->loc, &move);
	}

	if (vec_mag(&c->loc) > 10)
		c->loc = vec_zero();

	//c->loc = vec_zero();

	c->is_cached = 0;
	c->vert_cached = 0;
}

float total_phase;
void mix_animations(Character *c, int a1, int a2)
{
	animstate_init(&c->anim[0], c->model->anim, a1, c->anim_data[a1].fps, &c->anim_data[a1].move.x, c->anim_data[a1].ang_z, c->anim_data[a1].loop, c->anim_data[a1].phase);
	animstate_init(&c->anim[1], c->model->anim, a2, c->anim_data[a2].fps, &c->anim_data[a2].move.x, c->anim_data[a2].ang_z, c->anim_data[a2].loop, c->anim_data[a2].phase);
	c->anim_weight[0] = 0.5;
	c->anim_weight[1] = 0.5;

#if 1
	anim_sync(&c->anim[1], &c->anim[0]);
#else
	c->anim[1].state.anim_scale = c->anim[1].state.length / c->anim[0].state.length;

	// set the initial phase; the stored data is in keyframes
	c->anim[0].state.anim_time = anim_data[a1].phase / anim_data[a1].fps;
	c->anim[1].state.anim_time = anim_data[a2].phase / anim_data[a2].fps;
	if (c->anim[0].state.anim_time < 0) c->anim[0].state.anim_time += c->anim[0].state.length;
	if (c->anim[1].state.anim_time < 0) c->anim[1].state.anim_time += c->anim[1].state.length;
	total_phase = 0;
#endif
}

void switch_anim(Character *c, int a)
{
	if (a < 0)
	{
		int cur, a2 = mixes[-a*2-1];
		anim_number = a;
		last = animcontrol_play(c->control, mixes[-a*2-2], 0.35, 0.5, 1);
		if (last >= 0)
		{
			cur = animcontrol_free_slot(c->control);
			if (cur >= 0)
			{
				animstate_init(&c->anim[cur], c->model->anim, a2, c->anim_data[a2].fps, &c->anim_data[a2].move.x, c->anim_data[a2].ang_z, c->anim_data[a2].loop, c->anim_data[a2].phase);
				animcontrol_add_to_group(c->control, cur, last, 0.5);
			}
		}
	}
	else
	{
		last = animcontrol_play(c->control, a, 0.25, 1, 1);
		anim_number = a;
	}
}

void switch_anim_old(Character *c, int a)
{
	if (a < 0)
	{
		anim_number = a;
		mix_animations(c,mixes[-a*2-2],mixes[-a*2-1]);
	}
	else
	{
		animstate_init(&c->anim[0], c->model->anim, a, c->anim_data[a].fps, &c->anim_data[a].move.x, c->anim_data[a].ang_z, c->anim_data[a].loop, c->anim_data[a].phase);
		c->anim_weight[0] = 1;
		anim_number = a;
		c->anim[1].set = 0;
	}
}

static int foo;
void sim_char(Character *c, float dt)
{
	int i;
	float dfacing, dw;
	vec step,move;

	dt *= anim_scale;
	step = vec_zero();
	dfacing = 0;
	dw = 0;

	animcontrol_sim(c->control, dt);


	for (i=0; i < NUM_CHAR_ANIM; ++i)
	{
		if (c->anim[i].state.anim_time > 10)
			++foo;
		if (c->anim[i].set)
		{
			float afacing;
			vec astep;
			anim_sim(&c->anim[i], dt, (float *) &astep, &afacing);
			vec_addeq_scale(&step, &astep, c->anim_weight[i]);
			dfacing += afacing * c->anim_weight[i];
			dw += c->anim_weight[i];
		}
	}

	if (dw)
	{
		c->facing += dfacing/dw;
		vec_scaleeq(&step, 1.0/dw);
		vec_rotate_z(&move, &step, c->facing);
		vec_addeq(&c->loc, &move);
	}

	if (vec_mag(&c->loc) > 10)
		c->loc = vec_zero();

	//c->loc = vec_zero();

	c->is_cached = 0;
	c->vert_cached = 0;

	if (c->control->time_left < 0.25)
		switch_anim(c, anim_number);
}

void character_compute_bones(STB3D_ModelTransform *bones, Character *c)
{
	int i,j,n=0;
	float dw=0;

	for (i=0; i < NUM_CHAR_ANIM; ++i)
		if (c->anim[i].set)
		{
			dw += c->anim_weight[i];
			++n;
		}

	if (dw && !rest_pose)
	{
		// @TODO: special case n=1
		int first=1;

		memset(bones, 0, sizeof(bones[0]) * c->model->num_bones);
		dw = 1.0/dw;

		for (i=0; i < NUM_CHAR_ANIM; ++i)
		{
			if (c->anim[i].set)
			{
				float w = dw * c->anim_weight[i];
				STB3D_ModelTransform abones[64];
				anim_get_bones(abones, &c->anim[i], STB3D_MODEL_interpolate_cubic);
				for (j=0; j < c->model->num_bones; ++j)
				{
#if NEIGHBORHOOD_WHEN_BLENDING
					if (!first)
					{
						quat *temp = (quat *) &bones[j].rot_quat;
						float *p = abones[j].rot_quat;
						if (temp->x*p[0] + temp->y*p[1] + temp->z*p[2] + temp->w*p[3] < 0)
						{
							abones[j].rot_quat[0] = -abones[j].rot_quat[0];
							abones[j].rot_quat[1] = -abones[j].rot_quat[1];
							abones[j].rot_quat[2] = -abones[j].rot_quat[2];
							abones[j].rot_quat[3] = -abones[j].rot_quat[3];
						}
					}
#endif
					quat_scale_addeq((quat *) &bones[j].rot_quat, (quat *) &abones[j].rot_quat, w);
					vec_addeq_scale((vec *) &bones[j].translate, (vec *) &abones[j].translate, w);
				}
				if (w)
					first = 0;
			}
		}

		for (j=0; j < c->model->num_bones; ++j)
			quat_normalize((quat *) &bones[j].rot_quat);

	}
	else
	{
		memset(bones, 0, sizeof(bones[0]) * c->model->num_bones);
		for (j=0; j < c->model->num_bones; ++j)
		{
			*(quat *) bones[j].rot_quat = *(quat *) c->model->bones[j].rest_quat;
			*(vec *)bones[j].translate = *(vec *)c->model->anim->anim[0][j].frame[0].rest_xform.translate;
		}
	}
}

void draw_character(Character *c, int shadow_pass)
{
	int i;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(ch.loc.x, ch.loc.y, ch.loc.z);
	glRotatef(180 + ch.facing * 180 / M_PI, 0,0,1);
	glMultMatrixf(roty[0]);

	if (!c->is_cached)
	{
		STB3D_ModelTransform bones[64];
		character_compute_bones(bones, c);
		model_convert_bonelocal_to_skin2world(c->bone_mat, c->model, bones);
		if (show_skel)
			model_convert_bonelocal_to_bone2world(c->bone_debug, c->model, bones);
		c->is_cached = 1;
	}

	if (!c->vert_cached)
	{
		if (c->model->streaming_vertex_buffer)
		{
			float *vert;
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, c->model->streaming_vertex_buffer);
			vert = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
			if (vert)
			{
				for (i=0; i < c->model->num_meshes; ++i)
				{
					compute_mesh_verts(vert, &c->model->meshes[i], c->bone_mat);
					vert += c->model->meshes[i].num_vertices * 6;
				}

				if (glUnmapBufferARB(GL_ARRAY_BUFFER_ARB))
					c->vert_cached = 1;
			}
		}
		else
		{
			float *vert;
			if (c->verts == NULL)
			{
				int n = 6 * num_verts(c->model);
				c->verts = malloc(n * sizeof(c->verts[0]));
			}
			vert = c->verts;
			for (i=0; i < c->model->num_meshes; ++i)
			{
				compute_mesh_verts(vert, &c->model->meshes[i], c->bone_mat);
				vert += c->model->meshes[i].num_vertices * 6;
			}

			c->vert_cached = 1;
		}
	}

	if (c->vert_cached)
	{
		float *vert = c->verts;
		int voff=0, ioff=0;
		if (c->model->streaming_vertex_buffer && shadow_pass)
		{
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, c->model->streaming_vertex_buffer);
			vert = 0;
		}
		for (i=0; i < c->model->num_meshes; ++i)
		{
			if (!shadow_pass)
				draw_smesh_prevert(&c->model->meshes[i], vert, c->model, voff, ioff);
			else
				draw_smesh_prevert_shadow(&c->model->meshes[i], vert);
			vert += c->model->meshes[i].num_vertices * 6;
			voff += c->model->meshes[i].num_vertices;
			ioff += c->model->meshes[i].num_vertex_indices;
		}
		if (glBindBufferARB)
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
	else
	{
		for (i=0; i < c->model->num_meshes; ++i)
			draw_smesh_blend_vert(&c->model->meshes[i], c->bone_mat);
	}

	glPopMatrix();
}

