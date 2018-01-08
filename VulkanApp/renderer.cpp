#include "renderer.h"
#include <array>
#include <map>

void VulkanRenderer::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string vs_filename, std::string ps_filename)
{
	devices_ = devices;
	swap_chain_ = swap_chain;
	render_mode_ = RenderMode::DEFERRED;

	// load a default texture
	default_texture_ = new Texture();
	default_texture_->Init(devices, "../res/textures/default.png");

	// create the texture cache
	texture_cache_ = new VulkanTextureCache(devices);

	CreateMaterialShader(vs_filename, ps_filename);
	CreateShaders();
	CreatePrimitiveBuffer();
	CreateMaterialBuffer();
	CreateCommandPool();
	CreateBuffers();
	CreateSemaphores();

	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().graphics_family, 0, &graphics_queue_);
	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().compute_family, 0, &compute_queue_);
}

void VulkanRenderer::RenderScene()
{
	// regenerate the shadow map for any moving light
	for (Light* light : lights_)
	{
		if (!light->GetLightStationary() && light->GetShadowsEnabled())
			light->GenerateShadowMap();
	}

	// get swap chain index
	uint32_t image_index = swap_chain_->GetCurrentSwapChainImage();
	VkExtent2D swap_extent = swap_chain_->GetSwapChainExtent();


	if (render_mode_ == RenderMode::BUFFER_VIS)
	{
		RenderVisualisation(image_index);
	}
	else if (render_mode_ == RenderMode::FORWARD || render_mode_ == RenderMode::DEFERRED || render_mode_ == RenderMode::DEFERRED_COMPUTE)
	{
		// send matrix data to the gpu
		UniformBufferObject ubo = {};
		ubo.model = glm::mat4(1.0f);
		ubo.view = render_camera_->GetViewMatrix();
		ubo.proj = render_camera_->GetProjectionMatrix();
		
		void* mapped_data;
		vkMapMemory(devices_->GetLogicalDevice(), matrix_buffer_memory_, 0, sizeof(UniformBufferObject), 0, &mapped_data);
		memcpy(mapped_data, &ubo, sizeof(UniformBufferObject));
		vkUnmapMemory(devices_->GetLogicalDevice(), matrix_buffer_memory_);

		// send camera data to the gpu
		SceneLightData scene_data = {};
		scene_data.scene_data = glm::vec4(glm::vec3(0.1f, 0.1f, 0.1f), lights_.size());
		scene_data.camera_pos = glm::vec4(render_camera_->GetPosition(), 1.0f);

		vkMapMemory(devices_->GetLogicalDevice(), light_buffer_memory_, 0, sizeof(SceneLightData), 0, &mapped_data);
		memcpy(mapped_data, &scene_data, sizeof(SceneLightData));
		vkUnmapMemory(devices_->GetLogicalDevice(), light_buffer_memory_);

		for (Light* light : lights_)
		{
			// send the light data to the gpu
			light->SendLightData(devices_, light_buffer_memory_);
		}
		
		// transition the intermediate image to color write optimal layout
		devices_->TransitionImageLayout(swap_chain_->GetIntermediateImage(), swap_chain_->GetSwapChainImageFormat(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		// render the skybox
		skybox_->Render(render_camera_);

		if (render_mode_ == RenderMode::FORWARD)
		{
			RenderForward(image_index);
		}
		else if (render_mode_ == RenderMode::DEFERRED_COMPUTE)
		{
			RenderGBuffer(image_index);
			RenderDeferredCompute(image_index);
		}
		else if (render_mode_ == RenderMode::DEFERRED)
		{
			RenderGBuffer(image_index);
			RenderDeferred(image_index);
		}
	}

}

void VulkanRenderer::RenderForward(uint32_t image_index)
{
	VkSemaphore wait_semaphore = swap_chain_->GetImageAvailableSemaphore();
	VkSemaphore skybox_semaphore = skybox_->GetRenderSemaphore();

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { wait_semaphore, skybox_semaphore };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 2;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffers_[image_index];

	VkSemaphore signal_semaphores[] = { render_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}
}

void VulkanRenderer::RenderGBuffer(uint32_t image_index)
{
	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &g_buffer_command_buffers_[0];

	VkSemaphore signal_semaphores[] = { g_buffer_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}
}

void VulkanRenderer::RenderDeferred(uint32_t image_index)
{
	VkSemaphore skybox_semaphore = skybox_->GetRenderSemaphore();

	//  transition the intermediate image to color write optimal layout
	devices_->TransitionImageLayout(swap_chain_->GetIntermediateImage(), swap_chain_->GetSwapChainImageFormat(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { g_buffer_semaphore_, skybox_semaphore };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 2;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &deferred_command_buffer_;

	VkSemaphore signal_semaphores[] = { render_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}

	// copy the intermediate image to the swap chain
	swap_chain_->FinalizeIntermediateImage();
}

void VulkanRenderer::RenderDeferredCompute(uint32_t image_index)
{
	VkSemaphore skybox_semaphore = skybox_->GetRenderSemaphore();

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { g_buffer_semaphore_, skybox_semaphore };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 2;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &deferred_compute_command_buffer_;

	VkSemaphore signal_semaphores[] = { render_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred compute command buffer!");
	}
	
	// copy the intermediate image to the swap chain
	swap_chain_->FinalizeIntermediateImage();
}

void VulkanRenderer::RenderVisualisation(uint32_t image_index)
{
	VkSemaphore wait_semaphore = swap_chain_->GetImageAvailableSemaphore();

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { wait_semaphore };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &buffer_visualisation_command_buffers_[image_index];

	VkSemaphore signal_semaphores[] = { render_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;


	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit visualisation command buffer!");
	}
}

void VulkanRenderer::Cleanup()
{
	// clean up command and descriptor pools
	vkDestroyCommandPool(devices_->GetLogicalDevice(), command_pool_, nullptr);


	// clean up the buffer
	vkDestroyBuffer(devices_->GetLogicalDevice(), matrix_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), matrix_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), light_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), light_buffer_memory_, nullptr);

	// clean up shaders
	material_shader_->Cleanup();
	delete material_shader_;
	material_shader_ = nullptr;

	buffer_visualisation_shader_->Cleanup();
	delete buffer_visualisation_shader_;
	buffer_visualisation_shader_ = nullptr;

	shadow_map_shader_->Cleanup();
	delete shadow_map_shader_;
	shadow_map_shader_ = nullptr;

	g_buffer_shader_->Cleanup();
	delete g_buffer_shader_;
	g_buffer_shader_ = nullptr;

	deferred_shader_->Cleanup();
	delete deferred_shader_;
	deferred_shader_ = nullptr;

	deferred_compute_shader_->Cleanup();
	delete deferred_compute_shader_;
	deferred_compute_shader_ = nullptr;

	// clean up primitive buffer
	primitive_buffer_->Cleanup();
	delete primitive_buffer_;
	primitive_buffer_ = nullptr;

	// clean up the material buffer
	material_buffer_->CleanUp();
	delete material_buffer_;
	material_buffer_ = nullptr;

	// clean up the texture cache
	texture_cache_->Cleanup();
	delete texture_cache_;
	texture_cache_ = nullptr;

	// clean up the pipeline
	rendering_pipeline_->CleanUp();
	delete rendering_pipeline_;
	rendering_pipeline_ = nullptr;

	// clean up the buffer visualisation pipeline
	buffer_visualisation_pipeline_->CleanUp();
	delete buffer_visualisation_pipeline_;
	buffer_visualisation_pipeline_ = nullptr;

	// clean up deferred rendering pipelines
	g_buffer_pipeline_->CleanUp();
	delete g_buffer_pipeline_;
	g_buffer_pipeline_ = nullptr;

	deferred_pipeline_->CleanUp();
	delete deferred_pipeline_;
	deferred_pipeline_ = nullptr;

	deferred_compute_pipeline_->CleanUp();
	delete deferred_compute_pipeline_;
	deferred_compute_pipeline_ = nullptr;

	// clean up g buffer
	g_buffer_->Cleanup();
	delete g_buffer_;
	g_buffer_ = nullptr;

	// clean up the skybox
	skybox_->Cleanup();
	delete skybox_;
	skybox_ = nullptr;

	// clean up default texture
	default_texture_->Cleanup();
	delete default_texture_;
	default_texture_ = nullptr;

	// clean up samplers
	vkDestroySampler(devices_->GetLogicalDevice(), g_buffer_normalized_sampler_, nullptr);
	vkDestroySampler(devices_->GetLogicalDevice(), g_buffer_unnormalized_sampler_, nullptr);
	vkDestroySampler(devices_->GetLogicalDevice(), shadow_map_sampler_, nullptr);

	// clean up semaphores
	vkDestroySemaphore(devices_->GetLogicalDevice(), render_semaphore_, nullptr);
}

void VulkanRenderer::InitPipelines()
{
	rendering_pipeline_ = new VulkanPipeline();

	// add the material buffers to the pipeline
	rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	CreateLightBuffer();
	rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 3, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));

	// fill out any empty texture arrays using the default texture
	if (ambient_textures_.empty())
		ambient_textures_.push_back(default_texture_);

	if (diffuse_textures_.empty())
		diffuse_textures_.push_back(default_texture_);
	
	if (specular_textures_.empty())
		specular_textures_.push_back(default_texture_);

	if (specular_highlight_textures_.empty())
		specular_highlight_textures_.push_back(default_texture_);

	if (emissive_textures_.empty())
		emissive_textures_.push_back(default_texture_);
	
	if (normal_textures_.empty())
		normal_textures_.push_back(default_texture_);

	if (alpha_textures_.empty())
		alpha_textures_.push_back(default_texture_);

	if (reflection_textures_.empty())
		reflection_textures_.push_back(default_texture_);

	// add the material textures to the pipeline
	rendering_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 4, default_texture_->GetSampler());
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 5, ambient_textures_);
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 6, diffuse_textures_);
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 7, specular_textures_);
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 8, specular_highlight_textures_);
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 9, emissive_textures_);
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 10, normal_textures_);
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 11, alpha_textures_);
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 12, reflection_textures_);

	// set the pipeline material shader
	rendering_pipeline_->SetShader(material_shader_);

	// create the material pipeline
	rendering_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	InitDeferredPipeline();

	// initialize a buffer visualisation pipeline
	buffer_visualisation_pipeline_ = new BufferVisualisationPipeline();
	buffer_visualisation_pipeline_->SetShader(buffer_visualisation_shader_);
	buffer_visualisation_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0, g_buffer_normalized_sampler_);
	buffer_visualisation_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 1, lights_[0]->GetShadowMap()->GetImageViews()[0]);

	buffer_visualisation_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// initialize the skybox
	skybox_ = new Skybox();
	skybox_->Init(devices_, swap_chain_, command_pool_);
}

