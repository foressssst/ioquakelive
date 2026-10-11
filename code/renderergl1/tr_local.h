/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#ifndef TR_LOCAL_H
#define TR_LOCAL_H

#define USE_LEGACY_DLIGHTS
#define USE_PMLIGHT
#define MAX_REAL_DLIGHTS	(MAX_DLIGHTS*2)
#define MAX_LITSURFS		(MAX_DRAWSURFS)

#define MAX_TEXTURE_SIZE	2048

#define USE_VBO
#define USE_TESS_NEEDS_NORMAL
#define USE_FBO

#ifdef USE_VBO
#define USE_VBO_GRID
#endif

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "tr_common.h"
#include "iqm.h"
#include "qgl.h"

#define GL_INDEX_TYPE		GL_UNSIGNED_INT

typedef uint32_t glIndex_t;

#define	REFENTITYNUM_BITS	12
#define	REFENTITYNUM_MASK	((1<<REFENTITYNUM_BITS) - 1)
#define	MAX_REFENTITIES		((1<<REFENTITYNUM_BITS) - 1)
#define	REFENTITYNUM_WORLD	((1<<REFENTITYNUM_BITS) - 1)
#define SHADERNUM_BITS	14
#define MAX_SHADERS		(1<<SHADERNUM_BITS)
#define SHADERNUM_MASK	(MAX_SHADERS-1)

typedef struct dlight_s {
	vec3_t	origin;
	vec3_t	origin2;
	vec3_t	dir;

	vec3_t	color;
	float	radius;

	vec3_t	transformed;
	vec3_t	transformed2;
	int		additive;
	qboolean linear;
#ifdef USE_PMLIGHT
	struct litSurf_s	*head;
	struct litSurf_s	*tail;
#endif
} dlight_t;

typedef struct {
	refEntity_t	e;

	float		axisLength;
#ifdef USE_LEGACY_DLIGHTS
	int			needDlights;
#endif
	qboolean	lightingCalculated;
	vec3_t		lightDir;
	vec3_t		ambientLight;
	int			ambientLightInt;
	vec3_t		directedLight;
#ifdef USE_PMLIGHT
	vec3_t		shadowLightDir;
#endif
} trRefEntity_t;

typedef struct {
	vec3_t		origin;
	vec3_t		axis[3];
	vec3_t		viewOrigin;
	float		modelMatrix[16];
} orientationr_t;

typedef enum {
	SS_BAD,
	SS_PORTAL,
	SS_ENVIRONMENT,
	SS_OPAQUE,

	SS_DECAL,
	SS_SEE_THROUGH,
	SS_BANNER,

	SS_FOG,

	SS_UNDERWATER,

	SS_BLEND0,
	SS_BLEND1,
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST,

	SS_NEAREST
} shaderSort_t;

#define MAX_SHADER_STAGES 8

typedef enum {
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH, 
	GF_INVERSE_SAWTOOTH, 

	GF_NOISE

} genFunc_t;

typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7
} deform_t;

typedef enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST
} alphaGen_t;

typedef enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,
	CGEN_IDENTITY,
	CGEN_ENTITY,
	CGEN_ONE_MINUS_ENTITY,
	CGEN_EXACT_VERTEX,
	CGEN_VERTEX,
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,
	CGEN_LIGHTING_DIFFUSE,
	CGEN_FOG,
	CGEN_CONST
} colorGen_t;

typedef enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_ENVIRONMENT_MAPPED_FP,
	TCGEN_FOG,
	TCGEN_VECTOR
} texCoordGen_t;

typedef enum {
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
} acff_t;

typedef struct {
	float base;
	float amplitude;
	float phase;
	float frequency;

	genFunc_t	func;
} waveForm_t;

#define TR_MAX_TEXMODS 4

typedef enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE,
	TMOD_OFFSET,
	TMOD_SCALE_OFFSET,
	TMOD_OFFSET_SCALE,
} texMod_t;

#define	MAX_SHADER_DEFORMS	3
typedef struct {
	deform_t	deformation;

	vec3_t		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
} deformStage_t;

typedef struct {
	texMod_t		type;

	union {

		waveForm_t		wave;

		struct {
			float		matrix[2][2];
			float		translate[2];
		};

		struct {
			float		scale[2];
			float		offset[2];
		};

		float			scroll[2];
		float			rotateSpeed;

	};

} texModInfo_t;

#define MAX_IMAGE_ANIMATIONS		24
#define MAX_IMAGE_ANIMATIONS_QL	16

#define LIGHTMAP_INDEX_NONE			0
#define LIGHTMAP_INDEX_SHADER		1
#define LIGHTMAP_INDEX_OFFSET		2

typedef struct {
	image_t			*image[MAX_IMAGE_ANIMATIONS];
	int				numImageAnimations;
	double			imageAnimationSpeed;

	texCoordGen_t	tcGen;
	vec3_t			tcGenVectors[2];

	int				numTexMods;
	texModInfo_t	*texMods;

	int				videoMapHandle;
	int				lightmap;
	qboolean		isVideoMap;
	unsigned int 	isScreenMap : 1;
	unsigned int 	dlight : 1;
} textureBundle_t;

#define NUM_TEXTURE_BUNDLES 2

#define TESS_ST0	1<<0
#define TESS_ST1	1<<1
#define TESS_ENV0	1<<2
#define TESS_ENV1	1<<3

typedef struct {
	qboolean		active;

	textureBundle_t	bundle[NUM_TEXTURE_BUNDLES];

	waveForm_t		rgbWave;
	colorGen_t		rgbGen;

	waveForm_t		alphaWave;
	alphaGen_t		alphaGen;

	color4ub_t		constantColor;

	unsigned		stateBits;
	GLint			mtEnv;

	acff_t			adjustColorsForFog;

	qboolean		isDetail;
	qboolean		depthFragment;

	short			vboVPindex[3];
	short			vboFPindex[2];

	uint32_t		color_offset;
	uint32_t		tex_offset[2];

	uint32_t		tessFlags;

} shaderStage_t;

struct shaderCommands_s;

typedef enum {
	FP_NONE,
	FP_EQUAL,
	FP_LE
} fogPass_t;

