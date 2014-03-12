#ifndef STB_INCLUDE_STB_3DMODEL_H
#define STB_INCLUDE_STB_3DMODEL_H

enum
{
	STB3D_type_uchar=1,
	STB3D_type_ushort,
	STB3D_type_int,
	STB3D_type_float,
	STB3D_type_float2,
	STB3D_type_ushort2,
	STB3D_type_float3,
};

typedef struct
{
	unsigned char stride;

	unsigned char pos_type    , pos_off;
	unsigned char norm_type   , norm_off;
	unsigned char tc_count    , tc_type, tc_off;
	unsigned char bone_count  , bone_type, bone_off;
	unsigned char weight_count, weight_type, weight_off;

	unsigned char vertex_index_type;
} STB3D_VertexDescriptor;

typedef struct
{
	char name[256];
	float color[4];
	float spec_color[4];
	float emis_color[4];
	float emis_weight;
	char texture[256];
	unsigned int texid;
} STB3D_ModelMaterial;

#define STB3D_MAX_VERTEX_BONES 4
typedef struct
{
	float pos[3];
	float norm[3];
	float tc[2];
	unsigned short weight[STB3D_MAX_VERTEX_BONES];
	unsigned char bone[STB3D_MAX_VERTEX_BONES];
} STB3D_ModelVertex;

typedef struct
{
	STB3D_VertexDescriptor;
	STB3D_ModelMaterial *mat;
	STB3D_ModelVertex *vertices;
	unsigned short *vertex_index_list;  // 64K max
	int num_vertices;
	int num_vertex_indices;
	int frame;
} STB3D_ModelMesh;

typedef struct
{
	float rot_quat[4];
	float translate[3];
} STB3D_ModelTransform;

typedef struct
{
	int framenum; // not used now, but needs to be binary searched or something
	STB3D_ModelTransform rest_xform;
} STB3D_ModelTransformKeyframe;

enum
{
	STB3D_ANIMTYPE_keyframes_fixed_frame_rate,
};

typedef struct
{
	int type;
	int num_frames;
	STB3D_ModelTransformKeyframe *frame;
} STB3D_ModelBoneAnimation;

// bound to a particular model
typedef struct
{
	STB3D_ModelBoneAnimation **anim;  // anim[anum_number][bone]
	int num_anim;
	char **anim_names;
	int num_bones;
} STB3D_ModelAnimationSet;

typedef struct
{
	char name[64];
	float rest_xform[16], bind_xform[16];
	float rest_quat[4];
	float bbox[2][3];  // axis-aligned bbox in bone-space
	int bind;
	int parent;
} STB3D_ModelBone;

typedef struct
{
	int num_meshes;
	STB3D_ModelMesh *meshes;
} STB3D_ModelLOD;

typedef struct
{
	int num_meshes;
	int num_mats;
	int num_bones;
	int num_lod;
	STB3D_ModelMesh *meshes;
	STB3D_ModelLOD *lods;
	STB3D_ModelMaterial *materials;
	STB3D_ModelBone *bones;
	STB3D_ModelAnimationSet *anim;

	unsigned int static_vertex_buffer;
	unsigned int static_partial_vertex_buffer;
	unsigned int streaming_vertex_buffer;
	unsigned int static_index_buffer;
} STB3D_Model;

typedef struct STB3D__AnimationStateAbstract STB3D_AnimationStateAbstract;

struct STB3D__AnimationStateAbstract
{
	// animation properties
	float length;       // length in time - seconds if no scale
	float movement[3];  // movement spatial-units/second
	float rotation;     // rotation in radians/second
	int loop;           // does the animation loop?

	// synchronization parent
	STB3D_AnimationStateAbstract *sync;
	float sync_phase;   // relative phase from 'sync', expressed in scale of 'length'
	float sync_rate;    // precomputed conversion factor: length / sync->length

	// no-sync configuration data
	float anim_scale;   // time to scale

	// state data
	float anim_time;
};

typedef struct
{
	STB3D_AnimationStateAbstract state;
	STB3D_ModelAnimationSet *set;
	int anim_number;
	int num_frames;
	float anim_time;
	float fps;
	float phase;
} STB3D_AnimationState;

enum
{
	STB3D_MODEL_interpolate_cubic=0,
	STB3D_MODEL_interpolate_nearest,
	STB3D_MODEL_interpolate_linear,
};


extern void animabstract_init(STB3D_AnimationStateAbstract *as, float length, float move[3], float rot, int loop);
extern void animstate_init(STB3D_AnimationState *as, STB3D_ModelAnimationSet *set, int anim_number, float fps, float move[3], float rot, int loop, float phase);
extern void anim_sim(STB3D_AnimationState *as, float dt, float *dpos, float *dori);
extern void anim_get_bones(STB3D_ModelTransform *bones, STB3D_AnimationState *as, int interpolation_type);
extern void model_convert_bonelocal_to_bone2world(float bones[][16], STB3D_Model *model, STB3D_ModelTransform *raw_bones);
extern void model_convert_bonelocal_to_skin2world(float bones[][16], STB3D_Model *model, STB3D_ModelTransform *raw_bones);
extern void anim_sync(STB3D_AnimationState *as, STB3D_AnimationState *parent);
extern void model_compute_bone_bounds(STB3D_Model *model);

#endif
