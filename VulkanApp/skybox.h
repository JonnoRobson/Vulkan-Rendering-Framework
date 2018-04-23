#ifndef _SKYBOX_PIPELINE_H_
#define _SKYBOX_PIPELINE_H_

#include "pipeline.h"
#include "mesh.h"
#include "camera.h"

/**
* Vulkan graphics pipeline for rendering a skybox
*/
class SkyboxPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();
};

/**
* Stores and handles all resources for rendering a skybox
*/
class Skybox
{
public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VkCommandPool command_pool);
	void Cleanup();
	void Render(Camera* camera);

	inline VkSemaphore GetRenderSemaphore() { return render_semaphore_; }

protected:
	void InitPipeline(VulkanDevices* devices, VulkanSwapChain* swap_chain);
	void InitCommandBuffer(VkCommandPool command_pool);
	void InitResources();

protected:
	VulkanDevices* devices_;

	VulkanShader* skybox_shader_;
	SkyboxPipeline* skybox_pipeline_;
	Mesh* skybox_mesh_;
	Texture* skybox_texture_;
	VkCommandBuffer skybox_command_buffer_;
	VkSemaphore render_semaphore_;
	VkBuffer matrix_buffer_;
	VkDeviceMemory matrix_buffer_memory_;

};

#endif