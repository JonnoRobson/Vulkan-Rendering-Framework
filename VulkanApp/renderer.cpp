#include "renderer.h"
#include <array>
#include <map>

void VulkanRenderer::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string vs_filename, std::string ps_filename)
{
	devices_ = devices;
	swap_chain_ = swap_chain;

	CreateShaders();
	CreatePrimitiveBuffer();
	CreateRenderPass();
	CreateFramebuffers();
	CreatePipeline();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateDescriptorPool();
	CreateSemaphores();

	for (VulkanShader* shader : shaders_)
	{
		// initialize shader descriptor sets for this descriptor pool
		shader->GetDescriptorSet(descriptor_pool_);
	}

	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().graphics_family, 0, &graphics_queue_);
}

void VulkanRenderer::Cleanup()
{
	// clean up framebuffers and render pass
	vkDestroyRenderPass(devices_->GetLogicalDevice(), render_pass_, nullptr);
	for (size_t i = 0; i < frame_buffers_.size(); i++)
	{
		vkDestroyFramebuffer(devices_->GetLogicalDevice(), frame_buffers_[i], nullptr);
	}

	// clean up pipeline
	vkDestroyPipelineLayout(devices_->GetLogicalDevice(), pipeline_layout_, nullptr);
	vkDestroyPipeline(devices_->GetLogicalDevice(), pipeline_, nullptr);

	// clean up command and descriptor pools
	vkDestroyCommandPool(devices_->GetLogicalDevice(), command_pool_, nullptr);
	vkDestroyDescriptorPool(devices_->GetLogicalDevice(), descriptor_pool_, nullptr);

	// clean up semaphores
	for (size_t i = 0; i < semaphores_.size(); i++)
	{
		vkDestroySemaphore(devices_->GetLogicalDevice(), semaphores_[i], nullptr);
	}

	// clean up shaders
	for (size_t i = 0; i < shaders_.size(); i++)
	{
		shaders_[i]->Cleanup();
		delete shaders_[i];
	}
	shaders_.clear();

	// clean up primitive buffer
	primitive_buffer_->Cleanup();
	delete primitive_buffer_;
	primitive_buffer_ = nullptr;
}

void VulkanRenderer::RecreateSwapChainFeatures()
{
	// free the previous swap chain features
	vkDestroyRenderPass(devices_->GetLogicalDevice(), render_pass_, nullptr);
	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, static_cast<uint32_t>(begin_command_buffers_.size()), begin_command_buffers_.data());
	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, static_cast<uint32_t>(render_command_buffers_.size()), render_command_buffers_.data());
	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, static_cast<uint32_t>(finish_command_buffers_.size()), finish_command_buffers_.data());

	for (size_t i = 0; i < frame_buffers_.size(); i++)
	{
		vkDestroyFramebuffer(devices_->GetLogicalDevice(), frame_buffers_[i], nullptr);
	}

	// create the new swap chain features
	CreateRenderPass();
	CreateFramebuffers();
	CreateCommandBuffers();
}

void VulkanRenderer::PreRender()
{
	// get the next available swap chain image
	uint32_t image_index = swap_chain_->GetCurrentSwapChainImage();

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { swap_chain_->GetImageAvailableSemaphore() };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &begin_command_buffers_[image_index];

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit clear command buffer!");
	}
}

void VulkanRenderer::RenderPass()
{
	// get the next available swap chain image
	uint32_t image_index = swap_chain_->GetCurrentSwapChainImage();

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { swap_chain_->GetImageAvailableSemaphore() };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &render_command_buffers_[image_index];

	VkSemaphore signal_semaphores[] = { semaphores_[1] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;


	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}
}

void VulkanRenderer::PostRender()
{

}

