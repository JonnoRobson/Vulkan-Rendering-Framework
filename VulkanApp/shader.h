#ifndef _SHADER_H_
#define _SHADER_H_

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <map>

#include "device.h"
#include "swap_chain.h"
#include "texture.h"
#include "uniform_buffer.h"

class VulkanShader
{
public:
	void Init(VulkanDevices* devices,  VulkanSwapChain* swap_chain, std::string vs_filename, std::string ts_filename, std::string gs_filename, std::string fs_filename);
	virtual void Cleanup();

	virtual void RecordShaderCommands(VkCommandBuffer& command_buffer, VkPipelineLayout pipeline, VkDescriptorPool descriptor_pool);

	// getters
	VkShaderModule GetVertexShader() { return vertex_shader_module_; }
	VkShaderModule GetTessellationShader() { return tessellation_shader_module_; }
	VkShaderModule GetGeometryShader() { return geometry_shader_module_; }
	VkShaderModule GetFragmentShader() { return fragment_shader_module_; }

	VkDescriptorSet GetDescriptorSet(VkDescriptorPool descriptor_pool);
	
	const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStageInfo() const { return shader_stage_info_; }
	const VkVertexInputBindingDescription& GetBindingDescription() const { return vertex_binding_; }
	const std::vector<VkVertexInputAttributeDescription>& GetAttributeDescriptions() const { return vertex_attributes_; }
	const VkPipelineVertexInputStateCreateInfo& GetVertexInputDescription() const { return vertex_input_; }
	const VkPipelineInputAssemblyStateCreateInfo& GetInputAssemblyDescription() const { return input_assembly_; }
	const VkPipelineViewportStateCreateInfo& GetViewportStateDescription() const { return viewport_state_; }
	const VkPipelineRasterizationStateCreateInfo& GetRasterizerStateDescription() const { return rasterizer_state_; }
	const VkPipelineMultisampleStateCreateInfo& GetMultisampleStateDescription() const { return multisample_state_; }
	const VkPipelineDepthStencilStateCreateInfo& GetDepthStencilStateDescription() const { return depth_stencil_state_; }
	const VkPipelineColorBlendStateCreateInfo& GetBlendStateDescription() const { return blend_state_; }
	const VkPipelineDynamicStateCreateInfo& GetDynamicStateDescription() const { return dynamic_state_description_; }
	const VkDescriptorSetLayout& GetDescriptorSetLayout() const { return descriptor_set_layout_; }
	const VkDescriptorSetLayoutCreateInfo& GetDescriptorSetLayoutInfo() const { return descriptor_set_layout_info_; }

	Texture& GetTexture(int index) { return shader_textures_[index]; }
	VkSampler& GetSampler(int index) {return shader_samplers_[index]; }
	UniformBuffer& GetUniformBuffer(int index) { return shader_uniform_buffers_[index]; }

	bool IsPrimitiveShader() { return primitive_shader_; }

protected:
	virtual void LoadShaders(std::string vs_filename, std::string ts_filename, std::string gs_filename, std::string fs_filename);

	virtual void CreateDescriptorSet(VkDescriptorPool descriptor_pool);
	virtual void CreateDescriptorLayout();

	virtual void CreateTextures();
	virtual void CreateSamplers();
	virtual void CreateUniformBuffers();

	// these functions actually create the descriptions of the state but named for clarity
	virtual void CreateVertexBinding();
	virtual void CreateVertexAttributes();
	virtual void CreateVertexInput();
	virtual void CreateInputAssembly();
	virtual void CreateViewportState();
	virtual void CreateRasterizerState();
	virtual void CreateMultisampleState();
	virtual void CreateDepthStencilState();
	virtual void CreateBlendState();
	virtual void CreateDynamicState();

	VkShaderModule CreateShaderModule(const std::vector<char>& code);

protected:
	// device and swap chain handles for function calls
	VulkanDevices* devices_;
	VulkanSwapChain* swap_chain_;

	VkDescriptorSetLayout descriptor_set_layout_;
	std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings_;
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info_;
	std::map<VkDescriptorPool, VkDescriptorSet> descriptor_sets_;

	// pipeline creation data
	std::vector<VkPipelineShaderStageCreateInfo> shader_stage_info_;
	VkVertexInputBindingDescription vertex_binding_;
	std::vector<VkVertexInputAttributeDescription> vertex_attributes_;
	VkPipelineVertexInputStateCreateInfo vertex_input_;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_;
	std::vector<VkViewport> viewports_;
	std::vector<VkRect2D> scissor_rects_;
	VkPipelineViewportStateCreateInfo viewport_state_;
	VkPipelineRasterizationStateCreateInfo rasterizer_state_;
	VkPipelineMultisampleStateCreateInfo multisample_state_;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state_;
	std::vector<VkPipelineColorBlendAttachmentState> attachment_blend_states_;
	VkPipelineColorBlendStateCreateInfo blend_state_;
	std::vector<VkDynamicState> dynamic_states_;
	VkPipelineDynamicStateCreateInfo dynamic_state_description_;

	VkShaderModule vertex_shader_module_;
	VkShaderModule tessellation_shader_module_;
	VkShaderModule geometry_shader_module_;
	VkShaderModule fragment_shader_module_;

	std::vector<Texture> shader_textures_;
	std::vector<VkSampler> shader_samplers_;
	std::vector<UniformBuffer> shader_uniform_buffers_;

	bool primitive_shader_;
};

#endif