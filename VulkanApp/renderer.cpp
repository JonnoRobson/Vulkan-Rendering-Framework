#include "renderer.h"
#include <chrono>
#include <iostream>
#include <array>
#include <map>

void VulkanRenderer::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, int multisample_level)
{
	devices_ = devices;
	swap_chain_ = swap_chain;
	render_mode_ = RenderMode::VISIBILITY_PEELED;
	timing_enabled_ = false;
	multisample_level_ = multisample_level;

	// load a default texture
	default_texture_ = new Texture();
	default_texture_->Init(devices, "../res/textures/default.png", true);

	// create the texture cache
	texture_cache_ = new VulkanTextureCache(devices);

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
	// set the render semaphore as the signal semaphore
	current_signal_semaphore_ = render_semaphore_;

	// regenerate the shadow map for any moving light
	for (Light* light : lights_)
	{
		if (!light->GetLightStationary() && light->GetShadowsEnabled())
			light->GenerateShadowMap(command_pool_, meshes_);
	}

	// get swap chain index
	uint32_t image_index = swap_chain_->GetCurrentSwapChainImage();
	VkExtent2D swap_extent = swap_chain_->GetSwapChainExtent();


	if (render_mode_ == RenderMode::BUFFER_VIS)
	{
		RenderVisualisation(image_index);
	}
	else
	{
		// send matrix data to the gpu
		UniformBufferObject ubo = {};
		ubo.model = glm::mat4(1.0f);
		ubo.view = render_camera_->GetViewMatrix();
		ubo.proj = render_camera_->GetProjectionMatrix();

		devices_->CopyDataToBuffer(matrix_buffer_memory_, &ubo, sizeof(UniformBufferObject));

		// send camera data to the gpu
		SceneLightData scene_data = {};
		scene_data.scene_data = glm::vec4(glm::vec3(0.1f, 0.1f, 0.1f), lights_.size());
		//scene_data.scene_data = glm::vec4(glm::vec3(0.85f * 0.5f, 0.68f * 0.5f, 0.92f * 0.5f), lights_.size());
		scene_data.camera_data = glm::vec4(render_camera_->GetPosition(), 1000.0f);
		devices_->CopyDataToBuffer(light_buffer_memory_, &scene_data, sizeof(SceneLightData));


		for (Light* light : lights_)
		{
			// send the light data to the gpu
			light->SendLightData(devices_, light_buffer_memory_);
		}

		// render the skybox
		skybox_->Render(render_camera_);

		// cull the scene geometry
		CullGeometry();

		// render the scene

#ifdef _VISIBILITY

		// send data to the visibility data buffer
		VisibilityRenderData visibility_data = {};
		visibility_data.screen_dimensions = glm::vec4(swap_extent.width, swap_extent.height, 0, 0);
		visibility_data.invView = glm::inverse(render_camera_->GetViewMatrix());
		visibility_data.invProj = glm::inverse(render_camera_->GetProjectionMatrix());
		devices_->CopyDataToBuffer(visibility_data_buffer_memory_, &visibility_data, sizeof(VisibilityRenderData));

		if (timing_enabled_)
		{
			auto vis_begin = std::chrono::high_resolution_clock::now();

			RenderVisibility();

			vkQueueWaitIdle(graphics_queue_);
			auto vis_end = std::chrono::high_resolution_clock::now();
			float vis_time = std::chrono::duration_cast<std::chrono::milliseconds>(vis_end - vis_begin).count();
			std::cout << "Visibility took " << vis_time << "ms.\n";

			auto shade_begin = std::chrono::high_resolution_clock::now();

			RenderVisbilityDeferred();

			vkQueueWaitIdle(graphics_queue_);
			auto shade_end = std::chrono::high_resolution_clock::now();
			float shade_time = std::chrono::duration_cast<std::chrono::milliseconds>(shade_end - shade_begin).count();
			std::cout << "Shading took " << shade_time << "ms.\n";

			// render transparency
			auto transparency_begin = std::chrono::high_resolution_clock::now();

			current_signal_semaphore_ = transparency_composite_semaphore_;
			RenderTransparency();

			vkQueueWaitIdle(graphics_queue_);
			auto transparency_end = std::chrono::high_resolution_clock::now();
			float transparency_time = std::chrono::duration_cast<std::chrono::milliseconds>(transparency_end - transparency_begin).count();
			std::cout << "Transparency took " << transparency_time << "ms.\n";

			std::cout << "Total shading took " << vis_time + shade_time + transparency_time << "ms.\n\n\n";

			timing_enabled_ = false;
		}
		else
		{
			RenderVisibility();
			RenderVisbilityDeferred();
			current_signal_semaphore_ = transparency_composite_semaphore_;
			RenderTransparency();
		}

#elif _VISIBILITY_PEELED

		// send data to the visibility data buffer
		VisibilityPeelRenderData visibility_data = {};
		visibility_data.screen_dimensions = glm::vec4(swap_extent.width, swap_extent.height, 0, 0);
		visibility_data.invView = glm::inverse(render_camera_->GetViewMatrix());
		visibility_data.invProj = glm::inverse(render_camera_->GetProjectionMatrix());
		devices_->CopyDataToBuffer(visibility_data_buffer_memory_, &visibility_data, sizeof(VisibilityPeelRenderData));
	
		if (timing_enabled_)
		{
			// render visibility
			auto vis_begin = std::chrono::high_resolution_clock::now();

			RenderVisibilityPeel();

			vkQueueWaitIdle(graphics_queue_);
			auto vis_end = std::chrono::high_resolution_clock::now();
			float vis_time = std::chrono::duration_cast<std::chrono::milliseconds>(vis_end - vis_begin).count();
			std::cout << "Visibility took " << vis_time << "ms.\n";

			// render deferred stage
			auto shade_begin = std::chrono::high_resolution_clock::now();

			RenderVisibilityPeelDeferred();

			vkQueueWaitIdle(graphics_queue_);
			auto shade_end = std::chrono::high_resolution_clock::now();
			float shade_time = std::chrono::duration_cast<std::chrono::milliseconds>(shade_end - shade_begin).count();
			std::cout << "Shading took " << shade_time << "ms.\n";

			std::cout << "Total shading took " << vis_time + shade_time << "ms.\n\n\n";

			timing_enabled_ = false;
		}
		else
		{
			RenderVisibilityPeel();
			RenderVisibilityPeelDeferred();
		}

#elif _DEFERRED

		if (timing_enabled_)
		{
			// render g buffer
			auto vis_begin = std::chrono::high_resolution_clock::now();

			RenderGBuffer();

			vkQueueWaitIdle(graphics_queue_);
			auto vis_end = std::chrono::high_resolution_clock::now();
			float vis_time = std::chrono::duration_cast<std::chrono::milliseconds>(vis_end - vis_begin).count();
			std::cout << "Visibility took " << vis_time << "ms.\n";

			// render deferred stage
			auto shade_begin = std::chrono::high_resolution_clock::now();

			RenderDeferred();

			vkQueueWaitIdle(graphics_queue_);
			auto shade_end = std::chrono::high_resolution_clock::now();
			float shade_time = std::chrono::duration_cast<std::chrono::milliseconds>(shade_end - shade_begin).count();
			std::cout << "Shading took " << shade_time << "ms.\n";

			// render transparency
			auto transparency_begin = std::chrono::high_resolution_clock::now();

			current_signal_semaphore_ = transparency_composite_semaphore_;
			RenderTransparency();

			vkQueueWaitIdle(graphics_queue_);
			auto transparency_end = std::chrono::high_resolution_clock::now();
			float transparency_time = std::chrono::duration_cast<std::chrono::milliseconds>(transparency_end - transparency_begin).count();
			std::cout << "Transparency took " << transparency_time << "ms.\n";

			std::cout << "Total shading took " << vis_time + shade_time + transparency_time << "ms.\n\n\n";

			timing_enabled_ = false;
		}
		else
		{
			RenderGBuffer();
			RenderDeferred();
			current_signal_semaphore_ = transparency_composite_semaphore_;
			RenderTransparency();
		}

#endif
		
		if (hdr_->GetHDRMode() > 0)
		{
			hdr_->Render(swap_chain_, &current_signal_semaphore_);
				current_signal_semaphore_ = hdr_->GetHDRSemaphore();
		}
		
		swap_chain_->FinalizeIntermediateImage();
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

void VulkanRenderer::RenderGBuffer()
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

void VulkanRenderer::RenderDeferred()
{
	VkSemaphore skybox_semaphore = skybox_->GetRenderSemaphore();

	//  transition the intermediate image to color write optimal layout
	devices_->TransitionImageLayout(swap_chain_->GetIntermediateImage(), swap_chain_->GetIntermediateImageFormat(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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
}

void VulkanRenderer::RenderDeferredCompute()
{
	VkSemaphore skybox_semaphore = skybox_->GetRenderSemaphore();

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = { g_buffer_semaphore_, skybox_semaphore, swap_chain_->GetImageAvailableSemaphore() };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 3;
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
}

void VulkanRenderer::RenderVisibility()
{
	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &visibility_command_buffer_;

	VkSemaphore signal_semaphores[] = { g_buffer_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}
}

void VulkanRenderer::RenderVisbilityDeferred()
{
	VkSemaphore skybox_semaphore = skybox_->GetRenderSemaphore();

	//  transition the intermediate image to color write optimal layout
	devices_->TransitionImageLayout(swap_chain_->GetIntermediateImage(), swap_chain_->GetIntermediateImageFormat(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore wait_semaphores[] = { g_buffer_semaphore_, skybox_semaphore };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 2;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &visibility_deferred_command_buffer_;

	VkSemaphore signal_semaphores[] = { render_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}
}

void VulkanRenderer::RenderVisibilityPeel()
{
	// clear the peel depth buffer manually since it is need out of order
	peel_depth_buffer_->ClearDepthImage({ 0.0f, 0 }, VISIBILITY_PEEL_COUNT - 1);

	// set up generic draw info
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;

	VkSemaphore signal_semaphores[] = { g_buffer_semaphore_ };

	// submit the visibility layer peel pipelines
	for (int i = 0; i < VISIBILITY_PEEL_COUNT; i++)
	{
		// cheap wait for the previous pass to complete
		vkQueueWaitIdle(graphics_queue_);

		// submit the draw command buffer
		submit_info.pCommandBuffers = &visibility_peel_command_buffers_[i];

		if (i == VISIBILITY_PEEL_COUNT - 1)
		{
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = signal_semaphores;
		}
		else
		{
			submit_info.signalSemaphoreCount = 0;
			submit_info.pSignalSemaphores = nullptr;
		}

		VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit visibility peel command buffer!");
		}
	}
}

void VulkanRenderer::RenderVisibilityPeelDeferred()
{
	VkSemaphore skybox_semaphore = skybox_->GetRenderSemaphore();

	//  transition the intermediate image to color write optimal layout
	devices_->TransitionImageLayout(swap_chain_->GetIntermediateImage(), swap_chain_->GetIntermediateImageFormat(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// submit the draw command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore wait_semaphores[] = { g_buffer_semaphore_, skybox_semaphore };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 2;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &visibility_peel_deferred_command_buffer_;

	VkSemaphore signal_semaphores[] = { render_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit deferred command buffer!");
	}
}

void VulkanRenderer::RenderTransparency()
{
	VkSemaphore wait_semaphores[] = { render_semaphore_ };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	// clear the transparency buffers
	VkClearColorValue revealage_clear = { 1.f, 1.f, 1.f, 1.f };
	accumulation_buffer_->ClearImage();
	revealage_buffer_->ClearImage(revealage_clear);

	// submit the transparency render command buffer
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &transparency_command_buffer_;

	VkSemaphore signal_semaphores[] = { transparency_semaphore_ };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit transparency command buffer!");
	}

	// submit the transparency composite command buffer
	wait_semaphores[0] = transparency_semaphore_;
	submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &transparency_composite_command_buffer_;

	signal_semaphores[0] = transparency_composite_semaphore_;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	result = vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit transparency composite command buffer!");
	}
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

void VulkanRenderer::CullGeometry()
{
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &shape_culling_command_buffer_;

	VkResult result = vkQueueSubmit(compute_queue_, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit shape culling command buffer!");
	}

	vkQueueWaitIdle(compute_queue_);
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

	shape_culling_shader_->Cleanup();
	delete shape_culling_shader_;
	shape_culling_shader_ = nullptr;

	shadow_map_shader_->Cleanup();
	delete shadow_map_shader_;
	shadow_map_shader_ = nullptr;
		
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
	
	// clean up the buffer visualisation pipeline
	buffer_visualisation_pipeline_->CleanUp();
	delete buffer_visualisation_pipeline_;
	buffer_visualisation_pipeline_ = nullptr;
	
	// clean up the shape culling pipeline
	shape_culling_pipeline_->CleanUp();
	delete shape_culling_pipeline_;
	shape_culling_pipeline_ = nullptr;

#ifdef _DEFERRED
	CleanupDeferredPipeline();
	CleanupTransparencyPipeline();
#elif _VISIBILITY
	CleanupVisibilityPipeline();
	CleanupTransparencyPipeline();
#elif _VISIBILITY_PEELED
	CleanupVisibilityPeelPipeline();
#endif

	// clean up the skybox
	skybox_->Cleanup();
	delete skybox_;
	skybox_ = nullptr;

	// clean up the hdr renderer
	hdr_->Cleanup();
	delete hdr_;
	hdr_ = nullptr;

	// clean up default texture
	default_texture_->Cleanup();
	delete default_texture_;
	default_texture_ = nullptr;

	// clean up samplers
	vkDestroySampler(devices_->GetLogicalDevice(), buffer_normalized_sampler_, nullptr);
	vkDestroySampler(devices_->GetLogicalDevice(), buffer_unnormalized_sampler_, nullptr);
	vkDestroySampler(devices_->GetLogicalDevice(), shadow_map_sampler_, nullptr);

	// clean up semaphores
	vkDestroySemaphore(devices_->GetLogicalDevice(), render_semaphore_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), transparency_semaphore_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), g_buffer_semaphore_, nullptr);
	vkDestroySemaphore(devices_->GetLogicalDevice(), transparency_composite_semaphore_, nullptr);
}

void VulkanRenderer::CleanupForwardPipeline()
{

}

void VulkanRenderer::CleanupDeferredPipeline()
{
	// clean up shaders
	g_buffer_shader_->Cleanup();
	delete g_buffer_shader_;
	g_buffer_shader_ = nullptr;

	deferred_shader_->Cleanup();
	delete deferred_shader_;
	deferred_shader_ = nullptr;

	// clean up g buffer
	g_buffer_->Cleanup();
	delete g_buffer_;
	g_buffer_ = nullptr;

	// clean up deferred rendering pipelines
	g_buffer_pipeline_->CleanUp();
	delete g_buffer_pipeline_;
	g_buffer_pipeline_ = nullptr;

	deferred_pipeline_->CleanUp();
	delete deferred_pipeline_;
	deferred_pipeline_ = nullptr;
}

void VulkanRenderer::CleanupVisibilityPipeline()
{
	// clean up buffers
	vkDestroyBuffer(devices_->GetLogicalDevice(), visibility_data_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), visibility_data_buffer_memory_, nullptr);

	// clean up shaders
	visibility_shader_->Cleanup();
	delete visibility_shader_;
	visibility_shader_ = nullptr;

	visibility_deferred_shader_->Cleanup();
	delete visibility_deferred_shader_;
	visibility_deferred_shader_ = nullptr;

	// clean up visibility buffer
	visibility_buffer_->Cleanup();
	delete visibility_buffer_;
	visibility_buffer_ = nullptr;

	// clean up the visibility rendering pipelines
	visibility_pipeline_->CleanUp();
	delete visibility_pipeline_;
	visibility_pipeline_ = nullptr;

	visibility_deferred_pipeline_->CleanUp();
	delete visibility_deferred_pipeline_;
	visibility_deferred_pipeline_ = nullptr;
}

void VulkanRenderer::CleanupVisibilityPeelPipeline()
{
	// cleanup shaders
	visibility_peel_shader_->Cleanup();
	delete visibility_peel_shader_;
	visibility_peel_shader_ = nullptr;

	visibility_peel_deferred_shader_->Cleanup();
	delete visibility_peel_deferred_shader_;
	visibility_deferred_shader_ = nullptr;

	// clean up visibility peel pipelines
	for (int i = 0; i < visibility_peel_pipelines_.size(); i++)
	{
		visibility_peel_pipelines_[i]->CleanUp();
		delete visibility_peel_pipelines_[i];
	}
	visibility_peel_pipelines_.clear();

	// clean up visibility deferred pipeline
	visibility_peel_deferred_pipeline_->CleanUp();
	delete visibility_peel_deferred_pipeline_;
	visibility_peel_deferred_pipeline_ = nullptr;

	// clean up visibility peel resources
	visibility_peel_buffer_->Cleanup();
	delete visibility_peel_buffer_;
	visibility_peel_buffer_ = nullptr;

	peel_depth_buffer_->Cleanup();
	delete peel_depth_buffer_;
	peel_depth_buffer_ = nullptr;

	vkDestroyBuffer(devices_->GetLogicalDevice(), visibility_data_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), visibility_data_buffer_memory_, nullptr);
}

void VulkanRenderer::CleanupTransparencyPipeline()
{
	// cleanup shaders
	transparency_shader_->Cleanup();
	delete transparency_shader_;
	transparency_shader_ = nullptr;

	transparency_composite_shader_->Cleanup();
	delete transparency_composite_shader_;
	transparency_composite_shader_ = nullptr;

	// clean up transparency pipelines
	transparency_pipeline_->CleanUp();
	delete transparency_pipeline_;
	transparency_pipeline_ = nullptr;

	transparency_composite_pipeline_->CleanUp();
	delete transparency_composite_pipeline_;
	transparency_composite_pipeline_ = nullptr;

	// clean up transparency buffers
	accumulation_buffer_->Cleanup();
	delete accumulation_buffer_;
	accumulation_buffer_ = nullptr;

	revealage_buffer_->Cleanup();
	delete revealage_buffer_;
	revealage_buffer_ = nullptr;
}

void VulkanRenderer::InitPipelines()
{
	CreateLightBuffer();

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

	// initialize the buffer samplers
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_NEAREST;
	sampler_info.minFilter = VK_FILTER_NEAREST;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = 1;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler_info.unnormalizedCoordinates = VK_TRUE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &buffer_unnormalized_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create g buffer sampler!");
	}

	// initialize the shadow map sampler
	sampler_info.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &shadow_map_sampler_) != VK_SUCCESS ||
		vkCreateSampler(devices_->GetLogicalDevice(), &sampler_info, nullptr, &buffer_normalized_sampler_) != VK_SUCCESS) {
		throw std::runtime_error("failed to create normalized samplers!");
	}

	// initialize the shape buffer now that we know how many there are
	primitive_buffer_->InitShapeBuffer(devices_);

	// init rendering pipelines