void VulkanRenderer::CreateRenderPass()
{
	// setup the color buffer attachment
	VkAttachmentDescription color_attachment = {};
	color_attachment.format = swap_chain_->GetSwapChainImageFormat();
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// setup the subpass attachment description
	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the depth buffer attachment
	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = swap_chain_->FindDepthFormat();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass description
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
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
	std::array<VkAttachmentDescription, 2> attachments = { color_attachment, depth_attachment };

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

void VulkanRenderer::CreateRenderPasses()
{
	// setup the buffer clear pass description
	// setup the color buffer attachment
	VkAttachmentDescription color_attachment = {};
	color_attachment.format = swap_chain_->GetSwapChainImageFormat();
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// setup the subpass attachment description
	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// setup the depth buffer attachment
	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format = swap_chain_->FindDepthFormat();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass attachment description
	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// setup the subpass description
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
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
	std::array<VkAttachmentDescription, 2> attachments = { color_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	render_pass_info.pAttachments = attachments.data();
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	if (vkCreateRenderPass(devices_->GetLogicalDevice(), &render_pass_info, nullptr, &clear_render_pass_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create clearing render pass!");
	}

	// modify the render pass description for the draw pass
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

	if (vkCreateRenderPass(devices_->GetLogicalDevice(), &render_pass_info, nullptr, &draw_render_pass_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create draw render pass!");
	}
}

void VulkanRenderer::CreateFramebuffers()
{
	std::vector<VkImageView>& image_views = swap_chain_->GetSwapChainImageViews();
	frame_buffers_.resize(image_views.size());

	for (size_t i = 0; i < image_views.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { image_views[i], swap_chain_->GetDepthImageView() };

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = render_pass_;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = swap_chain_->GetSwapChainExtent().width;
		framebuffer_info.height = swap_chain_->GetSwapChainExtent().height;
		framebuffer_info.layers = 1;

		if (vkCreateFramebuffer(devices_->GetLogicalDevice(), &framebuffer_info, nullptr, &frame_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void VulkanRenderer::CreatePipeline()
{
	VkDevice vk_device = devices_->GetLogicalDevice();

	std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos;
	std::vector<VkPipelineVertexInputStateCreateInfo> vertex_input_infos;
	std::vector<VkPipelineInputAssemblyStateCreateInfo> input_assembly_infos;
	std::vector<VkPipelineViewportStateCreateInfo> viewport_state_infos;
	std::vector<VkPipelineRasterizationStateCreateInfo> rasterizer_state_infos;
	std::vector<VkPipelineMultisampleStateCreateInfo> multisampling_infos;
	std::vector<VkPipelineDepthStencilStateCreateInfo> depth_stencil_infos;
	std::vector<VkPipelineColorBlendStateCreateInfo> blend_state_infos;
	std::vector<VkPipelineDynamicStateCreateInfo> dynamic_state_infos;
	std::vector<VkDescriptorSetLayout> descriptor_set_layouts;

	// set up shader stage info
	for (VulkanShader* shader : shaders_)
	{
		// append shader stage info
		std::vector<VkPipelineShaderStageCreateInfo> shader_infos = shader->GetShaderStageInfo();
		for (size_t i = 0; i < shader_infos.size(); i++)
		{
			shader_stage_infos.push_back(shader_infos[i]);
		}

		vertex_input_infos.push_back(shader->GetVertexInputDescription());
	
		// append input assembly info
		input_assembly_infos.push_back(shader->GetInputAssemblyDescription());

		// append viewport state info
		viewport_state_infos.push_back(shader->GetViewportStateDescription());

		// append rasterizer state info
		rasterizer_state_infos.push_back(shader->GetRasterizerStateDescription());

		// append multisample state info
		multisampling_infos.push_back(shader->GetMultisampleStateDescription());

		// append depth stencil state info
		depth_stencil_infos.push_back(shader->GetDepthStencilStateDescription());

		// append blend state info
		blend_state_infos.push_back(shader->GetBlendStateDescription());

		// append dynamic state info
		dynamic_state_infos.push_back(shader->GetDynamicStateDescription());

		// append descriptor set layout info
		descriptor_set_layouts.push_back(shader->GetDescriptorSetLayout());
	}

	// setup pipeline layout creation info
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());
	pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = 0;

	// create the pipeline layout
	if (vkCreatePipelineLayout(vk_device, &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// setup pipeline creation info
	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = static_cast<uint32_t>(shader_stage_infos.size());
	pipeline_info.pStages = shader_stage_infos.data();
	pipeline_info.pVertexInputState = vertex_input_infos.data();
	pipeline_info.pInputAssemblyState = input_assembly_infos.data();
	pipeline_info.pViewportState = viewport_state_infos.data();
	pipeline_info.pRasterizationState = rasterizer_state_infos.data();
	pipeline_info.pMultisampleState = multisampling_infos.data();
	pipeline_info.pDepthStencilState = depth_stencil_infos.data();
	pipeline_info.pColorBlendState = blend_state_infos.data();
	pipeline_info.pDynamicState = dynamic_state_infos.data();
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.renderPass = render_pass_;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.flags = 0;

	if (vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}
}


void VulkanRenderer::CreateCommandPool()
{
	QueueFamilyIndices queue_family_indices = VulkanDevices::FindQueueFamilies(devices_->GetPhysicalDevice(), swap_chain_->GetSurface());

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
	pool_info.flags = 0;

	if (vkCreateCommandPool(devices_->GetLogicalDevice(), &pool_info, nullptr, &command_pool_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

void VulkanRenderer::CreateCommandBuffers()
{
	CreateBeginCommandBuffers();
	CreateFinishCommandBuffers();
}

void VulkanRenderer::CreateBeginCommandBuffers()
{
	begin_command_buffers_.resize(frame_buffers_.size());

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = (uint32_t)begin_command_buffers_.size();

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, begin_command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render begin command buffers!");
	}

	for (size_t i = 0; i < begin_command_buffers_.size(); i++)
	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(begin_command_buffers_[i], &begin_info);

		std::array<VkClearValue, 2> clear_values = {};
		clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clear_values[1].depthStencil = { 1.0f, 0 };

		VkClearRect rect = {};
		rect.rect.offset = { 0, 0 };
		rect.rect.extent = swap_chain_->GetSwapChainExtent();
		rect.baseArrayLayer = 0;
		rect.layerCount = 1;

		VkRenderPassBeginInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = clear_render_pass_;
		render_pass_info.framebuffer = frame_buffers_[i];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swap_chain_->GetSwapChainExtent();
		render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
		render_pass_info.pClearValues = clear_values.data();

		// clear the framebuffer image
		//vkCmdBeginRenderPass(begin_command_buffers_[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		//vkCmdEndRenderPass(begin_command_buffers_[i]);
		
		if (vkEndCommandBuffer(begin_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record render begin command buffer!");
		}
	}
}

void VulkanRenderer::CreateRenderCommandBuffers()
{
	render_command_buffers_.resize(frame_buffers_.size());

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = (uint32_t)render_command_buffers_.size();

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, render_command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}

	for (size_t i = 0; i < render_command_buffers_.size(); i++)
	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(render_command_buffers_[i], &begin_info);

		std::array<VkClearValue, 2> clear_values = {};
		clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clear_values[1].depthStencil = { 1.0f, 0 };
		
		VkRenderPassBeginInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass_;
		render_pass_info.framebuffer = frame_buffers_[i];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swap_chain_->GetSwapChainExtent();
		render_pass_info.clearValueCount = 0;
		render_pass_info.pClearValues = nullptr;

		primitive_buffer_->RecordBindingCommands(render_command_buffers_[i]);

		// create pipleine commands
		vkCmdBeginRenderPass(render_command_buffers_[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(render_command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
		
		for (VulkanShader* shader : shaders_)
		{
			shader->RecordShaderCommands(render_command_buffers_[i], pipeline_layout_, descriptor_pool_);
			
			if (shader->IsPrimitiveShader())
			{
				for (Mesh* mesh : meshes_)
				{
					mesh->RecordDrawCommands(render_command_buffers_[i], shaders_[0]->GetUniformBuffer(0).GetBuffer());
				}
			}
		}
		
		vkCmdEndRenderPass(render_command_buffers_[i]);

		if (vkEndCommandBuffer(render_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record render command buffer!");
		}
	}
}

void VulkanRenderer::CreateFinishCommandBuffers()
{
	finish_command_buffers_.resize(frame_buffers_.size());

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = (uint32_t)finish_command_buffers_.size();

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, finish_command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render finish command buffers!");
	}
}

void VulkanRenderer::CreateDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> pool_sizes;

	for (VulkanShader* shader : shaders_)
	{
		VkDescriptorSetLayoutCreateInfo layout_info = shader->GetDescriptorSetLayoutInfo();

		for (uint32_t i = 0; i < layout_info.bindingCount; i++)
		{
			bool found = false;
			
			// search for the binding in the exist list of bindings
			for (VkDescriptorPoolSize& binding : pool_sizes)
			{
				if (layout_info.pBindings[i].descriptorType == binding.type)
				{
					// if binding is found increment binding count
					binding.descriptorCount++;
					found = true;
					break;
				}
			}

			if (!found)
			{
				VkDescriptorPoolSize binding;
				binding.type = layout_info.pBindings[i].descriptorType;
				binding.descriptorCount = 1;
				pool_sizes.push_back(binding);
			}
		}
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();
	pool_info.maxSets = 1; // change this probably

	if (vkCreateDescriptorPool(devices_->GetLogicalDevice(), &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void VulkanRenderer::CreateShaders()
{
	VulkanShader* render_shader = new VulkanShader();
	render_shader->Init(devices_, swap_chain_, "../res/shaders/vert.spv", "", "", "../res/shaders/frag.spv");

	shaders_.push_back(render_shader);
}

void VulkanRenderer::CreateSemaphores()
{
	VkSemaphore clear_finished_semaphore, render_finished_semaphore;

	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &clear_finished_semaphore) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &render_finished_semaphore) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}

	semaphores_.push_back(clear_finished_semaphore);
	semaphores_.push_back(render_finished_semaphore);
}

void VulkanRenderer::CreatePrimitiveBuffer()
{
	primitive_buffer_ = new VulkanPrimitiveBuffer();
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
	primitive_buffer_->Init(devices_, Vertex::GetBindingDescription(), attribute_descriptions);
}

void VulkanRenderer::AddMesh(Mesh* mesh)
{
	meshes_.push_back(mesh);
	mesh->AddToPrimitiveBuffer(devices_, primitive_buffer_);

	// recreate the command buffer
	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, render_command_buffers_.size(), render_command_buffers_.data());
	CreateRenderCommandBuffers();
}

void VulkanRenderer::RemoveMesh(Mesh* remove_mesh)
{
	bool found = false;
	auto mesh_it = meshes_.begin();
	for (auto mesh = meshes_.begin(); mesh != meshes_.end(); mesh++)
	{
		if (*mesh == remove_mesh)
		{
			mesh_it = mesh;
			found = true;
		}
	}

	if (found)
	{
		meshes_.erase(mesh_it);

		// recreate the command buffer
		CreateRenderCommandBuffers();
	}
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& code)
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