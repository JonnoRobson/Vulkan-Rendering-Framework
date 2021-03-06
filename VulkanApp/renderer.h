#ifndef _RENDERER_H_
#define _RENDERER_H_

#define PERFORMANCE_CAPTURES 10

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "device.h"
#include "swap_chain.h"
#include "shader.h"
#include "mesh.h"
#include "light.h"
#include "primitive_buffer.h"
#include "material_buffer.h"
#include "texture_cache.h"
#include "camera.h"
#include "compute_shader.h"
#include "buffer_visualisation_pipeline.h"
#include "g_buffer_pipeline.h"
#include "deferred_compute_pipeline.h"
#include "deferred_pipeline.h"
#include "weighted_blended_transparency_pipeline.h"
#include "transparency_composite_pipeline.h"
#include "visibility_pipeline.h"
#include "visibility_deferred_pipeline.h"
#include "visibility_front_peel_pipeline.h"
#include "visibility_peel_deferred_pipeline.h"
#include "shape_culling_pipeline.h"
#include "HDR.h"
#include "skybox.h"

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct SampleCountData
{
	VkSampleCountFlagBits sample_count;
	std::string deferred_shader;
	std::string visibility_deferred_shader;
	std::string visibility_peel_deferred_shader;
	std::string transparency_composite_shader;
};

struct PerformanceCapturePoint
{
	glm::vec3 capture_position;
	glm::vec3 capture_rotation;
};

static std::map<int, SampleCountData> multisample_data =
{
	{ 1, { VK_SAMPLE_COUNT_1_BIT, "deferred.frag.spv", "visibility_deferred.frag.spv", "visibility_peel_deferred.frag.spv", "transparency_composite.frag.spv" }},
	{ 2, { VK_SAMPLE_COUNT_2_BIT, "deferred_msaa_02.frag.spv", "visibility_deferred_msaa_02.frag.spv", "visibility_peel_deferred_msaa_02.frag.spv", "transparency_composite_msaa_02.frag.spv" }},
	{ 4, { VK_SAMPLE_COUNT_4_BIT, "deferred_msaa_04.frag.spv", "visibility_deferred_msaa_04.frag.spv", "visibility_peel_deferred_msaa_04.frag.spv" , "transparency_composite_msaa_04.frag.spv" }},
	{ 8, { VK_SAMPLE_COUNT_8_BIT, "deferred_msaa_08.frag.spv", "visibility_deferred_msaa_08.frag.spv", "visibility_peel_deferred_msaa_08.frag.spv", "transparency_composite_msaa_08.frag.spv" }},
	{ 16, { VK_SAMPLE_COUNT_16_BIT, "deferred_msaa_16.frag.spv", "visibility_deferred_msaa_16.frag.spv", "visibility_peel_deferred_msaa_16.frag.spv", "transparency_composite_msaa_16.frag.spv" }}
};

class VulkanRenderer
{
public:
	enum class RenderMode
	{
		FORWARD,
		DEFERRED,
		DEFERRED_COMPUTE,
		VISIBILITY,
		VISIBILITY_PEELED,
		BUFFER_VIS
	};

public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, int multisample_level = 1);
	void InitPipelines();
	void RenderScene();
	void Cleanup();
	
	void RecreateSwapChainFeatures();

	void AddMesh(Mesh* mesh);
	void RemoveMesh(Mesh* mesh);
	void AddLight(Light* light);
	void RemoveLight(Light* light);
	void SetCamera(Camera* camera) { render_camera_ = camera; }

	inline VulkanShader* GetMaterialShader() { return material_shader_; }
	inline VulkanShader* GetShadowMapShader() { return shadow_map_shader_; }

	inline VulkanSwapChain* GetSwapChain() { return swap_chain_; }
	inline VulkanPrimitiveBuffer* GetPrimitiveBuffer() { return primitive_buffer_; }
	inline VulkanMaterialBuffer* GetMaterialBuffer() { return material_buffer_; }
	inline VkCommandPool GetCommandPool() { return command_pool_; }
	inline std::vector<Mesh*> GetMeshes() { return meshes_; }

	void GetMatrixBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory) { buffer = matrix_buffer_; buffer_memory = matrix_buffer_memory_; }
	void GetLightBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory) { buffer = light_buffer_; buffer_memory = light_buffer_memory_; }
	void GetSceneMinMax(glm::vec3& scene_min, glm::vec3& scene_max);

	inline VkSemaphore GetSignalSemaphore() { return current_signal_semaphore_; }
	inline Texture* GetDefaultTexture() { return default_texture_; }

	uint32_t AddTextureMap(Texture* texture, Texture::MapType map_type);

	inline void SetRenderMode(RenderMode mode) { render_mode_ = mode; }

	inline void SetTextureDirectory(std::string dir) { texture_directory_ = dir; }
	inline std::string GetTextureDirectory() { return texture_directory_; }

	inline VulkanTextureCache*	GetTextureCache() { return texture_cache_; }
	inline HDR* GetHDR() { return hdr_; }

	void StartPerformanceCapture();
	void LoadCapturePoints(std::string filename);

