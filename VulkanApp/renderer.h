#ifndef _RENDERER_H_
#define _RENDERER_H_

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
#include "buffer_visualisation_pipeline.h"
#include "g_buffer_pipeline.h"
#include "deferred_pipeline.h"



struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

class VulkanRenderer
{
public:
	enum class RenderMode
	{
		FORWARD,
		DEFERRED,
		BUFFER_VIS
	};

public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string vs_filename, std::string ps_filename);
	void InitPipeline();
	void RenderScene();
	void Cleanup();
	
	void RecreateSwapChainFeatures();

	void AddMesh(Mesh* mesh);
	void RemoveMesh(Mesh* mesh);
	void AddLight(Light* light);
	void RemoveLight(Light* light);
	void SetCamera(Camera* camera) { render_camera_ = camera; }

	VulkanShader* GetMaterialShader() { return material_shader_; }
	VulkanShader* GetShadowMapShader() { return shadow_map_shader_; }

	VulkanSwapChain* GetSwapChain() { return swap_chain_; }
	VulkanPrimitiveBuffer* GetPrimitiveBuffer() { return primitive_buffer_; }
	VulkanMaterialBuffer* GetMaterialBuffer() { return material_buffer_; }
	VkCommandPool GetCommandPool() { return command_pool_; }

	void GetMatrixBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory) { buffer = matrix_buffer_; buffer_memory = matrix_buffer_memory_; }
	void GetLightBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory) { buffer = light_buffer_; buffer_memory = light_buffer_memory_; }

	VkSemaphore GetSignalSemaphore() { return render_semaphore_; }
	Texture* GetDefaultTexture() { return default_texture_; }

	uint32_t AddTextureMap(Texture* texture, Texture::MapType map_type);

	inline void SetRenderMode(RenderMode mode) { render_mode_ = mode; }

	VulkanTextureCache*	GetTextureCache() { return texture_cache_; }

protected:
	void CreateBuffers();
	void CreateSemaphores();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateGBufferCommandBuffers();
	void CreateDeferredCommandBuffers();
	void CreateMaterialShader(std::string vs_filename, std::string ps_filename);
	void CreateShaders();
	void CreatePrimitiveBuffer();
	void CreateMaterialBuffer();
	void CreateLightBuffer();
	void RenderPass(uint32_t frame_index);
	void RenderVisualisation(uint32_t frame_index);
	void RenderGBuffer(uint32_t frame_index);
	void RenderDeferred(uint32_t frame_index);

	void InitGBufferPipeline();

protected:
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;
	VulkanPrimitiveBuffer* primitive_buffer_;
	VulkanMaterialBuffer* material_buffer_;
	VulkanTextureCache* texture_cache_;
	VulkanShader* material_shader_;
	VulkanShader* shadow_map_shader_;
	VulkanShader* buffer_visualisation_shader_;
	VulkanPipeline* rendering_pipeline_;
	BufferVisualisationPipeline* buffer_visualisation_pipeline_;
	
	// deferred shading components
	VulkanShader* g_buffer_shader_;
	VulkanShader* deferred_shader_;
	VkSampler g_buffer_sampler_;
	GBufferPipeline* g_buffer_pipeline_;
	DeferredPipeline* deferred_pipeline_;
	VulkanRenderTarget* g_buffer_;

	Camera* render_camera_;
	Texture* default_texture_;

	VkBuffer matrix_buffer_;
	VkDeviceMemory matrix_buffer_memory_;
	
	VkBuffer light_buffer_;
	VkDeviceMemory light_buffer_memory_;

	VkQueue graphics_queue_;
	VkCommandPool command_pool_;
	std::vector<VkCommandBuffer> command_buffers_;
	std::vector<VkCommandBuffer> buffer_visualisation_command_buffers_;
	std::vector<VkCommandBuffer> g_buffer_command_buffers_;
	std::vector<VkCommandBuffer> deferred_command_buffers_;

	VkSemaphore g_buffer_semaphore_;
	VkSemaphore render_semaphore_;
	
	std::vector<Mesh*> meshes_;
	std::vector<Light*> lights_;

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
};

#endif