void VulkanRenderer::InitDeferredPipeline()
{
	// initialize the g buffer sampler
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_NEAREST;
	sampler_info.minFilter = VK_FILTER_NEAREST;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = 1;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_TRUE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &g_buffer_unnormalized_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create g buffer sampler!");
	}

	// initialize the shadow map sampler
	sampler_info.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &shadow_map_sampler_) != VK_SUCCESS ||
		vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &g_buffer_normalized_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create normalized samplers!");
	}

	// initialize the g buffer
	g_buffer_ = new VulkanRenderTarget();
	g_buffer_->Init(devices_, VK_FORMAT_R32G32B32A32_SFLOAT, swap_chain_->GetSwapChainExtent().width, swap_chain_->GetSwapChainExtent().height, 2, false);

	// initialize the g buffer pipeline
	g_buffer_pipeline_ = new GBufferPipeline();
	g_buffer_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	g_buffer_pipeline_->SetShader(g_buffer_shader_);
	g_buffer_pipeline_->SetGBuffer(g_buffer_);
	g_buffer_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// calculate size of the light buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));

	// setup the shadow map descriptor
	std::vector<VkImageView> shadow_maps;
	for (Light* light : lights_)
	{
		shadow_maps.push_back(light->GetShadowMap()->GetImageViews()[0]);
	}

	// initialize the deferred pipeline
	deferred_pipeline_ = new DeferredPipeline();
	deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, light_buffer_, buffer_size);
	deferred_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));

	// add the material textures to the pipeline
	deferred_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 2, default_texture_->GetSampler());
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 3, ambient_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 4, diffuse_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 5, specular_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 6, specular_highlight_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 7, emissive_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 8, normal_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 9, alpha_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 10, reflection_textures_);
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 11, shadow_maps);

	// add the g buffer to the pipeline
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 12, g_buffer_->GetImageViews());
	deferred_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 13, g_buffer_normalized_sampler_);

	// set the deferred shader
	deferred_pipeline_->SetShader(deferred_shader_);
	deferred_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// create the deferred command buffers
	CreateDeferredCommandBuffers();

	// initialize the deferred compute pipeline
	deferred_compute_pipeline_ = new DeferredComputePipeline();
	deferred_compute_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_COMPUTE_BIT, 1, light_buffer_, buffer_size);
	deferred_compute_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_COMPUTE_BIT, 2, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));

	// add the material textures to the pipeline
	deferred_compute_pipeline_->AddSampler(VK_SHADER_STAGE_COMPUTE_BIT, 3, default_texture_->GetSampler());
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 4, ambient_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 5, diffuse_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 6, specular_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 7, specular_highlight_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 8, emissive_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 9, normal_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 10, alpha_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 11, reflection_textures_);
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 12, shadow_maps);

	// add the g buffer to the pipeline
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 13, g_buffer_->GetImageViews());
	deferred_compute_pipeline_->AddSampler(VK_SHADER_STAGE_COMPUTE_BIT, 14, g_buffer_unnormalized_sampler_);
	deferred_compute_pipeline_->AddSampler(VK_SHADER_STAGE_COMPUTE_BIT, 15, shadow_map_sampler_);
	deferred_compute_pipeline_->AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT, 0, swap_chain_->GetIntermediateImageView());

	// set the deferred shader
	deferred_compute_pipeline_->SetComputeShader(deferred_compute_shader_);
	deferred_compute_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// create the deferred compute command buffers
	CreateDeferredComputeCommandBuffers();
}