typedef struct {
	float		cloudHeight;
	image_t		*outerbox[6], *innerbox[6];
} skyParms_t;

typedef struct {
	vec3_t	color;
	float	depthForOpaque;
} fogParms_t;

typedef struct shader_s {
	char		name[MAX_QPATH];
	int			lightmapSearchIndex;
	int			lightmapIndex;

	int			index;
	int			sortedIndex;

	float		sort;

	qboolean	defaultShader;

	qboolean	explicitlyDefined;

	int			surfaceFlags;
	int			contentFlags;

	qboolean	entityMergable;

	qboolean	isSky;
	skyParms_t	sky;
	fogParms_t	fogParms;

	float		portalRange;
	float		portalRangeR;

	qboolean	multitextureEnv;

	cullType_t	cullType;
	qboolean	polygonOffset;

	unsigned	noMipMaps:1;
	unsigned	noPicMip:1;
	unsigned	noLightScale:1;
	unsigned	noVLcollapse:1;

	fogPass_t	fogPass;

	qboolean	needsNormal;
	qboolean	needsST2;

	int			numDeforms;
	deformStage_t	deforms[MAX_SHADER_DEFORMS];

	int			numUnfoggedPasses;
	shaderStage_t	*stages[MAX_SHADER_STAGES];

#ifdef USE_PMLIGHT
	int			lightingStage;
	int			lightingBundle;
#endif
	qboolean	isStaticShader;
	int			svarsSize;
	int			iboOffset;
	int			vboOffset;
	int			normalOffset;
	int			numIndexes;
	int			numVertexes;
	int			curVertexes;
	int			curIndexes;

	int			hasScreenMap;

	void	(*optimalStageIteratorFunc)( void );

	double	clampTime;
	double	timeOffset;

	struct shader_s *remappedShader;

	struct	shader_s	*next;
} shader_t;

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];

	stereoFrame_t	stereoFrame;

	int			time;
	int			rdflags;

	byte		areamask[MAX_MAP_AREA_BYTES];
	qboolean	areamaskModified;

	double		floatTime;

	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	int			num_entities;
	trRefEntity_t	*entities;

	unsigned int num_dlights;
	struct dlight_s	*dlights;

	int			numPolys;
	struct srfPoly_s	*polys;

	int			numDrawSurfs;
	struct drawSurf_s	*drawSurfs;
#ifdef USE_PMLIGHT
	int			numLitSurfs;
	struct litSurf_s	*litSurfs;
#endif
} trRefdef_t;

typedef struct image_s {
	char		*imgName;
	char		*imgName2;
	struct image_s *next;
	int			width, height;
	int			uploadWidth;
	int			uploadHeight;
	imgFlags_t	flags;
	GLuint		texnum;

	int			frameUsed;

	GLint		internalFormat;
	int			TMU;

} image_t;

#define MAX_SKIN_SURFACES	256

typedef struct {
	char		name[MAX_QPATH];
	shader_t	*shader;
} skinSurface_t;

typedef struct skin_s {
	char		name[MAX_QPATH];
	int			numSurfaces;
	skinSurface_t	*surfaces;
} skin_t;

typedef struct {
	int			originalBrushNumber;
	vec3_t		bounds[2];

	color4ub_t	colorInt;
	vec4_t		color;
	float		tcScale;
	fogParms_t	parms;

	qboolean	hasSurface;
	float		surface[4];
} fog_t;

typedef struct {
	float		eyeT;
	qboolean	eyeOutside;
	vec4_t		fogDistanceVector;
	vec4_t		fogDepthVector;
	const float *fogColor;
} fogProgramParms_t;

typedef enum {
	PV_NONE = 0,
	PV_PORTAL,
	PV_MIRROR,
	PV_COUNT
} portalView_t;

typedef struct {
	orientationr_t	or;
	orientationr_t	world;
	vec3_t		pvsOrigin;
	portalView_t portalView;
	int			frameSceneNum;
	int			frameCount;
	cplane_t	portalPlane;
	int			viewportX, viewportY, viewportWidth, viewportHeight;
	int			scissorX, scissorY, scissorWidth, scissorHeight;
	float		fovX, fovY;
	float		projectionMatrix[16];
	cplane_t	frustum[5];
	vec3_t		visBounds[2];
	float		zFar;
	stereoFrame_t	stereoFrame;
#ifdef USE_PMLIGHT
	unsigned int num_dlights;
	struct dlight_s	*dlights;
#endif
} viewParms_t;

typedef enum {
	SF_BAD,
	SF_SKIP,
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MD3,
	SF_MDR,
	SF_IQM,
	SF_FLARE,
	SF_ENTITY,

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0x7fffffff
} surfaceType_t;

typedef struct drawSurf_s {
	unsigned int		sort;
	surfaceType_t		*surface;
} drawSurf_t;

#ifdef USE_PMLIGHT
typedef struct litSurf_s {
	unsigned int		sort;
	surfaceType_t		*surface;
	struct litSurf_s	*next;
} litSurf_t;
#endif

#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32
#define	MAX_GRID_SIZE		(128+1)

typedef struct srfPoly_s {
	surfaceType_t	surfaceType;
	qhandle_t		hShader;
	int				fogIndex;
	int				numVerts;
	polyVert_t		*verts;
} srfPoly_t;

typedef struct srfFlare_s {
	surfaceType_t	surfaceType;
	vec3_t			origin;
	vec3_t			normal;
	vec3_t			color;
} srfFlare_t;

typedef struct srfGridMesh_s {
	surfaceType_t	surfaceType;

	int				dlightBits;

	vec3_t			meshBounds[2];
	vec3_t			localOrigin;
	float			meshRadius;

	vec3_t			lodOrigin;
	float			lodRadius;
	int				lodFixed;
	int				lodStitched;

	int				vboItemIndex;
	int				vboExpectIndices;
	int				vboExpectVertices;

	int				width, height;
	float			*widthLodError;
	float			*heightLodError;
	drawVert_t		verts[1];
} srfGridMesh_t;

#define	VERTEXSIZE	8
typedef struct {
	surfaceType_t	surfaceType;
	cplane_t	plane;

#ifdef USE_LEGACY_DLIGHTS
	int			dlightBits;
#endif
	int			vboItemIndex;
	float		*normals;

	int			numPoints;
	int			numIndices;
	int			ofsIndices;
	float		points[1][VERTEXSIZE];
} srfSurfaceFace_t;

