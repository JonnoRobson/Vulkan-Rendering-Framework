#include "shader.h"
#include "mesh.h"

void VulkanShader::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string vs_filename, std::string ts_filename, std::string gs_filename, std::string fs_filename)
{
	devices_ = devices;
	swap_chain_ = swap_chain;

	primitive_shader_ = true;

	LoadShaders(vs_filename, ts_filename, gs_filename, fs_filename);

	// create shader resources
	CreateTextures();
	CreateSamplers();
	CreateUniformBuffers();

	// create shader descriptors
	CreateDescriptorLayout();
	CreateVertexInput();
	CreateInputAssembly();
	CreateViewportState();
	CreateRasterizerState();
	CreateMultisampleState();
	CreateDepthStencilState();
	CreateBlendState();
	CreateDynamicState();
}

void VulkanShader::Cleanup()
{
	VkDevice device = devices_->GetLogicalDevice();

	// clean up descriptor set layout
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout_, nullptr);
	
	// clean up shader modules
	vkDestroyShaderModule(device, vertex_shader_module_, nullptr);
	vkDestroyShaderModule(device, tessellation_shader_module_, nullptr);
	vkDestroyShaderModule(device, geometry_shader_module_, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module_, nullptr);

	// clean up textures
	for (Texture& texture : shader_textures_)
	{
		texture.Cleanup();
	}
	shader_textures_.clear();

	// clean up samplers
	for (VkSampler& sampler : shader_samplers_)
	{
		vkDestroySampler(device, sampler, nullptr);
	}
	shader_samplers_.clear();

	// clean up buffers
	for (UniformBuffer& buffer : shader_uniform_buffers_)
	{
		buffer.Cleanup(devices_);
	}
	shader_uniform_buffers_.clear();

}

void VulkanShader::RecordShaderCommands(VkCommandBuffer& command_buffer, VkPipelineLayout pipeline_layout, VkDescriptorPool descriptor_pool)
{
	// set the dynamic viewport data
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)(swap_chain_->GetSwapChainExtent().width);
	viewport.height = (float)(swap_chain_->GetSwapChainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	// set the dynamic scissor data
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swap_chain_->GetSwapChainExtent();
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	// bind the descriptor set to the pipeline
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_sets_[descriptor_pool], 0, nullptr);
}

void VulkanShader::LoadShaders(std::string vs_filename, std::string ts_filename, std::string gs_filename, std::string fs_filename)
{

	if (!vs_filename.empty())
	{
		auto vert_shader_code = VulkanDevices::ReadFile(vs_filename);
		vertex_shader_module_ = CreateShaderModule(vert_shader_code);

		VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {};
		vertex_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertex_shader_stage_info.module = vertex_shader_module_;
		vertex_shader_stage_info.pName = "main";
		shader_stage_info_.push_back(vertex_shader_stage_info);
	}

	if (!ts_filename.empty())
	{
		auto tessellation_shader_code = VulkanDevices::ReadFile(ts_filename);
		tessellation_shader_module_ = CreateShaderModule(tessellation_shader_code);
	}

	if (!gs_filename.empty())
	{
		auto geometry_shader_code = VulkanDevices::ReadFile(gs_filename);
		geometry_shader_module_ = CreateShaderModule(geometry_shader_code);
	}

	if (!fs_filename.empty())
	{
		auto frag_shader_code = VulkanDevices::ReadFile(fs_filename);
		fragment_shader_module_ = CreateShaderModule(frag_shader_code);

		VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
		frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		frag_shader_stage_info.module = fragment_shader_module_;
		frag_shader_stage_info.pName = "main";
		shader_stage_info_.push_back(frag_shader_stage_info);
	}
}

VkShaderModule VulkanShader::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader_module;
	if (vkCreateShaderModule(devices_->GetLogicalDevice(), &create_info, nullptr, &shader_module) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return shader_module;
}