void VulkanRenderer::InitHDRPipeline()
{

}

void VulkanRenderer::RecreateSwapChainFeatures()
{
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
	int buffer_count = swap_chain_->GetSwapChainImages().size();

	// create rendering command buffers
	command_buffers_.resize(buffer_count);

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = (uint32_t)command_buffers_.size();

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}

	for (size_t i = 0; i < command_buffers_.size(); i++)
	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(command_buffers_[i], &begin_info);

		if (rendering_pipeline_)
		{
			// bind pipeline
			rendering_pipeline_->RecordCommands(command_buffers_[i], i);
			
			for (Mesh* mesh : meshes_)
			{
				mesh->RecordRenderCommands(command_buffers_[i]);
			}

			vkCmdEndRenderPass(command_buffers_[i]);
		}

		if (vkEndCommandBuffer(command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record render command buffer!");
		}
	}

	// create buffer visualisation command buffers
	buffer_visualisation_command_buffers_.resize(buffer_count);

	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = (uint32_t)buffer_visualisation_command_buffers_.size();

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, buffer_visualisation_command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}

	for (size_t i = 0; i < buffer_visualisation_command_buffers_.size(); i++)
	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(buffer_visualisation_command_buffers_[i], &begin_info);

		if (buffer_visualisation_pipeline_)
		{
			// bind pipeline
			buffer_visualisation_pipeline_->RecordCommands(buffer_visualisation_command_buffers_[i], i);

			vkCmdEndRenderPass(buffer_visualisation_command_buffers_[i]);
		}

		if (vkEndCommandBuffer(buffer_visualisation_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record render command buffer!");
		}
	}

	CreateGBufferCommandBuffers();
}