#ifdef _DEFERRED
	InitDeferredPipeline();
	InitTransparencyPipeline();
#elif _VISIBILITY
	InitVisibilityPipeline();
	InitTransparencyPipeline();
#elif _VISIBILITY_PEELED
	InitVisibilityPeelPipeline();
#endif

	// initialize the skybox
	skybox_ = new Skybox();
	skybox_->Init(devices_, swap_chain_, command_pool_);

	// initialize the hdr renderer
	hdr_ = new HDR();
	hdr_->Init(devices_, swap_chain_, command_pool_);

	// initialize a buffer visualisation pipeline
	buffer_visualisation_pipeline_ = new BufferVisualisationPipeline();
	buffer_visualisation_pipeline_->SetShader(buffer_visualisation_shader_);
	buffer_visualisation_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0, buffer_normalized_sampler_);
	buffer_visualisation_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 1, swap_chain_->GetDepthImageView());
	buffer_visualisation_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// initialize the shape culling pipeline
	shape_culling_pipeline_ = new ShapeCullingPipeline();
	shape_culling_pipeline_->SetShader(shape_culling_shader_);
	shape_culling_pipeline_->AddStorageBuffer(0, primitive_buffer_->GetIndirectDrawBuffer(), primitive_buffer_->GetShapeCount() * sizeof(IndirectDrawCommand));
	shape_culling_pipeline_->AddStorageBuffer(1, primitive_buffer_->GetShapeBuffer(), primitive_buffer_->GetShapeCount() * sizeof(ShapeData));
	shape_culling_pipeline_->AddUniformBuffer(2, matrix_buffer_, sizeof(UniformBufferObject));
	shape_culling_pipeline_->SetShapeCount(primitive_buffer_->GetShapeCount());
	shape_culling_pipeline_->Init(devices_);

	CreateCommandBuffers();
}