VkDescriptorSet VulkanShader::GetDescriptorSet(VkDescriptorPool descriptor_pool)
{
	auto search = descriptor_sets_.find(descriptor_pool);
	if (search != descriptor_sets_.end())
	{
		return descriptor_sets_[descriptor_pool];
	}
	else
	{
		CreateDescriptorSet(descriptor_pool);
	}
}

void VulkanShader::CreateDescriptorLayout()
{
	// set up uniform buffer binding info
	VkDescriptorSetLayoutBinding ubo_layout_binding = {};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	ubo_layout_binding.pImmutableSamplers = nullptr;
	descriptor_set_layout_bindings_.push_back(ubo_layout_binding);

	// set up sampler binding info
	VkDescriptorSetLayoutBinding sampler_layout_binding = {};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = nullptr;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descriptor_set_layout_bindings_.push_back(sampler_layout_binding);

	// setup descriptor set layout creation info
	descriptor_set_layout_info_ = {};
	descriptor_set_layout_info_.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_layout_info_.bindingCount = static_cast<uint32_t>(descriptor_set_layout_bindings_.size());
	descriptor_set_layout_info_.pBindings = descriptor_set_layout_bindings_.data();

	if (vkCreateDescriptorSetLayout(devices_->GetLogicalDevice(), &descriptor_set_layout_info_, nullptr, &descriptor_set_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void VulkanShader::CreateDescriptorSet(VkDescriptorPool descriptor_pool)
{
	VkDescriptorSetLayout layouts[] = { descriptor_set_layout_ };
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(devices_->GetLogicalDevice(), &alloc_info, &descriptor_sets_[descriptor_pool]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor set!");
	}

	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = shader_uniform_buffers_[0].GetBuffer();
	buffer_info.offset = 0;
	buffer_info.range = shader_uniform_buffers_[0].GetBufferSize();

	VkDescriptorImageInfo image_info = {};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = shader_textures_[0].GetImageView();
	image_info.sampler = shader_samplers_[0];

	// set up write data for descriptor sets
	std::array<VkWriteDescriptorSet, 2> descriptor_writes = {};
	// uniform buffer data
	descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[0].dstSet = descriptor_sets_[descriptor_pool];
	descriptor_writes[0].dstBinding = 0;
	descriptor_writes[0].dstArrayElement = 0;
	descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_writes[0].descriptorCount = 1;
	descriptor_writes[0].pBufferInfo = &buffer_info;
	// sampler data
	descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[1].dstSet = descriptor_sets_[descriptor_pool];
	descriptor_writes[1].dstBinding = 1;
	descriptor_writes[1].dstArrayElement = 0;
	descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_writes[1].descriptorCount = 1;
	descriptor_writes[1].pImageInfo = &image_info;

	vkUpdateDescriptorSets(devices_->GetLogicalDevice(), static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
}

void VulkanShader::CreateTextures()
{
	Texture test_texture;
	test_texture.Init(devices_, "../res/textures/texture.jpg");
	shader_textures_.push_back(test_texture);
}

void VulkanShader::CreateSamplers()
{
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = 16;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	VkSampler test_texture_sampler;
	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &test_texture_sampler) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}

	shader_samplers_.push_back(test_texture_sampler);
}

void VulkanShader::CreateUniformBuffers()
{
	UniformBuffer uniform_buffer;
	uniform_buffer.Init(devices_, 0, 0, 0, 0, 3);
	shader_uniform_buffers_.push_back(uniform_buffer);
}

void VulkanShader::CreateVertexInput()
{
	CreateVertexBinding();
	CreateVertexAttributes();

	vertex_input_ = {};
	vertex_input_.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_.vertexBindingDescriptionCount = 1;
	vertex_input_.pVertexBindingDescriptions = &vertex_binding_;
	vertex_input_.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes_.size());
	vertex_input_.pVertexAttributeDescriptions = vertex_attributes_.data();
}

void VulkanShader::CreateVertexBinding()
{
	// get the binding description from Vertex since this is what is used in a base render shader
	vertex_binding_ = Vertex::GetBindingDescription();
}

void VulkanShader::CreateVertexAttributes()
{
	// get the binding attributes from Vertex since this is what is used in a base render shader
	auto attribute_descriptions = Vertex::GetAttributeDescriptions();

	for (VkVertexInputAttributeDescription& description : attribute_descriptions)
	{
		vertex_attributes_.push_back(description);
	}
}

void VulkanShader::CreateInputAssembly()
{
	// setup input assembly creation info
	input_assembly_ = {};
	input_assembly_.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_.primitiveRestartEnable = VK_FALSE;
}

void VulkanShader::CreateViewportState()
{
	// setup viewport creation info
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)(swap_chain_->GetSwapChainExtent().width);
	viewport.height = (float)(swap_chain_->GetSwapChainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewports_.push_back(viewport);

	// setup scissor rect info
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swap_chain_->GetSwapChainExtent();
	scissor_rects_.push_back(scissor);

	// setup viewport state creation info
	viewport_state_ = {};
	viewport_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_.viewportCount = static_cast<uint32_t>(viewports_.size());
	viewport_state_.pViewports = viewports_.data();
	viewport_state_.scissorCount = static_cast<uint32_t>(scissor_rects_.size());
	viewport_state_.pScissors = scissor_rects_.data();
}

void VulkanShader::CreateRasterizerState()
{
	// setup rasterizer creation info
	rasterizer_state_ = {};
	rasterizer_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_state_.depthClampEnable = VK_FALSE;
	rasterizer_state_.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_state_.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_state_.lineWidth = 1.0f;
	rasterizer_state_.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_state_.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer_state_.depthBiasEnable = VK_FALSE;
	rasterizer_state_.depthBiasConstantFactor = 0.0f;
	rasterizer_state_.depthBiasClamp = 0.0f;
	rasterizer_state_.depthBiasSlopeFactor = 0.0f;
}

void VulkanShader::CreateMultisampleState()
{
	// setup multisample state creation info
	multisample_state_= {};
	multisample_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state_.sampleShadingEnable = VK_FALSE;
	multisample_state_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state_.minSampleShading = 1.0f;
	multisample_state_.pSampleMask = nullptr;
	multisample_state_.alphaToCoverageEnable = VK_FALSE;
	multisample_state_.alphaToOneEnable = VK_FALSE;
}

void VulkanShader::CreateDepthStencilState()
{
	// setup the depth stencil state creation info
	depth_stencil_state_ = {};
	depth_stencil_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state_.depthTestEnable = VK_TRUE;
	depth_stencil_state_.depthWriteEnable = VK_TRUE;
	depth_stencil_state_.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_state_.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state_.minDepthBounds = 0.0f;
	depth_stencil_state_.maxDepthBounds = 1.0f;
	depth_stencil_state_.stencilTestEnable = VK_FALSE;
	depth_stencil_state_.front = {};
	depth_stencil_state_.back = {};
}

void VulkanShader::CreateBlendState()
{
	// setup color blend creation info
	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	attachment_blend_states_.push_back(color_blend_attachment);

	// setup global color blend creation info
	blend_state_ = {};
	blend_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state_.logicOpEnable = VK_FALSE;
	blend_state_.logicOp = VK_LOGIC_OP_COPY;
	blend_state_.attachmentCount = static_cast<uint32_t>(attachment_blend_states_.size());
	blend_state_.pAttachments = attachment_blend_states_.data();
	blend_state_.blendConstants[0] = 0.0f;
	blend_state_.blendConstants[1] = 0.0f;
	blend_state_.blendConstants[2] = 0.0f;
	blend_state_.blendConstants[3] = 0.0f;
}

void VulkanShader::CreateDynamicState()
{
	// setup pipeline dynamic state info
	dynamic_states_.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamic_states_.push_back(VK_DYNAMIC_STATE_SCISSOR);

	dynamic_state_description_ = {};
	dynamic_state_description_.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_description_.dynamicStateCount = static_cast<uint32_t>(dynamic_states_.size());
	dynamic_state_description_.pDynamicStates = dynamic_states_.data();
}