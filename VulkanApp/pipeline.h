#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include "device.h"
#include "swap_chain.h"
#include "primitive_buffer.h"
#include "texture.h"
#include "shader.h"

class VulkanPipeline
{
protected:
	struct Descriptor
	{
		std::vector<VkDescriptorBufferInfo> buffer_infos;
		std::vector<VkDescriptorImageInfo> image_infos;
		VkDescriptorSetLayoutBinding layout_binding;
	};

public:
	VulkanPipeline();
	
	void Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, VulkanPrimitiveBuffer* primitive_buffer);
	void CleanUp();
	void RecreateSwapChainFeatures();

	inline void SetShader(VulkanShader* shader) { shader_ = shader; }

	void AddTexture(VkShaderStageFlags stage_flags, uint32_t binding_location, Texture* texture);
	void AddTextureArray(VkShaderStageFlags stage_flags, uint32_t binding_location, std::vector<Texture*>& textures);
	void AddSampler(VkShaderStageFlags stage_flags, uint32_t binding_location, VkSampler sampler);
	void AddUniformBuffer(VkShaderStageFlags stage_flags, uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size);
	void AddStorageBuffer(VkShaderStageFlags stage_flags, uint32_t binding_location, VkBuffer buffer, VkDeviceSize buffer_size);

	void RecordRenderCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:

	void CreateDescriptorSet();
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();

protected:

	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;
	VulkanPrimitiveBuffer* primitive_buffer_;

	VulkanShader* shader_;

	VkRenderPass render_pass_;
	std::vector<VkFramebuffer> framebuffers_;
	VkPipelineLayout pipeline_layout_;
	VkPipeline pipeline_;


	VkDescriptorPool descriptor_pool_;
	VkDescriptorSetLayout descriptor_set_layout_;
	VkDescriptorSet descriptor_set_;

	// info used in the creation of the pipeline
	std::vector<Descriptor> descriptor_infos_;
};

#endif