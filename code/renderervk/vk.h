#pragma once

#include "../renderercommon/vulkan/vulkan.h"
#include "tr_common.h"

void		VKimp_Init( glconfig_t *config );
void		VKimp_Shutdown( qboolean unloadDLL );
qboolean	VKimp_Probe( char *reason, int reasonSize );
void		*VK_GetInstanceProcAddr( VkInstance instance, const char *name );
qboolean	VK_CreateSurface( VkInstance instance, VkSurfaceKHR *pSurface );

#define MAX_SWAPCHAIN_IMAGES 8
#define MIN_SWAPCHAIN_IMAGES_IMM 3
#define MIN_SWAPCHAIN_IMAGES_FIFO   3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 4
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES ((1024 + 128)*2)

#define VERTEX_BUFFER_SIZE     (4 * 1024 * 1024)
#define VERTEX_BUFFER_SIZE_HI  (8 * 1024 * 1024)

#define STAGING_BUFFER_SIZE    (2 * 1024 * 1024)
#define STAGING_BUFFER_SIZE_HI (24 * 1024 * 1024)

#define IMAGE_CHUNK_SIZE (32 * 1024 * 1024)
#define MAX_IMAGE_CHUNKS 56

#define NUM_COMMAND_BUFFERS 2

#define USE_REVERSED_DEPTH

#define VK_NUM_BLOOM_PASSES 4

#ifndef _DEBUG
#define USE_DEDICATED_ALLOCATION
#endif
#define MAX_ATTACHMENTS_IN_POOL (8+VK_NUM_BLOOM_PASSES*2)

#define VK_DESC_STORAGE      0
#define VK_DESC_UNIFORM      0
#define VK_DESC_TEXTURE0     1
#define VK_DESC_TEXTURE1     2
#define VK_DESC_TEXTURE2     3
#define VK_DESC_FOG_COLLAPSE 4
#define VK_DESC_COUNT        5

#define VK_DESC_TEXTURE_BASE VK_DESC_TEXTURE0
#define VK_DESC_FOG_ONLY     VK_DESC_TEXTURE1
#define VK_DESC_FOG_DLIGHT   VK_DESC_TEXTURE1

typedef enum {
	TYPE_COLOR_BLACK,
	TYPE_COLOR_WHITE,
	TYPE_COLOR_GREEN,
	TYPE_COLOR_RED,
	TYPE_FOG_ONLY,
	TYPE_DOT,

	TYPE_SIGNLE_TEXTURE_LIGHTING,
	TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR,

	TYPE_SIGNLE_TEXTURE_DF,

	TYPE_GENERIC_BEGIN,
	TYPE_SIGNLE_TEXTURE = TYPE_GENERIC_BEGIN,
	TYPE_SIGNLE_TEXTURE_ENV,

	TYPE_SIGNLE_TEXTURE_IDENTITY,
	TYPE_SIGNLE_TEXTURE_IDENTITY_ENV,

	TYPE_SIGNLE_TEXTURE_FIXED_COLOR,
	TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV,

	TYPE_SIGNLE_TEXTURE_ENT_COLOR,
	TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV,

	TYPE_MULTI_TEXTURE_ADD2_IDENTITY,
	TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV,

	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV,

	TYPE_MULTI_TEXTURE_MUL2,
	TYPE_MULTI_TEXTURE_MUL2_ENV,
	TYPE_MULTI_TEXTURE_ADD2_1_1,
	TYPE_MULTI_TEXTURE_ADD2_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD2,
	TYPE_MULTI_TEXTURE_ADD2_ENV,

	TYPE_MULTI_TEXTURE_MUL3,
	TYPE_MULTI_TEXTURE_MUL3_ENV,
	TYPE_MULTI_TEXTURE_ADD3_1_1,
	TYPE_MULTI_TEXTURE_ADD3_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD3,
	TYPE_MULTI_TEXTURE_ADD3_ENV,

	TYPE_BLEND2_ADD,
	TYPE_BLEND2_ADD_ENV,
	TYPE_BLEND2_MUL,
	TYPE_BLEND2_MUL_ENV,
	TYPE_BLEND2_ALPHA,
	TYPE_BLEND2_ALPHA_ENV,
	TYPE_BLEND2_ONE_MINUS_ALPHA,
	TYPE_BLEND2_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND2_MIX_ALPHA,
	TYPE_BLEND2_MIX_ALPHA_ENV,

	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND2_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_BLEND3_ADD,
	TYPE_BLEND3_ADD_ENV,
	TYPE_BLEND3_MUL,
	TYPE_BLEND3_MUL_ENV,
	TYPE_BLEND3_ALPHA,
	TYPE_BLEND3_ALPHA_ENV,
	TYPE_BLEND3_ONE_MINUS_ALPHA,
	TYPE_BLEND3_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND3_MIX_ALPHA,
	TYPE_BLEND3_MIX_ALPHA_ENV,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND3_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV

} Vk_Shader_Type;