typedef struct {
	surfaceType_t	surfaceType;

#ifdef USE_LEGACY_DLIGHTS
	int				dlightBits;
#endif
	int				vboItemIndex;

	vec3_t			bounds[2];
	vec3_t			localOrigin;
	float			radius;

	int				numIndexes;
	int				*indexes;

	int				numVerts;
	drawVert_t		*verts;
} srfTriangles_t;

typedef struct {
	vec3_t translate;
	quat_t rotate;
	vec3_t scale;
} iqmTransform_t;

typedef struct {
	int		num_vertexes;
	int		num_triangles;
	int		num_frames;
	int		num_surfaces;
	int		num_joints;
	int		num_poses;
	struct srfIQModel_s	*surfaces;

	int		*triangles;

	float		*positions;
	float		*texcoords;
	float		*normals;
	float		*tangents;
	byte		*colors;
	int		*influences;

	byte		*influenceBlendIndexes;
	union {
		float	*f;
		byte	*b;
	} influenceBlendWeights;

	int		blendWeightsType;

	char		*jointNames;
	int		*jointParents;
	float		*bindJoints;
	float		*invBindJoints;
	iqmTransform_t	*poses;
	float		*bounds;
} iqmData_t;

typedef struct srfIQModel_s {
	surfaceType_t	surfaceType;
	char		name[MAX_QPATH];
	shader_t	*shader;
	iqmData_t	*data;
	int		first_vertex, num_vertexes;
	int		first_triangle, num_triangles;
	int		first_influence, num_influences;
} srfIQModel_t;

extern	void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

typedef struct msurface_s {
	int					viewCount;
	struct shader_s		*shader;
	int					fogIndex;
#ifdef USE_PMLIGHT
	int					vcVisible;
	int					lightCount;
#endif
	surfaceType_t		*data;
} msurface_t;

typedef struct mnode_s {
	int			contents;
	int			visframe;
	vec3_t		mins, maxs;
	struct mnode_s	*parent;

	cplane_t	*plane;
	struct mnode_s	*children[2];	

	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mnode_t;

typedef struct {
	vec3_t		bounds[2];
	msurface_t	*firstSurface;
	int			numSurfaces;
} bmodel_t;

typedef struct {
	int			cellId;
	bmodel_t	*bmodel;
	vec3_t		center;
	vec3_t		normal;
	vec3_t		rect[4];
} adSurface_t;

typedef struct {
	char		name[MAX_QPATH];
	char		baseName[MAX_QPATH];

	int			dataSize;

	int			numShaders;
	dshader_t	*shaders;

	bmodel_t	*bmodels;
	int			numBModels;

	int			numplanes;
	cplane_t	*planes;

	int			numnodes;
	int			numDecisionNodes;
	mnode_t		*nodes;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	int			numfogs;
	fog_t		*fogs;

	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	int			lightGridBounds[3];
	byte		*lightGridData;

	int			numClusters;
	int			clusterBytes;
	const byte	*vis;

	byte		*novis;

	char		*entityString;
	const char	*entityParsePoint;

	int			numAds;
	dadvertisement_t ads[MAX_MAP_ADVERTISEMENTS];
	shader_t	*adShaders[MAX_MAP_ADVERTISEMENTS];

	int			numAdSurfaces;
	adSurface_t	adSurfaces[MAX_MAP_ADVERTISEMENTS];
} world_t;

typedef enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_MESH,
	MOD_MDR,
	MOD_IQM
} modtype_t;

typedef struct model_s {
	char		name[MAX_QPATH];
	modtype_t	type;
	int			index;

	int			dataSize;
	bmodel_t	*bmodel;
	md3Header_t	*md3[MD3_MAX_LODS];
	void	*modelData;

	int			 numLods;
} model_t;

#define	MAX_MOD_KNOWN	1024

void		R_ModelInit (void);
model_t		*R_GetModelByHandle( qhandle_t hModel );
int			R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, 
					 float frac, const char *tagName );
void		R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs );

void		R_Modellist_f (void);

#define	MAX_DRAWIMAGES			2048
#define	MAX_SKINS				1024

#define	MAX_DRAWSURFS			0x20000
#define	DRAWSURF_MASK			(MAX_DRAWSURFS-1)

#define	DLIGHT_BITS 1
#define	DLIGHT_MASK ((1<<DLIGHT_BITS)-1)
#define	FOGNUM_BITS 5
#define	FOGNUM_MASK ((1<<FOGNUM_BITS)-1)

#define	QSORT_FOGNUM_SHIFT	DLIGHT_BITS
#define	QSORT_REFENTITYNUM_SHIFT (QSORT_FOGNUM_SHIFT + FOGNUM_BITS)
#define	QSORT_SHADERNUM_SHIFT	(QSORT_REFENTITYNUM_SHIFT+REFENTITYNUM_BITS)
#if (QSORT_SHADERNUM_SHIFT+SHADERNUM_BITS) > 32
	#error "Need to update sorting, too many bits."
#endif
#define QSORT_REFENTITYNUM_MASK (REFENTITYNUM_MASK << QSORT_REFENTITYNUM_SHIFT)

extern	int			gl_filter_min, gl_filter_max;

typedef struct {
	int		c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	int		c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
	int		c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
	int		c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;

	int		c_leafs;
	int		c_dlightSurfaces;
	int		c_dlightSurfacesCulled;
#ifdef USE_PMLIGHT
	int		c_light_cull_out;
	int		c_light_cull_in;
	int		c_lit_leafs;
	int		c_lit_surfs;
	int		c_lit_culls;
	int		c_lit_masks;
#endif
} frontEndCounters_t;

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)

typedef struct {
	GLuint		currenttextures[ MAX_TEXTURE_UNITS ];
	int			currenttmu;
	qboolean	finishCalled;
	GLint		texEnv[2];
	cullType_t	faceCulling;
	unsigned	glStateBits;
	unsigned	glClientStateBits[ MAX_TEXTURE_UNITS ];
	int			currentArray;
} glstate_t;