void VulkanRenderer::InitForwardPipeline()
{
	// calculate size of the light buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));

	// add the material buffers to the pipeline
	rendering_pipeline_ = new VulkanPipeline();
	rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	rendering_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 2, light_buffer_, buffer_size);
	rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 3, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));

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
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 13, shadow_maps_);

	// set the pipeline material shader
	rendering_pipeline_->SetShader(material_shader_);

	// create the material pipeline
	rendering_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

}

void VulkanRenderer::InitDeferredPipeline()
{
	// create the deferred rendering shaders
	g_buffer_shader_ = new VulkanShader();
	g_buffer_shader_->Init(devices_, swap_chain_, "../res/shaders/g_buffer.vert.spv", "", "", "../res/shaders/g_buffer.frag.spv");

	deferred_shader_ = new VulkanShader();
	deferred_shader_->Init(devices_, swap_chain_, "../res/shaders/deferred.vert.spv", "", "", multisample_data[multisample_level_].deferred_shader);
	
	// initialize the g buffer
	g_buffer_ = new VulkanRenderTarget();
	g_buffer_->Init(devices_, VK_FORMAT_R32G32B32A32_SFLOAT, swap_chain_->GetSwapChainExtent().width, swap_chain_->GetSwapChainExtent().height, 2, false, multisample_data[multisample_level_].sample_count);

	// initialize the g buffer pipeline
	g_buffer_pipeline_ = new GBufferPipeline();
	g_buffer_pipeline_->SetShader(g_buffer_shader_);
	g_buffer_pipeline_->SetGBuffer(g_buffer_);
	g_buffer_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	// material buffer and alpha maps are required to test for transparent pixels
	g_buffer_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));
	g_buffer_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 2, buffer_normalized_sampler_);
	g_buffer_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 3, alpha_textures_);
	g_buffer_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// calculate size of the light buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));
	
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
	deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 11, shadow_maps_);

	// add the g buffer to the pipeline
	deferred_pipeline_->AddStorageImageArray(VK_SHADER_STAGE_FRAGMENT_BIT, 12, g_buffer_->GetImageViews(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	deferred_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 13, buffer_normalized_sampler_);

	// set the deferred shader
	deferred_pipeline_->SetShader(deferred_shader_);
	deferred_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// create the deferred command buffers
	CreateDeferredCommandBuffers();
}

