#include "weighted_blended_transparency_pipeline.h"
#include <array>

void WeightedBlendedTransparencyPipeline::RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index)
{
	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_;
	render_pass_info.framebuffer = framebuffers_[buffer_index];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = swap_chain_->GetIntermediateImageExtent();

	std::array<VkClearValue, 2> clear_values = {};
	clear_values[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };

	render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues = clear_values.data();

	// create pipleine commands
	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

	primitive_buffer_->RecordBindingCommands(command_buffer);

	// set the dynamic viewport data
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swap_chain_->GetIntermediateImageExtent().width;
	viewport.height = (float)swap_chain_->GetIntermediateImageExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	// set the dynamic scissor data
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swap_chain_->GetIntermediateImageExtent();
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	// bind the descriptor set to the pipeline
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);
}


void WeightedBlendedTransparencyPipeline::CreatePipeline()
{
	// setup pipeline layout creation info
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = 0;

	// create the pipeline layout
	if (vkCreatePipelineLayout(devices_->GetLogicalDevice(), &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// setup color blend creation info
	VkPipelineColorBlendAttachmentState accumulation_blend_attachment = {};
	accumulation_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	accumulation_blend_attachment.blendEnable = VK_TRUE;
	accumulation_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	accumulation_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	accumulation_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	accumulation_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	accumulation_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	accumulation_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendAttachmentState revealage_blend_attachment = {};
	revealage_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	revealage_blend_attachment.blendEnable = VK_TRUE;
	revealage_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	revealage_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	revealage_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	revealage_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	revealage_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	revealage_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	std::array<VkPipelineColorBlendAttachmentState, 2> attachment_blend_states = { accumulation_blend_attachment, revealage_blend_attachment };

	// setup global color blend creation info
	VkPipelineColorBlendStateCreateInfo blend_state = {};
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = static_cast<uint32_t>(attachment_blend_states.size());
	blend_state.pAttachments = attachment_blend_states.data();
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	// setup depth stencil state info
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {}; 
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_FALSE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;
	depth_stencil_state.stencilTestEnable = VK_FALSE;
	depth_stencil_state.front = {};
	depth_stencil_state.back = {};

	// set up multisample state description
	VkPipelineMultisampleStateCreateInfo multisample_state = {};
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.rasterizationSamples = accumulation_buffer_->GetSampleCount();
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = nullptr;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

	// setup pipeline creation info
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = shader_->GetShaderStageCount();
	pipeline_info.pStages = shader_->GetShaderStageInfo().data();
	pipeline_info.pVertexInputState = &shader_->GetVertexInputDescription();
	pipeline_info.pInputAssemblyState = &shader_->GetInputAssemblyDescription();
	pipeline_info.pViewportState = &shader_->GetViewportStateDescription();
	pipeline_info.pRasterizationState = &shader_->GetRasterizerStateDescription();
	pipeline_info.pMultisampleState = &multisample_state;
	pipeline_info.pDepthStencilState = &depth_stencil_state;
	pipeline_info.pColorBlendState = &blend_state;
	pipeline_info.pDynamicState = &shader_->GetDynamicStateDescription();
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.renderPass = render_pass_;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.flags = 0;

	if (vkCreateGraphicsPipelines(devices_->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}
}

void WeightedBlendedTransparencyPipeline::CreateFramebuffers()
{
	framebuffers_.resize(1);

	std::vector<VkImageView> image_views = { accumulation_buffer_->GetImageViews()[0], revealage_buffer_->GetImageViews()[0] };
	std::array<VkImageView, 3> attachments = { image_views[0], image_views[1], swap_chain_->GetDepthImageView() };

	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = render_pass_;
	framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebuffer_info.pAttachments = attachments.data();
	framebuffer_info.width = swap_chain_->GetIntermediateImageExtent().width;
	framebuffer_info.height = swap_chain_->GetIntermediateImageExtent().height;
	framebuffer_info.layers = 1;

	if (vkCreateFramebuffer(devices_->GetLogicalDevice(), &framebuffer_info, nullptr, &framebuffers_[0]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create framebuffer!");
	}
}

void WeightedBlendedTransparencyPipeline::CreateRenderPass()
{
	// setup the 1st g buffer attachment
	VkAttachmentDescription accumulation_attachment = {};
	accumulation_attachment.format = accumulation_buffer_->GetRenderTargetFormat();
	accumulation_attachment.samples = accumulation_buffer_->GetSampleCount();
	accumulation_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	accumulation_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	accumulation_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	accumulation_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	accumulation_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	accumulation_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference accumulation_attachment_ref = {};
	accumulation_attachment_ref.attachment = 0;
	accumulation_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the 2nd g buffer attachment
	VkAttachmentDescription revealage_attachment = {};
	revealage_attachment.format = revealage_buffer_->GetRenderTargetFormat();
	revealage_attachment.samples = revealage_buffer_->GetSampleCount();
	revealage_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	revealage_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	revealage_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	revealage_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	revealage_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	revealage_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference revealage_attachment_ref = {};
	revealage_attachment_ref.attachment = 1;
	revealage_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the depth buffer attachment
	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = swap_chain_->FindDepthFormat();
	depth_attachment.samples = swap_chain_->GetSampleCount();
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 2;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 2> color_attachments = { accumulation_attachment_ref, revealage_attachment_ref };

	// setup the subpass description
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = color_attachments.size();
	subpass.pColorAttachments = color_attachments.data();
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	// setup the render pass dependancy description
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// setup the render pass description
	std::array<VkAttachmentDescription, 3> attachments = { accumulation_attachment, revealage_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	render_pass_info.pAttachments = attachments.data();
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	if (vkCreateRenderPass(devices_->GetLogicalDevice(), &render_pass_info, nullptr, &render_pass_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void WeightedBlendedTransparencyPipeline::SetRenderTargets(VulkanRenderTarget* accumulation, VulkanRenderTarget* revealage)
{
	accumulation_buffer_ = accumulation;
	revealage_buffer_ = revealage;
}