typedef struct glstatic_s {
	int windowWidth;
	int windowHeight;
	int captureWidth;
	int captureHeight;
	int initTime;
	qboolean deviceSupportsGamma;
} glstatic_t;

typedef struct {
	int		c_surfaces, c_shaders, c_vertexes, c_indexes, c_totalIndexes;
	float	c_overDraw;

	int		c_dlightVertexes;
	int		c_dlightIndexes;

	int		c_flareAdds;
	int		c_flareTests;
	int		c_flareRenders;

	int		msec;
#ifdef USE_PMLIGHT
	int		c_lit_batches;
	int		c_lit_vertices;
	int		c_lit_indices;
	int		c_lit_indices_latecull_in;
	int		c_lit_indices_latecull_out;
	int		c_lit_vertices_lateculltest;
#endif
} backEndCounters_t;

typedef struct videoFrameCommand_s {
	int					commandId;
	int					width;
	int					height;
	byte				*captureBuffer;
	byte				*encodeBuffer;
	qboolean			motionJpeg;
} videoFrameCommand_t;

enum {
	SCREENSHOT_TGA = 1<<0,
	SCREENSHOT_JPG = 1<<1,
	SCREENSHOT_BMP = 1<<2,
	SCREENSHOT_BMP_CLIPBOARD = 1<<3,
	SCREENSHOT_AVI = 1<<4,
	SCREENSHOT_LEVELSHOT = 1<<5
};

#define MAX_FRAME_FENCES 4

typedef struct {
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	orientationr_t	or;
	backEndCounters_t	pc;
	qboolean	isHyperspace;
	const trRefEntity_t *currentEntity;
	qboolean	skyRenderedThisView;

	qboolean	projection2D;
	color4ub_t	color2D;
	qboolean	doneBloom;
	qboolean	doneSurfaces;
	trRefEntity_t	entity2D;

	int		screenshotMask;
	char	screenshotTGA[ MAX_OSPATH ];
	char	screenshotJPG[ MAX_OSPATH ];
	char	screenshotBMP[ MAX_OSPATH ];
	qboolean screenShotTGAsilent;
	qboolean screenShotJPGsilent;
	qboolean screenShotBMPsilent;
	videoFrameCommand_t	vcmd;

	qboolean throttle;
	qboolean drawConsole;
	qboolean doneShadows;

	GLsync	frameFences[MAX_FRAME_FENCES];
	int		fenceIndex;

} backEndState_t;

typedef struct {
	qboolean				registered;
	qboolean				inited;

	int						visCount;
	int						frameCount;
	int						sceneCount;
	int						viewCount;
#ifdef USE_PMLIGHT
	int						lightCount;
#endif

	int						frameSceneNum;

	qboolean				worldMapLoaded;
	world_t					*world;

	const byte				*externalVisData;

	image_t					*defaultImage;
	image_t					*scratchImage[ MAX_VIDEO_HANDLES ];
	image_t					*fogImage;
	image_t					*dlightImage;
	image_t					*flareImage;
	image_t					*whiteImage;
	image_t					*identityLightImage;

	shader_t				*defaultShader;
	shader_t				*whiteShader;
	shader_t				*cinematicShader;
	shader_t				*shadowShader;
	shader_t				*projectionShadowShader;

	shader_t				*flareShader;
	shader_t				*sunShader;

	int						numLightmaps;
	image_t					**lightmaps;

	qboolean				mergeLightmaps;
	float					lightmapOffset[2];
	float					lightmapScale[2];
	int						lightmapMod;

	trRefEntity_t			*currentEntity;
	trRefEntity_t			worldEntity;
	int						currentEntityNum;
	int						shiftedEntityNum;
	model_t					*currentModel;

	viewParms_t				viewParms;

	float					identityLight;
	int						identityLightByte;
	qboolean				frameHasWorld;
	int						overbrightBits;

	orientationr_t			or;

	trRefdef_t				refdef;

	int						viewCluster;
#ifdef USE_PMLIGHT
	dlight_t				*light;
#endif
	vec3_t					sunLight;
	vec3_t					sunDirection;

	frontEndCounters_t		pc;
	int						frontEndMsec;

	model_t					*models[MAX_MOD_KNOWN];
	int						numModels;

	int						numImages;
	image_t					*images[MAX_DRAWIMAGES];

	int						numShaders;
	shader_t				*shaders[MAX_SHADERS];
	shader_t				*sortedShaders[MAX_SHADERS];

	int						numSkins;
	skin_t					*skins[MAX_SKINS];

	float					sinTable[FUNCTABLE_SIZE];
	float					squareTable[FUNCTABLE_SIZE];
	float					triangleTable[FUNCTABLE_SIZE];
	float					sawToothTable[FUNCTABLE_SIZE];
	float					inverseSawToothTable[FUNCTABLE_SIZE];
	float					fogTable[FOG_TABLE_SIZE];

	qboolean				mapLoading;
	qboolean				needScreenMap;

	qboolean				vertexLightingAllowed;

} trGlobals_t;

extern backEndState_t	backEnd;
extern trGlobals_t	tr;

extern int					gl_clamp_mode;

extern glstate_t	glState;

extern glstatic_t gls;

#ifdef USE_FBO
extern	qboolean			windowAdjusted;
extern	qboolean			superSampled;
#endif

extern cvar_t	*r_flareSize;
extern cvar_t	*r_flareFade;
extern cvar_t	*r_flareCoeff;

extern cvar_t	*r_railWidth;
extern cvar_t	*r_railCoreWidth;
extern cvar_t	*r_railSegmentLength;

extern cvar_t	*r_znear;
extern cvar_t	*r_zproj;
extern cvar_t	*r_stereoSeparation;

extern cvar_t	*r_lodbias;
extern cvar_t	*r_lodscale;

extern cvar_t	*r_teleporterFlash;