void VulkanRenderer::InitDeferredComputePipeline()
{
	// calculate size of the light buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));

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
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 12, shadow_maps_);

	// add the g buffer to the pipeline
	deferred_compute_pipeline_->AddTextureArray(VK_SHADER_STAGE_COMPUTE_BIT, 13, g_buffer_->GetImageViews());
	deferred_compute_pipeline_->AddSampler(VK_SHADER_STAGE_COMPUTE_BIT, 14, buffer_unnormalized_sampler_);
	deferred_compute_pipeline_->AddSampler(VK_SHADER_STAGE_COMPUTE_BIT, 15, shadow_map_sampler_);
	deferred_compute_pipeline_->AddStorageImage(VK_SHADER_STAGE_COMPUTE_BIT, 0, swap_chain_->GetIntermediateImageView());

	// set the deferred shader
	deferred_compute_pipeline_->SetComputeShader(deferred_compute_shader_);
	deferred_compute_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// create the deferred compute command buffers
	CreateDeferredComputeCommandBuffers();
}

void VulkanRenderer::InitVisibilityPipeline()
{
	// create the visibility rendering shaders
	visibility_shader_ = new VulkanShader();
	visibility_shader_->Init(devices_, swap_chain_, "../res/shaders/visibility.vert.spv", "", "", "../res/shaders/visibility.frag.spv");

	visibility_deferred_shader_ = new VulkanShader();
	visibility_deferred_shader_->Init(devices_, swap_chain_, "../res/shaders/screen_space.vert.spv", "", "", multisample_data[multisample_level_].visibility_deferred_shader);

	// create the visibility data buffer
	devices_->CreateBuffer(sizeof(VisibilityRenderData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, visibility_data_buffer_, visibility_data_buffer_memory_);

	// calculate size of the light buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));

	// initialize the visibility buffer
	VkExtent2D swap_size = swap_chain_->GetSwapChainExtent();
	visibility_buffer_ = new VulkanRenderTarget();
	visibility_buffer_->Init(devices_, VK_FORMAT_R32_UINT, swap_size.width, swap_size.height, 1, false, multisample_data[multisample_level_].sample_count);

	// initialize the visibility buffer generation pipeline
	visibility_pipeline_ = new VisibilityPipeline();
	visibility_pipeline_->SetShader(visibility_shader_);
	visibility_pipeline_->SetVisibilityBuffer(visibility_buffer_);
	visibility_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	visibility_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));
	visibility_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 2, buffer_normalized_sampler_);
	visibility_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 3, alpha_textures_);
	visibility_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// initialize the deferred pipeline
	visibility_deferred_pipeline_ = new VisibilityDeferredPipeline();
	visibility_deferred_pipeline_->SetShader(visibility_deferred_shader_);
	visibility_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, light_buffer_, buffer_size);
	visibility_deferred_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));

	// add the material textures to the pipeline
	visibility_deferred_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 2, default_texture_->GetSampler());
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 3, ambient_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 4, diffuse_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 5, specular_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 6, specular_highlight_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 7, emissive_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 8, normal_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 9, alpha_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 10, reflection_textures_);
	visibility_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 11, shadow_maps_);

	// add the visibility buffer to the pipeline
	visibility_deferred_pipeline_->AddStorageImage(VK_SHADER_STAGE_FRAGMENT_BIT, 12, visibility_buffer_->GetImageViews()[0]);
	visibility_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 13, primitive_buffer_->GetVertexBuffer(), primitive_buffer_->GetVertexCount() * sizeof(Vertex));
	visibility_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 14, primitive_buffer_->GetIndexBuffer(), primitive_buffer_->GetIndexCount() * sizeof(uint32_t));
	visibility_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 15, primitive_buffer_->GetShapeBuffer(), primitive_buffer_->GetShapeCount() * sizeof(ShapeData));
	visibility_deferred_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 16, visibility_data_buffer_, sizeof(VisibilityRenderData));
	visibility_deferred_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 17, swap_chain_->GetDepthImageView());
	visibility_deferred_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	CreateVisibilityCommandBuffer();
	CreateVisibilityDeferredCommandBuffer();
}