protected:
	// pipeline creation functions
	void InitForwardPipeline();
	void InitDeferredPipeline();
	void InitDeferredComputePipeline();
	void InitVisibilityPipeline();
	void InitVisibilityPeelPipeline();
	void InitTransparencyPipeline();

	// cleanup functions
	void CleanupForwardPipeline();
	void CleanupDeferredPipeline();
	void CleanupVisibilityPipeline();
	void CleanupVisibilityPeelPipeline();
	void CleanupTransparencyPipeline();

	// command buffer creation functions
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateForwardCommandBuffers();
	void CreateGBufferCommandBuffers();
	void CreateDeferredCommandBuffers();
	void CreateDeferredComputeCommandBuffers();
	void CreateTransparencyCommandBuffer();
	void CreateTransparencyCompositeCommandBuffer();
	void CreateVisibilityCommandBuffer();
	void CreateVisibilityDeferredCommandBuffer();
	void CreateVisibilityPeelCommandBuffers();
	void CreateVisibilityPeelDeferredCommandBuffers();
	void CreateBufferVisualisationCommandBuffers();
	void CreateCullingCommandBuffer();

	// resource creation functions
	void CreateBuffers();
	void CreateSemaphores();
	void CreateShaders();
	void CreatePrimitiveBuffer();
	void CreateMaterialBuffer();
	void CreateLightBuffer();

	// rendering functions
	void RenderForward(uint32_t frame_index);
	void RenderVisualisation(uint32_t frame_index);
	void RenderGBuffer();
	void RenderDeferred();
	void RenderDeferredCompute();
	void RenderVisibility();
	void RenderVisbilityDeferred();
	void RenderVisibilityPeel();
	void RenderVisibilityPeelDeferred();
	void RenderTransparency();
	void CullGeometry();

	// performance recording functions
	void RecordPerformance();

protected:
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;
	VulkanPrimitiveBuffer* primitive_buffer_;
	VulkanMaterialBuffer* material_buffer_;
	VulkanTextureCache* texture_cache_;
	int multisample_level_;

	VulkanShader* material_shader_;
	VulkanShader* shadow_map_shader_;
	VulkanShader* buffer_visualisation_shader_;
	VulkanComputeShader* shape_culling_shader_;
	VulkanPipeline* rendering_pipeline_;
	ShapeCullingPipeline* shape_culling_pipeline_;
	BufferVisualisationPipeline* buffer_visualisation_pipeline_;
	VkSampler buffer_unnormalized_sampler_, buffer_normalized_sampler_, shadow_map_sampler_;
	
	// deferred shading components
	VulkanShader *g_buffer_shader_, *deferred_shader_;
	VulkanComputeShader* deferred_compute_shader_;
	GBufferPipeline* g_buffer_pipeline_;
	DeferredPipeline* deferred_pipeline_;
	DeferredComputePipeline* deferred_compute_pipeline_;
	VulkanRenderTarget* g_buffer_;
	std::vector<VkCommandBuffer> g_buffer_command_buffers_;
	VkCommandBuffer deferred_command_buffer_;
	VkCommandBuffer deferred_compute_command_buffer_;

	// visibility buffer shading components
	VulkanShader *visibility_shader_;
	VulkanShader *visibility_deferred_shader_;
	VulkanRenderTarget* visibility_buffer_;
	VisibilityPipeline* visibility_pipeline_;
	VisibilityDeferredPipeline* visibility_deferred_pipeline_;
	VkCommandBuffer visibility_command_buffer_;
	VkCommandBuffer visibility_deferred_command_buffer_;

	// visibility peeled shading components
	VulkanShader *visibility_peel_shader_, *visibility_peel_deferred_shader_;
	VulkanRenderTarget *visibility_peel_buffer_, *peel_depth_buffer_;
	VisibilityPeelDeferredPipeline* visibility_peel_deferred_pipeline_;
	VkCommandBuffer visibility_peel_deferred_command_buffer_;
	std::vector<VisibilityFrontPeelPipeline*> visibility_peel_pipelines_;
	std::vector<VkCommandBuffer> visibility_peel_command_buffers_;

	// transparency shading components
	VulkanShader *transparency_shader_, *transparency_composite_shader_;
	WeightedBlendedTransparencyPipeline* transparency_pipeline_;
	TransparencyCompositePipeline* transparency_composite_pipeline_;
	VulkanRenderTarget *accumulation_buffer_, *revealage_buffer_;
	VkCommandBuffer transparency_command_buffer_, transparency_composite_command_buffer_;
	VkSemaphore transparency_semaphore_, transparency_composite_semaphore_;

	// buffers
	VkBuffer matrix_buffer_, light_buffer_, visibility_data_buffer_;
	VkDeviceMemory matrix_buffer_memory_, light_buffer_memory_, visibility_data_buffer_memory_;
	HDR* hdr_;
	Skybox* skybox_;

	Camera* render_camera_;
	Texture* default_texture_;
	
	VkQueue graphics_queue_;
	VkQueue compute_queue_;
	VkCommandPool command_pool_;
	std::vector<VkCommandBuffer> command_buffers_;
	std::vector<VkCommandBuffer> buffer_visualisation_command_buffers_;
	VkCommandBuffer shape_culling_command_buffer_;

	VkSemaphore g_buffer_semaphore_;
	VkSemaphore render_semaphore_;
	VkSemaphore current_signal_semaphore_;

	std::vector<Mesh*> meshes_;
	std::vector<Light*> lights_;
	std::vector<VkImageView> shadow_maps_;
	std::string texture_directory_;

	RenderMode render_mode_;

	// texture maps
	std::vector<Texture*> ambient_textures_;
	std::vector<Texture*> diffuse_textures_;
	std::vector<Texture*> specular_textures_;
	std::vector<Texture*> specular_highlight_textures_;
	std::vector<Texture*> emissive_textures_;
	std::vector<Texture*> normal_textures_;
	std::vector<Texture*> alpha_textures_;
	std::vector<Texture*> reflection_textures_;

	//  rendering stage timing
	int performance_captures_remaining_;
	int current_capture_point_;
	double visibility_time_;
	double shading_time_;
	double transparency_time_;
	double post_process_time_;
	std::string model_filename_;
	std::vector<PerformanceCapturePoint> capture_points_;
};

#endif