extern cvar_t	*r_fastsky;
extern cvar_t	*r_fastSkyColor;
extern cvar_t	*r_drawSkyFloor;
extern cvar_t	*r_neatsky;
extern cvar_t	*r_drawSun;
extern cvar_t	*r_dynamiclight;
extern cvar_t	*r_mergeLightmaps;
#ifdef USE_PMLIGHT
extern cvar_t	*r_dlightMode;
extern cvar_t	*r_dlightSpecPower;
extern cvar_t	*r_dlightSpecColor;
extern cvar_t	*r_dlightScale;
extern cvar_t	*r_dlightIntensity;
#endif
extern cvar_t	*r_dlightSaturation;
#ifdef USE_VBO
extern cvar_t	*r_vbo;
#endif
#ifdef USE_FBO
extern cvar_t	*r_fbo;
extern cvar_t	*r_hdr;
extern cvar_t	*r_bloom;
extern cvar_t	*r_bloom_threshold;
extern cvar_t	*r_bloom_threshold_mode;
extern cvar_t	*r_bloom_modulate;
extern cvar_t	*r_bloom_passes;
extern cvar_t	*r_bloom_blend_base;
extern cvar_t	*r_bloom_intensity;
extern cvar_t	*r_bloom_filter_size;
extern cvar_t	*r_bloom_reflection;

extern cvar_t	*r_renderWidth;
extern cvar_t	*r_renderHeight;
extern cvar_t	*r_renderScale;
#endif

extern cvar_t	*r_dlightBacks;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern  cvar_t	*r_detailTextures;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_facePlaneCull;
extern	cvar_t	*r_nocurves;
extern	cvar_t	*r_showcluster;

extern cvar_t	*r_gamma;

extern cvar_t	*r_bloomActive;
extern cvar_t	*r_bloomBlurFalloff;
extern cvar_t	*r_bloomBlurRadius;
extern cvar_t	*r_bloomBlurScale;
extern cvar_t	*r_bloomBrightThreshold;
extern cvar_t	*r_bloomIntensity;
extern cvar_t	*r_bloomPasses;
extern cvar_t	*r_bloomSaturation;
extern cvar_t	*r_bloomSceneIntensity;
extern cvar_t	*r_bloomSceneSaturation;
extern cvar_t	*r_colorCorrectActive;
extern cvar_t	*r_contrast;
extern cvar_t	*r_enableBloom;
extern cvar_t	*r_enableColorCorrect;
extern cvar_t	*r_enablePostProcess;
extern cvar_t	*r_floatingPointFBOs;
extern cvar_t	*r_postProcessActive;

extern	cvar_t	*r_nobind;
extern	cvar_t	*r_singleShader;
extern	cvar_t	*r_roundImagesDown;
extern	cvar_t	*r_colorMipLevels;
extern	cvar_t	*r_picmip;
extern	cvar_t	*r_picmipExplosions;
extern	cvar_t	*r_nomip;
extern	cvar_t	*r_finish;
extern	cvar_t	*r_textureMode;
extern	cvar_t	*r_offsetFactor;
extern	cvar_t	*r_offsetUnits;

extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_lightmap;
extern	cvar_t	*r_vertexLight;

extern	cvar_t	*r_showtris;
extern	cvar_t	*r_showsky;
extern	cvar_t	*r_shownormals;
extern	cvar_t	*r_clear;

extern	cvar_t	*r_shadows;
extern	cvar_t	*r_flares;

extern	cvar_t	*r_intensity;

extern	cvar_t	*r_lockpvs;
extern	cvar_t	*r_noportals;
extern	cvar_t	*r_portalOnly;

extern	cvar_t	*r_subdivisions;
extern	cvar_t	*r_lodCurveError;
extern	cvar_t	*r_skipBackEnd;

extern	cvar_t	*r_anaglyphMode;

extern	cvar_t	*r_greyscale;

extern	cvar_t	*r_ignoreGLErrors;

extern	cvar_t	*r_overBrightBits;
extern	cvar_t	*r_mapOverBrightBits;
extern	cvar_t	*r_mapOverBrightCap;
extern	cvar_t	*r_mapGreyScale;

extern	cvar_t	*r_debugSurface;
extern	cvar_t	*r_simpleMipMaps;

extern	cvar_t	*r_showImages;
extern	cvar_t	*r_defaultImage;
extern	cvar_t	*r_debugSort;

extern	cvar_t	*r_printShaders;

extern cvar_t	*r_marksOnTriangleMeshes;

void R_SwapBuffers( int );

void R_RenderView( const viewParms_t *parms );

void R_AddMD3Surfaces( trRefEntity_t *e );
void R_AddNullModelSurfaces( trRefEntity_t *e );
void R_AddBeamSurfaces( trRefEntity_t *e );
void R_AddRailSurfaces( trRefEntity_t *e, qboolean isUnderwater );
void R_AddLightningBoltSurfaces( trRefEntity_t *e );

void R_AddPolygonSurfaces( void );

void R_DecomposeSort( unsigned sort, int *entityNum, shader_t **shader, 
					 int *fogNum, int *dlightMap );

void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, int fogIndex, int dlightMap );
#ifdef USE_PMLIGHT
void R_DecomposeLitSort( unsigned sort, int *entityNum, shader_t **shader, int *fogNum );
void R_AddLitSurf( surfaceType_t *surface, shader_t *shader, int fogIndex );
#endif

#define	CULL_IN		0
#define	CULL_CLIP	1
#define	CULL_OUT	2

void R_LocalPointToWorld( const vec3_t local, vec3_t world );
int R_CullLocalBox( const vec3_t bounds[2] );
int R_CullPointAndRadius( const vec3_t origin, float radius );
int R_CullLocalPointAndRadius( const vec3_t origin, float radius );
int R_CullDlight( const dlight_t *dl );

void R_SetupProjection( viewParms_t *dest, float zProj, qboolean computeFrustum );
void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms, orientationr_t *or );

const float *GL_Ortho( const float left, const float right, const float bottom, const float top, const float znear, const float zfar );
void	GL_Bind( image_t *image );
void	GL_BindTexNum( GLuint texnum );
void	GL_SelectTexture( int unit );
void	GL_BindTexture( int unit, GLuint texnum );
void	GL_TextureMode( const char *string );
void	GL_CheckErrors( void );
void	GL_State( unsigned stateVector );
void	GL_ClientState( int unit, unsigned stateVector );
void	GL_TexEnv( GLint env );
void	GL_Cull( cullType_t cullType );

#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define GLS_SRCBLEND_BITS						0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define GLS_DSTBLEND_BITS						0x000000f0