void VulkanRenderer::InitVisibilityPeelPipeline()
{
	// calculate size of the light buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));

	// create the visibility peel shaders
	visibility_peel_shader_ = new VulkanShader();
	visibility_peel_shader_->Init(devices_, swap_chain_, "../res/shaders/visibility_front_peel.vert.spv", "", "", "../res/shaders/visibility_front_peel.frag.spv");

	visibility_peel_deferred_shader_ = new VulkanShader();
	visibility_peel_deferred_shader_->Init(devices_, swap_chain_, "../res/shaders/screen_space.vert.spv", "", "", multisample_data[multisample_level_].visibility_peel_deferred_shader);

	// initalize the peeled visibility buffer
	VkExtent2D swap_size = swap_chain_->GetSwapChainExtent();
	visibility_peel_buffer_ = new VulkanRenderTarget();
	visibility_peel_buffer_->Init(devices_, VK_FORMAT_R32_UINT, swap_size.width, swap_size.height, VISIBILITY_PEEL_COUNT, false, multisample_data[multisample_level_].sample_count);

	// initialize the min max depth buffer
	peel_depth_buffer_ = new VulkanRenderTarget();
	peel_depth_buffer_->Init(devices_, VK_FORMAT_D32_SFLOAT, swap_size.width, swap_size.height, VISIBILITY_PEEL_COUNT, false);
	
	std::vector<VkImageView> visibility_peels = visibility_peel_buffer_->GetImageViews();
	std::vector<VkImageView> depth_peels = peel_depth_buffer_->GetImageViews();

	// create the visibility peel pipelines
	visibility_peel_pipelines_.resize(VISIBILITY_PEEL_COUNT);
	for (int i = 0; i < VISIBILITY_PEEL_COUNT; i++)
	{
		visibility_peel_pipelines_[i] = new VisibilityFrontPeelPipeline();
		visibility_peel_pipelines_[i]->SetShader(visibility_peel_shader_);

		// add the resources to the visibility peel pipelines
		visibility_peel_pipelines_[i]->SetOutputBuffers(depth_peels[i], visibility_peels[i]);
		visibility_peel_pipelines_[i]->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
		visibility_peel_pipelines_[i]->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));
		visibility_peel_pipelines_[i]->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 2, buffer_normalized_sampler_);
		visibility_peel_pipelines_[i]->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 3, alpha_textures_);
		if (i == 0)
			visibility_peel_pipelines_[i]->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 4, depth_peels[VISIBILITY_PEEL_COUNT - 1], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		else
			visibility_peel_pipelines_[i]->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 4, depth_peels[i - 1], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		visibility_peel_pipelines_[i]->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 5, buffer_unnormalized_sampler_);

		// initialize the visibility peel pipelines
		visibility_peel_pipelines_[i]->Init(devices_, swap_chain_, primitive_buffer_);
	}

	// create the visibility data buffer
	devices_->CreateBuffer(sizeof(VisibilityPeelRenderData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, visibility_data_buffer_, visibility_data_buffer_memory_);

	// create the visibility peel deferred pipelines
	visibility_peel_deferred_pipeline_ = new VisibilityPeelDeferredPipeline();
	visibility_peel_deferred_pipeline_->SetShader(visibility_peel_deferred_shader_);

	// add the resources to the visibility peel deferred pipeline	
	visibility_peel_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, light_buffer_, buffer_size);
	visibility_peel_deferred_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 1, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));

	// add the material textures to the pipeline
	visibility_peel_deferred_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 2, default_texture_->GetSampler());
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 3, ambient_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 4, diffuse_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 5, specular_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 6, specular_highlight_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 7, emissive_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 8, normal_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 9, alpha_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 10, reflection_textures_);
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 11, shadow_maps_);

	// add visibility rendering resources to the pipeline
	visibility_peel_deferred_pipeline_->AddStorageImageArray(VK_SHADER_STAGE_FRAGMENT_BIT, 12, visibility_peel_buffer_->GetImageViews());
	visibility_peel_deferred_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 13, peel_depth_buffer_->GetImageViews());
	visibility_peel_deferred_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 14, buffer_unnormalized_sampler_);
	visibility_peel_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 15, primitive_buffer_->GetVertexBuffer(), primitive_buffer_->GetVertexCount() * sizeof(Vertex));
	visibility_peel_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 16, primitive_buffer_->GetIndexBuffer(), primitive_buffer_->GetIndexCount() * sizeof(uint32_t));
	visibility_peel_deferred_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 17, primitive_buffer_->GetShapeBuffer(), primitive_buffer_->GetShapeCount() * sizeof(ShapeData));
	visibility_peel_deferred_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 18, visibility_data_buffer_, sizeof(VisibilityRenderData));
	visibility_peel_deferred_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	CreateVisibilityPeelCommandBuffers();
	CreateVisibilityPeelDeferredCommandBuffers();
}

