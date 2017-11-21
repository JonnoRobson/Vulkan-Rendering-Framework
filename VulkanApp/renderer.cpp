#include "renderer.h"
#include <array>
#include <map>

void VulkanRenderer::Init(VulkanDevices* devices, VulkanSwapChain* swap_chain, std::string vs_filename, std::string ps_filename)
{
	devices_ = devices;
	swap_chain_ = swap_chain;

	// load a default texture
	default_texture_ = new Texture();
	default_texture_->Init(devices, "../res/textures/default.png");

	CreateMaterialShader(vs_filename, ps_filename);
	CreatePrimitiveBuffer();
	CreateMaterialBuffer();
	CreateCommandPool();
	CreateBuffers();
	CreateSemaphores();

	vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().graphics_family, 0, &graphics_queue_);
}

void VulkanRenderer::RenderScene()
{
	// get swap chain index
	uint32_t image_index = swap_chain_->GetCurrentSwapChainImage();
	VkExtent2D swap_extent = swap_chain_->GetSwapChainExtent();

	// send camera data to the gpu
	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1.0f);
	ubo.view = render_camera_->GetViewMatrix();
	ubo.proj = glm::perspective(glm::radians(45.0f), swap_extent.width / (float)swap_extent.height, 0.1f, 1000.0f);
	ubo.proj[1][1] *= -1;

	void* mapped_data;
	vkMapMemory(devices_->GetLogicalDevice(), matrix_buffer_memory_, 0, sizeof(UniformBufferObject), 0, &mapped_data);
	memcpy(mapped_data, &ubo, sizeof(UniformBufferObject));
	vkUnmapMemory(devices_->GetLogicalDevice(), matrix_buffer_memory_);

	for (Light* light : lights_)
	{
		// send the light data to the gpu
		light->SendLightData(devices_, light_buffer_memory_);

		RenderPass(image_index);
	}
}

void VulkanRenderer::RenderPass(uint32_t image_index)
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

void VulkanRenderer::Cleanup()
{
	// clean up command and descriptor pools
	vkDestroyCommandPool(devices_->GetLogicalDevice(), command_pool_, nullptr);

	// clean up shaders
	material_shader_->Cleanup();
	delete material_shader_;
	material_shader_ = nullptr;

	// clean up primitive buffer
	primitive_buffer_->Cleanup();
	delete primitive_buffer_;
	primitive_buffer_ = nullptr;

	// clean up the material buffer
	material_buffer_->CleanUp();
	delete material_buffer_;
	material_buffer_ = nullptr;

	// clean up the pipeline
	rendering_pipeline_->CleanUp();
	delete rendering_pipeline_;
	rendering_pipeline_ = nullptr;

	// clean up default texture
	default_texture_->Cleanup();
	delete default_texture_;
	default_texture_ = nullptr;

	// clean up semaphores
	vkDestroySemaphore(devices_->GetLogicalDevice(), render_semaphore_, nullptr);
}

void VulkanRenderer::InitPipeline()
{
	rendering_pipeline_ = new VulkanPipeline();

	// add the material buffers to the pipeline
	rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer_, sizeof(UniformBufferObject));
	rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 2, light_buffer_, sizeof(LightBufferObject));
	rendering_pipeline_->AddUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 3, material_buffer_->GetBuffer(), MAX_MATERIAL_COUNT * sizeof(MaterialData));

	// add the material textures to the pipeline
	rendering_pipeline_->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 4, default_texture_->GetSampler());
	rendering_pipeline_->AddTextureArray(VK_SHADER_STAGE_FRAGMENT_BIT, 5, diffuse_textures_);
	/*
	material_pipeline_->AddTexture(VK_SHADER_STAGE_VERTEX_BIT, 1, displacement_texture_);
	material_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 4, ambient_texture_);
	material_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 6, specular_texture_);
	material_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 7, specular_highlight_texture_);
	material_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 8, emissive_texture_);
	material_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 9, bump_texture_);
	material_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 10, alpha_texture_);
	material_pipeline_->AddTexture(VK_SHADER_STAGE_FRAGMENT_BIT, 11, reflection_texture_);
	*/

	// set the pipeline material shader
	rendering_pipeline_->SetShader(material_shader_);

	// create the material pipeline
	rendering_pipeline_->Init(devices_, swap_chain_, primitive_buffer_);
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
			rendering_pipeline_->RecordRenderCommands(command_buffers_[i], i);

			// bind primitive buffer
			primitive_buffer_->RecordBindingCommands(command_buffers_[i]);

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
}

void VulkanRenderer::CreateMaterialShader(std::string vs_filename, std::string ps_filename)
{
	material_shader_ = new VulkanShader();
	material_shader_->Init(devices_, swap_chain_, vs_filename, "", "", ps_filename);
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
	CreateCommandBuffers();
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
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(devices_->GetLogicalDevice(), &semaphoreInfo, nullptr, &render_semaphore_) != VK_SUCCESS) {

		throw std::runtime_error("failed to create semaphores!");
	}
}

void VulkanRenderer::CreateBuffers()
{
	devices_->CreateBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, matrix_buffer_, matrix_buffer_memory_);
	devices_->CreateBuffer(sizeof(LightBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, light_buffer_, light_buffer_memory_);
}