#define GLS_BLEND_BITS							(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)

#define GLS_DEPTHMASK_TRUE						0x00000100

#define GLS_POLYMODE_LINE						0x00000200

#define GLS_DEPTHTEST_DISABLE					0x00000400
#define GLS_DEPTHFUNC_EQUAL						0x00000800

#define GLS_ATEST_GT_0							0x00001000
#define GLS_ATEST_LT_80							0x00002000
#define GLS_ATEST_GE_80							0x00003000
#define GLS_ATEST_BITS							0x00003000

#define GLS_DEFAULT								GLS_DEPTHMASK_TRUE

#define CLS_NONE								0x00000000
#define CLS_COLOR_ARRAY							0x00000001
#define CLS_TEXCOORD_ARRAY						0x00000002
#define CLS_NORMAL_ARRAY						0x00000004

void		RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty );
void		RE_UploadCinematic( int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty );

void		RE_BeginFrame( stereoFrame_t stereoFrame );
void		RE_BeginRegistration( glconfig_t *glconfig );
void		RE_LoadWorldMap( const char *mapname );
void		RE_SetWorldVisData( const byte *vis );
qhandle_t	RE_RegisterModel( const char *name );
qhandle_t	RE_RegisterSkin( const char *name );

qboolean	RE_GetEntityToken( char *buffer, int size );

model_t		*R_AllocModel( void );

void		R_Init( void );

void		R_SetColorMappings( void );
void		R_GammaCorrect( byte *buffer, int bufSize );
void		R_ColorShiftLightingBytes( const byte in[4], byte out[4], qboolean hasAlpha );

void	R_ImageList_f( void );
void	R_SkinList_f( void );

void	R_InitFogTable( void );
float	R_FogFactor( float s, float t );
void	R_InitImages( void );
void	R_DeleteTextures( void );
int		R_SumOfUsedImages( void );
void	R_InitSkins( void );
skin_t	*R_GetSkinByHandle( qhandle_t hSkin );

int R_ComputeLOD( trRefEntity_t *ent );

const void *RB_TakeVideoFrameCmd( const void *data );

float R_ClampDenorm( float v );

shader_t	*R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage );
shader_t	*R_GetShaderByHandle( qhandle_t hShader );
shader_t	*R_GetShaderByState( int index, long *cycleTime );
shader_t	*R_FindShaderByName( const char *name );
void		R_InitShaders( void );
void		R_ShaderList_f( void );
void		RE_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset);

void		RB_SurfaceGridEstimate( srfGridMesh_t *cv, int *numVertexes, int *numIndexes ); 

typedef struct stageVars
{
	color4ub_t	colors[SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
	vec2_t		*texcoordPtr[NUM_TEXTURE_BUNDLES];
} stageVars_t;

typedef struct shaderCommands_s 
{
#pragma pack(push,16)
	glIndex_t	indexes[SHADER_MAX_INDEXES] QALIGN(16);
	vec4_t		xyz[SHADER_MAX_VERTEXES*2] QALIGN(16);
	vec4_t		normal[SHADER_MAX_VERTEXES] QALIGN(16);
	vec2_t		texCoords[2][SHADER_MAX_VERTEXES] QALIGN(16);
	vec2_t		texCoords00[SHADER_MAX_VERTEXES] QALIGN(16);
	color4ub_t	vertexColors[SHADER_MAX_VERTEXES] QALIGN(16);
#ifdef USE_LEGACY_DLIGHTS
	int			vertexDlightBits[SHADER_MAX_VERTEXES] QALIGN(16);
#endif
	stageVars_t	svars QALIGN(16);

	color4ub_t	constantColor255[SHADER_MAX_VERTEXES] QALIGN(16);
#pragma pack(pop)

	surfaceType_t	surfType;
	int			vboIndex;
	qboolean	allowVBO;

	shader_t	*shader;
	double		shaderTime;
	int			fogNum;
#ifdef USE_LEGACY_DLIGHTS
	int			dlightBits;
#endif
	int			numIndexes;
	int			numVertexes;

#ifdef USE_PMLIGHT
	const dlight_t* light;
	qboolean	dlightPass;
	qboolean	dlightUpdateParams;
	cullType_t	cullType;
#endif

#ifdef USE_TESS_NEEDS_NORMAL
	int			needsNormal;
#endif
#ifdef USE_TESS_NEEDS_ST2
	int			needsST2;
#endif

	int			numPasses;
	shaderStage_t **xstages;

} shaderCommands_t;

extern	shaderCommands_t	tess;

void RB_BeginSurface( shader_t *shader, int fogNum );
void RB_EndSurface( void );
void RB_CheckOverflow( int verts, int indexes );
#define RB_CHECKOVERFLOW(v,i) RB_CheckOverflow(v,i)

void RB_StageIteratorGeneric( void );
void RB_StageIteratorSky( void );

void RB_AddQuadStamp( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color );
void RB_AddQuadStampExt( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2 );
void RB_AddQuadStamp2( float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color );

void RB_ShowImages( void );

void R_AddBrushModelSurfaces( trRefEntity_t *e );
void R_AddWorldSurfaces( void );
qboolean R_inPVS( const vec3_t p1, const vec3_t p2 );

void R_ClearFlares( void );

void RB_AddFlare( void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal );
void RB_AddDlightFlares( void );
void RB_RenderFlares( void );

void R_DlightBmodel( bmodel_t *bmodel );
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent );
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *or );
int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );

#ifdef USE_PMLIGHT
void ARB_SetupLightParams( void );
void ARB_LightingPass( void );
qboolean R_LightCullBounds( const dlight_t* dl, const vec3_t mins, const vec3_t maxs );
#endif

void R_BindAnimatedImage( const textureBundle_t *bundle );
void R_DrawElements( int numIndexes, const glIndex_t *indexes );
void R_ComputeColors( const shaderStage_t *pStage );
void R_ComputeTexCoords( const int b, const textureBundle_t *bundle );

void QGL_InitARB( void );
void QGL_DoneARB( void );
#ifdef USE_FBO
void QGL_InitFBO( void );
#endif
qboolean ARB_UpdatePrograms( void );

qboolean GL_ProgramAvailable( void );
void GL_ProgramDisable( void );
void GL_ProgramEnable( void );