void VulkanRenderer::InitTransparencyPipeline()
{
	// create the tranparency rendering shaders
	transparency_shader_ = new VulkanShader();
	transparency_shader_->Init(devices_, swap_chain_, "../res/shaders/default_material.vert.spv", "", "", "../res/shaders/weighted_blended_transparency.frag.spv");

	transparency_composite_shader_ = new VulkanShader();
	transparency_composite_shader_->Init(devices_, swap_chain_, "../res/shaders/screen_space.vert.spv", "", "", "../res/shaders/transparency_composite.frag.spv");

	VkExtent2D image_extent = swap_chain_->GetSwapChainExtent();

	// calculate size of the light buffer
	VkDeviceSize buffer_size = sizeof(SceneLightData) + (lights_.size() * sizeof(LightData));

	// create the transparency buffers
	accumulation_buffer_ = new VulkanRenderTarget();
	accumulation_buffer_->Init(devices_, VK_FORMAT_R16G16B16A16_SFLOAT, image_extent.width, image_extent.height, 1, false);
	revealage_buffer_ = new VulkanRenderTarget();
	revealage_buffer_->Init(devices_, VK_FORMAT_R16_SFLOAT, image_extent.width, image_extent.height, 1, false);

	// create the transparency pipeline
	transparency_pipeline_ = new WeightedBlendedTransparencyPipeline();
	transparency_pipeline_->SetShader(transparency_shader_);
	transparency_pipeline_->SetRenderTargets(accumulation_buffer_, revealage_buffer_);

	// add resources to the pipeline
	transparency_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	transparency_pipeline_->AddStorageBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 2, light_buffer_, buffer_size);
	transparency_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 3, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));
	transparency_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 4, default_texture_->GetSampler());
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 5, ambient_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 6, diffuse_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 7, specular_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 8, specular_highlight_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 9, emissive_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 10, normal_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 11, alpha_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 12, reflection_textures_);
	transparency_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 13, shadow_maps_);

	// initialize the transparency pipeline
	transparency_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);

	// create the composite pipeline
	transparency_composite_pipeline_ = new TransparencyCompositePipeline();
	transparency_composite_pipeline_->SetShader(transparency_composite_shader_);

	// add resources to the pipeline
	transparency_composite_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 0, buffer_unnormalized_sampler_);
	transparency_composite_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 1, accumulation_buffer_->GetImageViews()[0]);
	transparency_composite_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 2, revealage_buffer_->GetImageViews()[0]);

	// initialize the transparency composite pipeline
	transparency_composite_pipeline_->Init(devices_, swap_chain_, nullptr);

	CreateTransparencyCompositeCommandBuffer();
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
	CreateGBufferCommandBuffers();
	CreateVisibilityCommandBuffer();
	CreateTransparencyCommandBuffer();
	CreateBufferVisualisationCommandBuffers();
	CreateCullingCommandBuffer();
}