typedef enum {
	SHADOW_DISABLED,
	SHADOW_EDGES,
	SHADOW_FS_QUAD,
} Vk_Shadow_Phase;

typedef enum {
	TRIANGLE_LIST = 0,
	TRIANGLE_STRIP,
	LINE_LIST,
	POINT_LIST
} Vk_Primitive_Topology;

typedef enum {
	DEPTH_RANGE_NORMAL,
	DEPTH_RANGE_ZERO,
	DEPTH_RANGE_ONE,
	DEPTH_RANGE_WEAPON,
	DEPTH_RANGE_COUNT
}  Vk_Depth_Range;

typedef struct {
	VkSamplerAddressMode address_mode;
	int gl_mag_filter;
	int gl_min_filter;
	qboolean max_lod_1_0;
	qboolean noAnisotropy;
} Vk_Sampler_Def;

typedef enum {
	RENDER_PASS_MAIN = 0,
	RENDER_PASS_SCREENMAP,
	RENDER_PASS_POST_BLOOM,
	RENDER_PASS_COUNT
} renderPass_t;

typedef struct {
	Vk_Shader_Type shader_type;
	unsigned int state_bits;
	cullType_t face_culling;
	qboolean polygon_offset;
	qboolean mirror;
	Vk_Shadow_Phase shadow_phase;
	Vk_Primitive_Topology primitives;
	int line_width;
	int fog_stage;
	int abs_light;
	int allow_discard;
	int acff;
	struct {
		byte rgb;
		byte alpha;
	} color;
} Vk_Pipeline_Def;

typedef struct VK_Pipeline {
	Vk_Pipeline_Def def;
	VkPipeline handle[ RENDER_PASS_COUNT ];
} VK_Pipeline_t;

typedef struct vkUniform_s {
	vec4_t eyePos;
	union {
		struct {
			vec4_t pos;
			vec4_t color;
			vec4_t vector;
		} light;
		struct {
			vec4_t color[3];
		} ent;
	};
	vec4_t fogDistanceVector;
	vec4_t fogDepthVector;
	vec4_t fogEyeT;
	vec4_t fogColor;
} vkUniform_t;

#define TESS_XYZ   (1)
#define TESS_RGBA0 (2)
#define TESS_RGBA1 (4)
#define TESS_RGBA2 (8)
#define TESS_ST0   (16)
#define TESS_ST1   (32)
#define TESS_ST2   (64)
#define TESS_NNN   (128)
#define TESS_VPOS  (256)
#define TESS_ENV   (512)
#define TESS_ENT0  (1024)
#define TESS_ENT1  (2048)
#define TESS_ENT2  (4096)

void vk_initialize( void );

void vk_init_descriptors( void );

void vk_shutdown( refShutdownCode_t code );

void vk_release_resources( void );

void vk_wait_idle( void );
void vk_queue_wait_idle( void );

void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, qboolean update );
void vk_update_descriptor_set( image_t *image, qboolean mipmap );
void vk_destroy_image_resources( VkImage *image, VkImageView *imageView );
void vk_update_attachment_descriptors( void );
void vk_destroy_samplers( void );

uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height );
void vk_create_pipelines( void );

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );
void vk_window_resized( void );
void vk_begin_frame( void );
void vk_end_frame( void );
void vk_present_frame( void );

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
qboolean vk_ql_bloom( void );
qboolean vk_bloom_wanted( void );
void vk_low_latency_input( void );
void vk_low_latency_flash( void );

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t*indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
void vk_draw_dot( uint32_t storage_offset );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height );
qboolean vk_bloom( void );

qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
void vk_update_mvp( const float *m );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset );
#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex );
#endif
void vk_reset_descriptor( int index );
void vk_update_descriptor( int index, VkDescriptorSet descriptor );
void vk_update_descriptor_offset( int index, uint32_t offset );

void vk_update_post_process_pipelines( void );

const char *vk_format_string( VkFormat format );

void VBO_PrepareQueues( void );
void VBO_RenderIBOItems( void );
void VBO_ClearQueue( void );