void VulkanRenderer::CreateGBufferCommandBuffers()
{
	// create g buffer command buffers
	g_buffer_command_buffers_.resize(1);

	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = (uint32_t)g_buffer_command_buffers_.size();

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, g_buffer_command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}

	for (size_t i = 0; i < g_buffer_command_buffers_.size(); i++)
	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(g_buffer_command_buffers_[i], &begin_info);

		if (g_buffer_pipeline_)
		{
			// bind pipeline
			g_buffer_pipeline_->RecordCommands(g_buffer_command_buffers_[i], i);

			for (Mesh* mesh : meshes_)
			{
				mesh->RecordRenderCommands(g_buffer_command_buffers_[i]);
			}

			vkCmdEndRenderPass(g_buffer_command_buffers_[i]);
		}

		if (vkEndCommandBuffer(g_buffer_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record render command buffer!");
		}
	}
}

void VulkanRenderer::CreateDeferredComputeCommandBuffers()
{
	int buffer_count = swap_chain_->GetSwapChainImages().size();

	// create rendering command buffers
	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, &deferred_compute_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(deferred_compute_command_buffer_, &begin_info);

	if (deferred_compute_pipeline_)
	{
		// bind pipeline
		deferred_compute_pipeline_->RecordCommands(deferred_compute_command_buffer_, 0);
	}

	if (vkEndCommandBuffer(deferred_compute_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record render command buffer!");
	}
}

void VulkanRenderer::CreateDeferredCommandBuffers()
{
	int buffer_count = swap_chain_->GetSwapChainImages().size();

	// create rendering command buffers
	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, &deferred_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(deferred_command_buffer_, &begin_info);

	if (deferred_pipeline_)
	{
		// bind pipeline
		deferred_pipeline_->RecordCommands(deferred_command_buffer_, 0);

		vkCmdEndRenderPass(deferred_command_buffer_);
	}

	if (vkEndCommandBuffer(deferred_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record render command buffer!");
	}	
}

void VulkanRenderer::CreateMaterialShader(std::string vs_filename, std::string ps_filename)
{
	material_shader_ = new VulkanShader();
	material_shader_->Init(devices_, swap_chain_, vs_filename, "", "", ps_filename);
}

void VulkanRenderer::CreateShaders()
{
	shadow_map_shader_ = new VulkanShader();
	shadow_map_shader_->Init(devices_, swap_chain_, "../res/shaders/shadow_map.vert.spv", "", "", "../res/shaders/shadow_map.frag.spv");

	buffer_visualisation_shader_ = new VulkanShader();
	buffer_visualisation_shader_->Init(devices_, swap_chain_, "../res/shaders/buffer_visualisation.vert.spv", "", "", "../res/shaders/buffer_visualisation.frag.spv");

	g_buffer_shader_ = new VulkanShader();
	g_buffer_shader_->Init(devices_, swap_chain_, "../res/shaders/g_buffer.vert.spv", "", "", "../res/shaders/g_buffer.frag.spv");
	
	deferred_shader_ = new VulkanShader();
	deferred_shader_->Init(devices_, swap_chain_, "../res/shaders/deferred.vert.spv", "", "", "../res/shaders/deferred.frag.spv");

	deferred_compute_shader_ = new VulkanComputeShader();
	deferred_compute_shader_->Init(devices_, swap_chain_, "../res/shaders/deferred.comp.spv");
}

void VulkanRenderer::CreatePrimitiveBuffer()
{
	primitive_buffer_ = new VulkanPrimitiveBuffer();
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
	primitive_buffer_->Init(devices_, Vertex::GetBindingDescription(), attribute_descriptions);
}

void VulkanRenderer::CreateMaterialBuffer()
{
	material_buffer_ = new VulkanMaterialBuffer();
	material_buffer_->InitMaterialBuffer(devices_, sizeof(MaterialData));
}

void VulkanRenderer::AddMesh(Mesh* mesh)
{
	meshes_.push_back(mesh);

	// recreate command buffers
	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, command_buffers_.size(), command_buffers_.data());
	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, buffer_visualisation_command_buffers_.size(), buffer_visualisation_command_buffers_.data());
	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, g_buffer_command_buffers_.size(), g_buffer_command_buffers_.data());
	CreateCommandBuffers();

	// recreate shadow map command buffers
	for (Light* light : lights_)
	{
		light->RecordShadowMapCommands(command_pool_, meshes_);
	}
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

		// recreate command buffers
		vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool_, command_buffers_.size(), command_buffers_.data());
		CreateCommandBuffers();
	}
}