#ifdef USE_FBO
extern qboolean		fboEnabled;
extern qboolean		blitMSfbo;

void FBO_BindMain( void );
void FBO_PostProcess( void );
void FBO_BlitMS( qboolean depthOnly );
void FBO_BlitSS( void );
qboolean FBO_Bloom( const float gamma, const float obScale, qboolean finalPass );
void FBO_CopyScreen( void );
GLuint FBO_ScreenTexture( void );
#endif

void RB_ShadowTessEnd( void );
void RB_ShadowFinish( void );
void RB_ProjectionShadowDeform( void );

void R_InitSkyTexCoords( float cloudLayerHeight );
void R_DrawSkyBox( const shaderCommands_t *shader );
void RB_DrawSun( float scale, shader_t *shader );

#define PATCH_STITCHING

srfGridMesh_t *R_SubdividePatchToGrid( int width, int height,
								drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] );
srfGridMesh_t *R_GridInsertColumn( srfGridMesh_t *grid, int column, int row, vec3_t point, float loderror );
srfGridMesh_t *R_GridInsertRow( srfGridMesh_t *grid, int row, int column, vec3_t point, float loderror );
void R_FreeSurfaceGridMesh( srfGridMesh_t *grid );

int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

void R_InitNextFrame( void );

void RE_ClearScene( void );
void RE_AddRefEntityToScene( const refEntity_t *ent );
void RE_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b );

void RE_RenderScene( const refdef_t *fd );

#define MC_BITS_X (16)
#define MC_BITS_Y (16)
#define MC_BITS_Z (16)
#define MC_BITS_VECT (16)

#define MC_SCALE_X (1.0f/64)
#define MC_SCALE_Y (1.0f/64)
#define MC_SCALE_Z (1.0f/64)

void MC_UnCompress(float mat[3][4],const unsigned char * comp);

void R_MDRAddAnimSurfaces( trRefEntity_t *ent );
void RB_MDRSurfaceAnim( mdrSurface_t *surface );
qboolean R_LoadIQM (model_t *mod, void *buffer, int filesize, const char *name );
void R_AddIQMSurfaces( trRefEntity_t *ent );
void RB_IQMSurfaceAnim( const surfaceType_t *surface );
int R_IQMLerpTag( orientation_t *tag, iqmData_t *data,
                  int startFrame, int endFrame,
                  float frac, const char *tagName );

void	R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst );
void	R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window );

void	RB_DeformTessGeometry( void );

void	RB_CalcEnvironmentTexCoords( float *dstTexCoords );
void	RB_CalcEnvironmentTexCoordsFP( float *dstTexCoords, qboolean screenMap );
void	RB_CalcFogTexCoords( float *dstTexCoords );
const fogProgramParms_t *RB_CalcFogProgramParms( void );
void	RB_CalcScrollTexCoords( const float scroll[2], float *srcTexCoords, float *dstTexCoords );
void	RB_CalcRotateTexCoords( float rotSpeed, float *srcTexCoords, float *dstTexCoords );
void	RB_CalcScaleTexCoords( const float scale[2], float *srcTexCoords, float *dstTexCoords );
void	RB_CalcTurbulentTexCoords( const waveForm_t *wf, float *srcTexCoords, float *dstTexCoords );
void	RB_CalcTransformTexCoords( const texModInfo_t *tmi, float *srcTexCoords, float *dstTexCoords );
void	RB_CalcModulateColorsByFog( unsigned char *dstColors );
void	RB_CalcModulateAlphasByFog( unsigned char *dstColors );
void	RB_CalcModulateRGBAsByFog( unsigned char *dstColors );
void	RB_CalcWaveAlpha( const waveForm_t *wf, unsigned char *dstColors );
void	RB_CalcWaveColor( const waveForm_t *wf, unsigned char *dstColors );
void	RB_CalcAlphaFromEntity( unsigned char *dstColors );
void	RB_CalcAlphaFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcStretchTexCoords( const waveForm_t *wf, float *srcTexCoords, float *dstTexCoords );
void	RB_CalcColorFromEntity( unsigned char *dstColors );
void	RB_CalcColorFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcSpecularAlpha( unsigned char *alphas );
void	RB_CalcDiffuseColor( unsigned char *colors );

void RB_ExecuteRenderCommands( const void *data );

#define	MAX_RENDER_COMMANDS	0x80000

typedef struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	int		used;
} renderCommandList_t;

typedef struct {
	int		commandId;
	float	color[4];
} setColorCommand_t;

typedef struct {
	int		commandId;
	int		buffer;
} drawBufferCommand_t;

typedef struct {
	int		commandId;
	image_t	*image;
	int		width;
	int		height;
	void	*data;
} subImageCommand_t;

typedef struct {
	int		commandId;
} swapBuffersCommand_t;

typedef struct {
	int		commandId;
} finishBloomCommand_t;

typedef struct {
	int		commandId;
	shader_t	*shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
} stretchPicCommand_t;

typedef struct {
	int		commandId;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	drawSurf_t *drawSurfs;
	int		numDrawSurfs;
} drawSurfsCommand_t;

typedef struct
{
	int commandId;

	GLboolean rgba[4];
} colorMaskCommand_t;

typedef struct
{
	int commandId;
} clearDepthCommand_t;

typedef struct
{
	int commandId;
	qboolean fullscreen;
	qboolean frontAndBack;
	qboolean colorMask;
} clearColorCommand_t;

typedef enum {
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
#ifdef USE_FBO
	RC_FINISHBLOOM,
#endif
	RC_COLORMASK,
	RC_CLEARDEPTH,
	RC_CLEARCOLOR,
	RC_BLOOM_SCENE,
	RC_BLOOM
} renderCommand_t;

typedef struct {
	int commandId;
} bloomCommand_t;

extern vec4_t g_fontColor2D;
void R_InitFonts( void );
void R_FontShutdown( void );
void R_FontList_f( void );
int RE_Font_Register( const char *name );
void RE_Font_DrawString( int x, int y, const char *text, int fontIndex, float scale, int limit, float *maxX, int flags );
void RE_TextBounds( const char *text, int start, int fontIndex, float scale, int limit, float *extents );
void RE_GetGlyphInfo( int fontIndex, int charValue, glyphInfo_t *out );
void RE_SetCompositionFont( int fontIndex, float scale );
void RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font );