typedef struct vk_tess_s {
	VkCommandBuffer command_buffer;

	VkSemaphore image_acquired;
	uint32_t	swapchain_image_index;
	qboolean	swapchain_image_acquired;
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished2;
#endif
	VkFence rendering_finished_fence;
	qboolean waitForFence;

	VkBuffer vertex_buffer;
	byte *vertex_buffer_ptr;
	uint32_t vertex_buffer_offset;

	VkDescriptorSet uniform_descriptor;
	uint32_t		uniform_read_offset;
	VkDeviceSize	buf_offset[8];
	VkDeviceSize	vbo_offset[8];

	VkBuffer		curr_index_buffer;
	uint32_t		curr_index_offset;

	struct {
		uint32_t		start, end;
		VkDescriptorSet	current[5];
		uint32_t		offset[1];
	} descriptor_set;

	Vk_Depth_Range		depth_range;
	VkPipeline			last_pipeline;

	uint32_t num_indexes;

	VkRect2D scissor_rect;
} vk_tess_t;

typedef struct {
	VkPhysicalDevice physical_device;
	VkSurfaceFormatKHR base_format;
	VkSurfaceFormatKHR present_format;

	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	qboolean swapchain_images_inited[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];

	VkCommandPool command_pool;
#ifdef USE_UPLOAD_QUEUE
	VkCommandBuffer staging_command_buffer;
#endif

	VkDeviceMemory image_memory[ MAX_ATTACHMENTS_IN_POOL ];
	uint32_t image_memory_count;

	struct {
		VkRenderPass main;
		VkRenderPass screenmap;
		VkRenderPass gamma;
		VkRenderPass capture;
		VkRenderPass bloom_extract;
		VkRenderPass blur[VK_NUM_BLOOM_PASSES*2];
		VkRenderPass post_bloom;
	} render_pass;

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout_sampler;
	VkDescriptorSetLayout set_layout_uniform;
	VkDescriptorSetLayout set_layout_storage;

	VkPipelineLayout pipeline_layout;
	VkPipelineLayout pipeline_layout_storage;
	VkPipelineLayout pipeline_layout_post_process;
	VkPipelineLayout pipeline_layout_blend;

	VkDescriptorSet color_descriptor;
	VkDescriptorSet color_descriptor_nearest;

	VkImage color_image;
	VkImageView color_image_view;

	VkImage bloom_image[1+VK_NUM_BLOOM_PASSES*2];
	VkImageView bloom_image_view[1+VK_NUM_BLOOM_PASSES*2];

	VkDescriptorSet bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES*2];

	VkImage depth_image;
	VkImageView depth_image_view;

	VkImage msaa_image;
	VkImageView msaa_image_view;

	struct {
		VkDescriptorSet color_descriptor;
		VkImage color_image;
		VkImageView color_image_view;

		VkImage color_image_msaa;
		VkImageView color_image_view_msaa;

		VkImage depth_image;
		VkImageView depth_image_view;

	} screenMap;

	struct {
		VkImage image;
		VkImageView image_view;
	} capture;

	struct {
		VkFramebuffer blur[VK_NUM_BLOOM_PASSES*2];
		VkFramebuffer bloom_extract;
		VkFramebuffer main[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer gamma[MAX_SWAPCHAIN_IMAGES];
		VkFramebuffer screenmap;
		VkFramebuffer capture;
	} framebuffers;

#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished;
	VkSemaphore image_uploaded2;
	VkSemaphore image_uploaded;
#endif

	vk_tess_t tess[ NUM_COMMAND_BUFFERS ], *cmd;
	int cmd_index;

	struct {
		VkBuffer		buffer;
		byte			*buffer_ptr;
		VkDeviceMemory	memory;
		VkDescriptorSet	descriptor;
	} storage;

	uint32_t uniform_item_size;
	uint32_t uniform_alignment;
	uint32_t storage_alignment;

	struct {
		VkBuffer vertex_buffer;
		VkDeviceMemory	buffer_memory;
	} vbo;

	VkDeviceMemory geometry_buffer_memory;
	VkDeviceSize geometry_buffer_size;
	VkDeviceSize geometry_buffer_size_new;

	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	struct {
		struct {
			VkShaderModule gen[3][2][2][2];
			VkShaderModule ident1[2][2][2];
			VkShaderModule fixed[2][2][2];
			VkShaderModule light[2];
		} vert;
		struct {
			VkShaderModule gen0_df;
			VkShaderModule gen[3][2][2];
			VkShaderModule ident1[2][2];
			VkShaderModule fixed[2][2];
			VkShaderModule ent[1][2];
			VkShaderModule light[2][2];
		} frag;

		VkShaderModule color_fs;
		VkShaderModule color_vs;

		VkShaderModule bloom_fs;
		VkShaderModule blur_fs;
		VkShaderModule blend_fs;
		VkShaderModule ql_bright_fs;
		VkShaderModule ql_blur_fs;
		VkShaderModule ql_bloom_fs;
		VkShaderModule ql_combine_fs;
		VkShaderModule ql_copy_fs;

		VkShaderModule gamma_fs;
		VkShaderModule gamma_vs;

		VkShaderModule fog_fs;
		VkShaderModule fog_vs;

		VkShaderModule dot_fs;
		VkShaderModule dot_vs;
	} modules;

	VkPipelineCache pipelineCache;

	VK_Pipeline_t pipelines[ MAX_VK_PIPELINES ];
	uint32_t pipelines_count;
	uint32_t pipelines_world_base;

	int32_t pipeline_create_count;

	uint32_t skybox_pipeline;

	uint32_t shadow_volume_pipelines[2][2];
	uint32_t shadow_finish_pipeline;

	uint32_t fog_pipelines[2][3][2];

#ifdef USE_LEGACY_DLIGHTS
	uint32_t dlight_pipelines[2][3][2];
#endif

#ifdef USE_PMLIGHT
	uint32_t dlight_pipelines_x[3][2][2][2];
	uint32_t dlight1_pipelines_x[3][2][2][2];
#endif

	uint32_t tris_debug_pipeline;
	uint32_t tris_mirror_debug_pipeline;
	uint32_t tris_debug_green_pipeline;
	uint32_t tris_mirror_debug_green_pipeline;
	uint32_t tris_debug_red_pipeline;
	uint32_t tris_mirror_debug_red_pipeline;

	uint32_t normals_debug_pipeline;
	uint32_t surface_debug_pipeline_solid;
	uint32_t surface_debug_pipeline_outline;
	uint32_t images_debug_pipeline;
	uint32_t images_debug_pipeline2;
	uint32_t surface_beam_pipeline;
	uint32_t surface_axis_pipeline;
	uint32_t dot_pipeline;

	VkPipeline gamma_pipeline;
	VkPipeline gamma_neutral_pipeline;
	VkPipeline capture_pipeline;
	VkPipeline capture_neutral_pipeline;
	VkPipeline bloom_extract_pipeline;
	VkPipeline blur_pipeline[VK_NUM_BLOOM_PASSES*2];
	VkPipeline ql_bright_pipeline;
	VkPipeline ql_bloom_pipeline;
	VkPipeline ql_combine_pipeline;
	VkPipeline ql_copy_pipeline;
	VkPipeline bloom_blend_pipeline;

	uint32_t frame_count;
	qboolean active;
	qboolean wideLines;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean dedicatedAllocation;
	qboolean debugMarkers;

	qboolean antiLag;
	qboolean lowLatency2;
	uint64_t latencyFrame;
	VkSemaphore latencySemaphore;
	uint64_t latencyValue;
	int latencyModeSet;
	int latencyMaxFps;

	float maxAnisotropy;
	float maxLod;

	VkFormat color_format;
	VkFormat capture_format;
	VkFormat depth_format;
	VkFormat bloom_format;

	VkImageLayout initSwapchainLayout;

	qboolean clearAttachment;
	qboolean fboActive;
	qboolean blitEnabled;
	qboolean msaaActive;

	qboolean offscreenRender;

	qboolean windowAdjusted;
	int		blitX0;
	int		blitY0;
	int		blitFilter;

	uint32_t renderWidth;
	uint32_t renderHeight;

	float renderScaleX;
	float renderScaleY;

	renderPass_t renderPassIndex;

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t image_chunk_size;

	uint32_t maxBoundDescriptorSets;

#ifdef USE_UPLOAD_QUEUE
	VkFence aux_fence;
	qboolean aux_fence_wait;
#endif

	struct staging_buffer_s {
		VkBuffer handle;
		VkDeviceMemory memory;
		VkDeviceSize size;
		byte *ptr;
#ifdef USE_UPLOAD_QUEUE
		VkDeviceSize offset;
#endif
	} staging_buffer;

	struct samplers_s {
		int count;
		Vk_Sampler_Def def[MAX_VK_SAMPLERS];
		VkSampler handle[MAX_VK_SAMPLERS];
		int filter_min;
		int filter_max;
	} samplers;

	struct defaults_t {
		VkDeviceSize staging_size;
		VkDeviceSize geometry_size;
	} defaults;

	char driverNote[200];

} Vk_Instance;

typedef struct {
	VkDeviceMemory memory;
	VkDeviceSize used;
} ImageChunk;

typedef struct {
	int num_image_chunks;
	ImageChunk image_chunks[MAX_IMAGE_CHUNKS];

	int dirty_depth_attachment;

	float modelview_transform[16];
} Vk_World;

extern Vk_Instance	vk;
extern Vk_World		vk_world;
