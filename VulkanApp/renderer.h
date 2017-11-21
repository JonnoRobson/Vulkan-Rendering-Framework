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
#include "camera.h"


struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

class VulkanRenderer
{
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

	VulkanSwapChain* GetSwapChain() { return swap_chain_; }
	VulkanPrimitiveBuffer* GetPrimitiveBuffer() { return primitive_buffer_; }
	VulkanMaterialBuffer* GetMaterialBuffer() { return material_buffer_; }
	VkCommandPool GetCommandPool() { return command_pool_; }

	void GetMatrixBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory) { buffer = matrix_buffer_; buffer_memory = matrix_buffer_memory_; }
	void GetLightBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory) { buffer = light_buffer_; buffer_memory = light_buffer_memory_; }

	VkSemaphore GetSignalSemaphore() { return render_semaphore_; }
	Texture* GetDefaultTexture() { return default_texture_; }

	void AddDiffuseTexture(Texture* texture) 
	{
		diffuse_textures_.push_back(texture);
		texture->SetTextureIndex(diffuse_textures_.size());
	}

protected:
	void CreateBuffers();
	void CreateSemaphores();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateMaterialShader(std::string vs_filename, std::string ps_filename);
	void CreatePrimitiveBuffer();
	void CreateMaterialBuffer();
	void RenderPass(uint32_t frame_index);

protected:
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;
	VulkanPrimitiveBuffer* primitive_buffer_;
	VulkanMaterialBuffer* material_buffer_;
	VulkanShader* material_shader_;
	VulkanPipeline* rendering_pipeline_;

	Camera* render_camera_;
	Texture* default_texture_;

	VkBuffer matrix_buffer_;
	VkDeviceMemory matrix_buffer_memory_;
	
	VkBuffer light_buffer_;
	VkDeviceMemory light_buffer_memory_;

	VkQueue graphics_queue_;
	VkCommandPool command_pool_;
	std::vector<VkCommandBuffer> command_buffers_;

	VkSemaphore render_semaphore_;
	
	std::vector<Mesh*> meshes_;
	std::vector<Light*> lights_;
	std::vector<Texture*> diffuse_textures_;
};

#endif