void R_QLPostRegister( void );
void R_QLPostInit( void );
void R_QLPostShutdown( void );
void RE_BeginBloomScene( void );
void RE_ApplyBloom( void );
const void *RB_BloomScene( const void *data );
const void *RB_Bloom( const void *data );
qboolean RB_QLColorCorrect( GLuint dstFbo, int width, int height );
qboolean R_CaptureGammaCorrect( void );

void R_FSRRegister( void );
float R_FSRScale( void );
void R_FSRInit( void );
void R_FSRShutdown( void );
qboolean RB_FSRBlit( GLuint srcTexture, int srcWidth, int srcHeight, int x0, int y0, int x1, int y1 );
void RB_TakeLevelShot( void );
void *R_GetCommandBuffer( int bytes );

void FBO_ResolveMultisample( void );
extern qboolean textureNonPowerOfTwo;

extern cvar_t *r_maxQueuedFrames;
void RB_ClearFrameFences( void );

extern GLint fboInternalFormat;
extern GLint fboTextureFormat;
extern GLint fboTextureType;

void FBO_BindTarget( GLuint target, GLuint buffer );
GLuint FBO_MainTexture( void );
GLuint FBO_MainBuffer( void );
GLuint FBO_ScratchBuffer( void );
void FBO_CopyScratchToMain( void );

void RE_Get_Advertisements( int *num, float *verts, char shaders[][MAX_QPATH] );

#define	MAX_POLYS		8192
#define	MAX_POLYVERTS	32768

typedef struct {
	drawSurf_t	drawSurfs[MAX_DRAWSURFS];
#ifdef USE_PMLIGHT
	litSurf_t	litSurfs[MAX_LITSURFS];
	dlight_t	dlights[MAX_REAL_DLIGHTS];
#else
	dlight_t	dlights[MAX_DLIGHTS];
#endif

	trRefEntity_t	entities[MAX_REFENTITIES];
	srfPoly_t	*polys;
	polyVert_t	*polyVerts;
	renderCommandList_t	commands;
} backEndData_t;

extern	int		max_polys;
extern	int		max_polyverts;

extern	backEndData_t	*backEndData;

void RB_ExecuteRenderCommands( const void *data );
void RB_TakeScreenshot( int x, int y, int width, int height, const char *fileName );
void RB_TakeScreenshotJPEG( int x, int y, int width, int height, const char *fileName );
void RB_TakeScreenshotBMP( int x, int y, int width, int height, const char *fileName, int clipboard );

void R_IssuePendingRenderCommands( void );

void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs );

void RE_SetColor( const float *rgba );
void RE_StretchPic ( float x, float y, float w, float h, 
					  float s1, float t1, float s2, float t2, qhandle_t hShader );
void RE_BeginFrame( stereoFrame_t stereoFrame );
void RE_EndFrame( int *frontEndMsec, int *backEndMsec );
void RE_TakeVideoFrame( int width, int height,
		byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

void RE_FinishBloom( void );
void RE_ThrottleBackend( void );
qboolean RE_CanMinimize( void );
const glconfig_t *RE_GetConfig( void );
void RE_VertexLighting( qboolean allowed );

#define MAX_FILTER_SIZE 20
#define MIN_FILTER_SIZE 1
#define MAX_BLUR_PASSES MAX_TEXTURE_UNITS

void R_BloomScreen( void );

#define GLE( ret, name, ... ) extern ret ( APIENTRY * q##name )( __VA_ARGS__ );
	QGL_Core_PROCS;
	QGL_Ext_PROCS;
	QGL_ARB_PROGRAM_PROCS;
	QGL_VBO_PROCS;
	QGL_FBO_PROCS;
	QGL_FBO_OPT_PROCS;
	QGL_GLSL_PROCS;
	QGL_ARB_SYNC_PROCS;
#undef GLE

#ifdef USE_VBO
extern void RB_StageIteratorVBO( void );
extern void R_BuildWorldVBO( msurface_t *surf, int surfCount );

extern void VBO_PushData( int itemIndex, shaderCommands_t *input );
extern void VBO_UnBind( void );
extern int VBO_Active( void );

extern void VBO_Cleanup( void );
extern void VBO_QueueItem( int itemIndex );
extern void VBO_ClearQueue( void );
extern void VBO_Flush( void );
#endif

typedef enum {
	Vertex,
	Fragment
} programType;

typedef enum {
	DEFAULT_VERTEX,
	DEFAULT_FRAGMENT,

	PROGRAM_BASE,

	DUMMY_VERTEX = PROGRAM_BASE,

#ifdef USE_PMLIGHT
	DLIGHT_VERTEX,
	DLIGHT_VERTEX_FOG_IN,
	DLIGHT_VERTEX_FOG_OUT,

	DLIGHT_FRAGMENT,
	DLIGHT_FRAGMENT_FOG,

	DLIGHT_ABS_FRAGMENT,
	DLIGHT_ABS_FRAGMENT_FOG,

	DLIGHT_LINEAR_FRAGMENT,
	DLIGHT_LINEAR_FRAGMENT_FOG,

	DLIGHT_LINEAR_ABS_FRAGMENT,
	DLIGHT_LINEAR_ABS_FRAGMENT_FOG,
#endif
	SPRITE_FRAGMENT,
#ifdef USE_FBO
	GAMMA_FRAGMENT,
	BLOOM_EXTRACT_FRAGMENT,
	BLUR_FRAGMENT,
	BLUR2_FRAGMENT,
	BLENDX_FRAGMENT,
	BLEND2_FRAGMENT,
	BLEND2_GAMMA_FRAGMENT,
#endif
	PROGRAM_COUNT

} programNum;

extern const char *fogOutVPCode;
extern const char *fogInVPCode;

qboolean ARB_CompileProgram( programType ptype, const char *text, GLuint program );
void ARB_ProgramEnableExt( GLuint vertexProgram, GLuint fragmentProgram );

void QGL_SetRenderScale( qboolean verbose );

int R_GetLightmapCoords( const int lightmapIndex, float *x, float *y );

#endif
