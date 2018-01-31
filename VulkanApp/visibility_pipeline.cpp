#include "visibility_pipeline.h"
#include <array>

void VisibilityPipeline::RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index)
{
	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_;
	render_pass_info.framebuffer = framebuffers_[buffer_index];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = swap_chain_->GetSwapChainExtent();

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
	viewport.width = (float)swap_chain_->GetSwapChainExtent().width;
	viewport.height = (float)swap_chain_->GetSwapChainExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	// set the dynamic scissor data
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swap_chain_->GetSwapChainExtent();
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	// bind the descriptor set to the pipeline
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);
}

void VisibilityPipeline::CreatePipeline()
{
	// setup push constant info
	VkPushConstantRange push_constant = {};
	push_constant.size = sizeof(uint32_t);
	push_constant.offset = 0;
	push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// setup pipeline layout creation info
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push_constant;

	// create the pipeline layout
	if (vkCreatePipelineLayout(devices_->GetLogicalDevice(), &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// setup color blend creation info
	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	std::array<VkPipelineColorBlendAttachmentState, 1> attachment_blend_states = { color_blend_attachment };

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

	// setup pipeline creation info
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = shader_->GetShaderStageCount();
	pipeline_info.pStages = shader_->GetShaderStageInfo().data();
	pipeline_info.pVertexInputState = &shader_->GetVertexInputDescription();
	pipeline_info.pInputAssemblyState = &shader_->GetInputAssemblyDescription();
	pipeline_info.pViewportState = &shader_->GetViewportStateDescription();
	pipeline_info.pRasterizationState = &shader_->GetRasterizerStateDescription();
	pipeline_info.pMultisampleState = &shader_->GetMultisampleStateDescription();
	pipeline_info.pDepthStencilState = &shader_->GetDepthStencilStateDescription();
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

void VisibilityPipeline::CreateFramebuffers()
{
	framebuffers_.resize(1);

	std::vector<VkImageView> image_views = visibility_buffer_->GetImageViews();
	std::array<VkImageView, 2> attachments = { image_views[0], swap_chain_->GetDepthImageView() };

	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = render_pass_;
	framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebuffer_info.pAttachments = attachments.data();
	framebuffer_info.width = swap_chain_->GetSwapChainExtent().width;
	framebuffer_info.height = swap_chain_->GetSwapChainExtent().height;
	framebuffer_info.layers = 1;

	if (vkCreateFramebuffer(devices_->GetLogicalDevice(), &framebuffer_info, nullptr, &framebuffers_[0]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create framebuffer!");
	}
}

void VisibilityPipeline::CreateRenderPass()
{
	// setup the 1st g buffer attachment
	VkAttachmentDescription visibility_attachment = {};
	visibility_attachment.format = visibility_buffer_->GetRenderTargetFormat();
	visibility_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	visibility_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	visibility_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	visibility_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	visibility_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	visibility_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	visibility_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// setup the subpass attachment description
	VkAttachmentReference visibility_attachment_ref = {};
	visibility_attachment_ref.attachment = 0;
	visibility_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the depth buffer attachment
	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = swap_chain_->FindDepthFormat();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 1> color_attachments = { visibility_attachment_ref };

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
	std::array<VkAttachmentDescription, 2> attachments = { visibility_attachment, depth_attachment };

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