void VulkanRenderer::AddLight(Light* light)
{
	light->SetLightBufferIndex(lights_.size());
	lights_.push_back(light);
}

void VulkanRenderer::RemoveLight(Light* remove_light)
{
	bool found = false;
	auto light_it = lights_.begin();
	for (auto light = lights_.begin(); light != lights_.end(); light++)
	{
		if (*light == remove_light)
		{
			light_it = light;
			found = true;
		}
	}

	if (found)
	{
		lights_.erase(light_it);
	}
}

void VulkanRenderer::CreateSemaphores()
{
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &render_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &g_buffer_semaphore_) != VK_SUCCESS) {

		throw std::runtime_error("failed to create semaphores!");
	}
}

void VulkanRenderer::CreateBuffers()
{
	devices_->CreateBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, matrix_buffer_, matrix_buffer_memory_);
}

void VulkanRenderer::CreateLightBuffer()
{
	// create the lighting data buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));

	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, light_buffer_, light_buffer_memory_);

	SceneLightData light_data = {};
	light_data.scene_data = glm::vec4(glm::vec3(0.1f, 0.1f, 0.1f), lights_.size());
	light_data.camera_pos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	void* mapped_data;
	vkMapMemory(devices_->GetLogicalDevice(), light_buffer_memory_, 0, buffer_size, 0, &mapped_data);
	memcpy(mapped_data, &light_data, sizeof(SceneLightData));
	vkUnmapMemory(devices_->GetLogicalDevice(), light_buffer_memory_);

	rendering_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 2, light_buffer_, buffer_size);

	// setup the shadow map descriptor
	std::vector<VkImageView> shadow_maps;
	for (Light* light : lights_)
	{
		shadow_maps.push_back(light->GetShadowMap()->GetImageViews()[0]);
	}

	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 13, shadow_maps);
}