void VulkanRenderer::CreateForwardCommandBuffers()
{
	int buffer_count = swap_chain_->GetSwapChainImages().size();

	// create rendering command buffers
	command_buffers_.resize(buffer_count);
	devices_->CreateCommandBuffers(command_pool_, command_buffers_.data(), buffer_count);

	// record the command buffers
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
				mesh->RecordRenderCommands(command_buffers_[i], RenderStage::GENERIC);
			}

			vkCmdEndRenderPass(command_buffers_[i]);
		}

		if (vkEndCommandBuffer(command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record render command buffer!");
		}
	}
}

void VulkanRenderer::CreateGBufferCommandBuffers()
{
	// create g buffer command buffers
	g_buffer_command_buffers_.resize(1);
	devices_->CreateCommandBuffers(command_pool_, g_buffer_command_buffers_.data(), g_buffer_command_buffers_.size());

	// record the command buffers
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

			// record draw commands
			primitive_buffer_->RecordIndirectDrawCommands(g_buffer_command_buffers_[i]);

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
	// create rendering command buffers
	devices_->CreateCommandBuffers(command_pool_, &deferred_compute_command_buffer_);

	// record the command buffer
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
	// create rendering command buffers
	devices_->CreateCommandBuffers(command_pool_, &deferred_command_buffer_);

	// record  the command buffer
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

void VulkanRenderer::CreateVisibilityCommandBuffer()
{
	// create rendering command buffer
	devices_->CreateCommandBuffers(command_pool_, &visibility_command_buffer_);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(visibility_command_buffer_, &begin_info);

	if (visibility_pipeline_)
	{
		// bind pipeline
		visibility_pipeline_->RecordCommands(visibility_command_buffer_, 0);

		primitive_buffer_->RecordIndirectDrawCommands(visibility_command_buffer_);

		vkCmdEndRenderPass(visibility_command_buffer_);
	}

	if (vkEndCommandBuffer(visibility_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record render command buffer!");
	}
}

void VulkanRenderer::CreateVisibilityDeferredCommandBuffer()
{
	// create rendering command buffers
	devices_->CreateCommandBuffers(command_pool_, &visibility_deferred_command_buffer_);

	// record the command buffer
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(visibility_deferred_command_buffer_, &begin_info);

	if (visibility_deferred_pipeline_)
	{
		// bind pipeline
		visibility_deferred_pipeline_->RecordCommands(visibility_deferred_command_buffer_, 0);

		vkCmdEndRenderPass(visibility_deferred_command_buffer_);
	}

	if (vkEndCommandBuffer(visibility_deferred_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record visibility deferred command buffer!");
	}
}

void VulkanRenderer::CreateVisibilityPeelCommandBuffers()
{
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	
	// create visibility peel command buffers
	visibility_peel_command_buffers_.resize(VISIBILITY_PEEL_COUNT);
	devices_->CreateCommandBuffers(command_pool_, visibility_peel_command_buffers_.data(), visibility_peel_command_buffers_.size());

	// record the visibility peel commands
	for (int i = 0; i < VISIBILITY_PEEL_COUNT; i++)
	{
		vkBeginCommandBuffer(visibility_peel_command_buffers_[i], &begin_info);

		if (visibility_peel_pipelines_[i])
		{
			// bind pipeline
			visibility_peel_pipelines_[i]->RecordCommands(visibility_peel_command_buffers_[i], 0);

			primitive_buffer_->RecordIndirectDrawCommands(visibility_peel_command_buffers_[i]);

			vkCmdEndRenderPass(visibility_peel_command_buffers_[i]);
		}

		if (vkEndCommandBuffer(visibility_peel_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record peel command buffer!");
		}
	}
}

void VulkanRenderer::CreateVisibilityPeelDeferredCommandBuffers()
{
	// create rendering command buffers
	devices_->CreateCommandBuffers(command_pool_, &visibility_peel_deferred_command_buffer_);

	// record the command buffer
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(visibility_peel_deferred_command_buffer_, &begin_info);

	if (visibility_peel_deferred_pipeline_)
	{
		// bind pipeline
		visibility_peel_deferred_pipeline_->RecordCommands(visibility_peel_deferred_command_buffer_, 0);

		vkCmdEndRenderPass(visibility_peel_deferred_command_buffer_);
	}

	if (vkEndCommandBuffer(visibility_peel_deferred_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record visibility deferred command buffer!");
	}
}

void VulkanRenderer::CreateTransparencyCommandBuffer()
{
	// create transparency command buffers
	devices_->CreateCommandBuffers(command_pool_, &transparency_command_buffer_);

	// record the command buffer
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(transparency_command_buffer_, &begin_info);

	if (transparency_pipeline_)
	{
		// bind pipeline
		transparency_pipeline_->RecordCommands(transparency_command_buffer_, 0);

		for (Mesh* mesh : meshes_)
		{
			mesh->RecordRenderCommands(transparency_command_buffer_, RenderStage::TRANSPARENT);
		}

		vkCmdEndRenderPass(transparency_command_buffer_);
	}

	if (vkEndCommandBuffer(transparency_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record transparent render command buffer!");
	}
}

void VulkanRenderer::CreateTransparencyCompositeCommandBuffer()
{
	// create tranparent composite commands
	devices_->CreateCommandBuffers(command_pool_, &transparency_composite_command_buffer_);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(transparency_composite_command_buffer_, &begin_info);

	if (transparency_composite_pipeline_)
	{
		// bind pipeline
		transparency_composite_pipeline_->RecordCommands(transparency_composite_command_buffer_, 0);

		vkCmdEndRenderPass(transparency_composite_command_buffer_);
	}

	if (vkEndCommandBuffer(transparency_composite_command_buffer_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record transparency composite command buffer!");
	}
}

void VulkanRenderer::CreateBufferVisualisationCommandBuffers()
{
	int buffer_count = swap_chain_->GetSwapChainImages().size();

	// create buffer visualisation command buffers
	buffer_visualisation_command_buffers_.resize(buffer_count);
	devices_->CreateCommandBuffers(command_pool_, buffer_visualisation_command_buffers_.data(), buffer_count);

	// record the command buffers
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

}

void VulkanRenderer::CreateCullingCommandBuffer()
{
	// create the shape culling command buffer
	devices_->CreateCommandBuffers(command_pool_, &shape_culling_command_buffer_);

	// record the command buffer
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(shape_culling_command_buffer_, &begin_info);

	if (shape_culling_pipeline_)
	{
		shape_culling_pipeline_->RecordCommands(shape_culling_command_buffer_);
	}

	vkEndCommandBuffer(shape_culling_command_buffer_);
}

void VulkanRenderer::CreateShaders()
{
	material_shader_ = new VulkanShader();
	material_shader_->Init(devices_, swap_chain_, "../res/shaders/default_material.vert.spv", "", "", "../res/shaders/default_material.frag.spv");

	shadow_map_shader_ = new VulkanShader();
	shadow_map_shader_->Init(devices_, swap_chain_, "../res/shaders/shadow_map.vert.spv", "", "", "../res/shaders/shadow_map.frag.spv");

	buffer_visualisation_shader_ = new VulkanShader();
	buffer_visualisation_shader_->Init(devices_, swap_chain_, "../res/shaders/buffer_visualisation.vert.spv", "", "", "../res/shaders/buffer_visualisation.frag.spv");
	
	shape_culling_shader_ = new VulkanComputeShader();
	shape_culling_shader_->Init(devices_, swap_chain_, "../res/shaders/shape_culling.comp.spv");
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

void VulkanRenderer::GetSceneMinMax(glm::vec3& scene_min, glm::vec3& scene_max)
{
	scene_min = glm::vec3(1e8f, 1e8f, 1e8f);
	scene_max = glm::vec3(-1e8f, -1e8f, -1e8f);
	for (Mesh* mesh : meshes_)
	{
		float scene_min_len = scene_min.x + scene_min.y + scene_min.z;
		float scene_max_len = scene_max.x + scene_max.y + scene_max.z;

		glm::vec3 mesh_min = mesh->GetMinVertex();
		glm::vec3 mesh_max = mesh->GetMaxVertex();

		float mesh_min_len = mesh_min.x + mesh_min.y + mesh_min.z;
		float mesh_max_len = mesh_max.x + mesh_max.y + mesh_max.z;

		if (mesh_min_len < scene_min_len)
			scene_min = mesh_min;

		if (mesh_max_len > scene_max_len)
			scene_max = mesh_max;
	}
}

void VulkanRenderer::AddLight(Light* light)
{
	light->SetLightBufferIndex(lights_.size());
	lights_.push_back(light);

	// store the shadow map image views for this light
	light->SetShadowMapIndex(shadow_maps_.size());
	for (VkImageView shadow_map : light->GetShadowMap()->GetImageViews())
		shadow_maps_.push_back(shadow_map);
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
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &g_buffer_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &transparency_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphore_info, nullptr, &transparency_composite_semaphore_) != VK_SUCCESS) {

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
	//light_data.scene_data = glm::vec4(glm::vec3(0.85f * 10.0f, 0.68f * 10.0f, 0.92f * 10.0f), lights_.size());
	light_data.camera_data = glm::vec4(0.0f, 0.0f, 0.0f, 1000.0f);

	devices_->CopyDataToBuffer(light_buffer_memory_, &light_data, sizeof(SceneLightData));
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