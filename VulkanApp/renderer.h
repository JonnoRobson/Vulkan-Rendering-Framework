#ifndef _RENDERER_H_
#define _RENDERER_H_

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "device.h"
#include "swap_chain.h"
#include "shader.h"
#include "mesh.h"
#include "primitive_buffer.h"

class VulkanRenderer
{
public:
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string vs_filename, std::string ps_filename);
	void Cleanup();

	void PreRender();
	void RenderPass();
	void PostRender();

	void RecreateSwapChainFeatures();

	void AddMesh(Mesh* mesh);
	void RemoveMesh(Mesh* mesh);

	VulkanShader* GetShader(int index) { return shaders_[index]; }
	VkSemaphore GetSignalSemaphore() { return semaphores_[1]; }

protected:
	void CreateRenderPass();
	void CreateRenderPasses();
	void CreateFramebuffers();
	void CreatePipeline();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateBeginCommandBuffers();
	void CreateRenderCommandBuffers();
	void CreateFinishCommandBuffers();
	void CreateDescriptorPool();
	void CreateSemaphores();
	void CreateShaders();
	void CreatePrimitiveBuffer();

	VkShaderModule CreateShaderModule(const std::vector<char>& code);

protected:
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;
	VulkanPrimitiveBuffer* primitive_buffer_;

	VkRenderPass render_pass_;
	VkRenderPass clear_render_pass_;
	VkRenderPass draw_render_pass_;

	std::vector<VkFramebuffer> frame_buffers_;
	VkPipelineLayout pipeline_layout_;
	VkPipeline pipeline_;

	VkQueue graphics_queue_;
	VkCommandPool command_pool_;
	std::vector<VkCommandBuffer> begin_command_buffers_;
	std::vector<VkCommandBuffer> render_command_buffers_;
	std::vector<VkCommandBuffer> finish_command_buffers_;
	std::vector<VkSemaphore> semaphores_;
	
	std::vector<VulkanShader*> shaders_;
	std::vector<Mesh*> meshes_;
	VkDescriptorPool descriptor_pool_;
};

#endif