uint32_t VulkanRenderer::AddTextureMap(Texture* texture, Texture::MapType map_type)
{
	switch (map_type)
	{
	case (Texture::MapType::AMBIENT):
	{
		ambient_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::AMBIENT);
		return ambient_textures_.size();
		break;
	}
	case (Texture::MapType::DIFFUSE):
	{
		diffuse_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::DIFFUSE);
		return diffuse_textures_.size();
		break;
	}
	case (Texture::MapType::SPECULAR):
	{
		specular_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::SPECULAR);
		return specular_textures_.size();
		break;
	}
	case (Texture::MapType::SPECULAR_HIGHLIGHT):
	{
		specular_highlight_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::SPECULAR_HIGHLIGHT);
		return specular_highlight_textures_.size();
		break;
	}
	case (Texture::MapType::EMISSIVE):
	{
		emissive_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::EMISSIVE);
		return emissive_textures_.size();
		break;
	}
	case (Texture::MapType::NORMAL):
	{
		normal_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::NORMAL);
		return normal_textures_.size();
		break;
	}
	case (Texture::MapType::ALPHA):
	{
		alpha_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::ALPHA);
		return alpha_textures_.size();
		break;
	}
	case (Texture::MapType::REFLECTION):
	{
		reflection_textures_.push_back(texture);
		texture->AddMapType(Texture::MapType::REFLECTION);
		return reflection_textures_.size();
		break;
	}
	}

	return 0;
}