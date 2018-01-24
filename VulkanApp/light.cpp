#include "light.h"
#include <glm/gtc/matrix_transform.hpp>

#include "device.h"
#include "renderer.h"

Light::Light()
{
	position_ = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	direction_ = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	color_ = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	intensity_ = 0.0f;
	range_ = 0.0f;
	type_ = 0.0f;
	shadows_enabled_ = false;
	stationary_ = true;
	light_buffer_index_ = 0;
	shadow_map_ = nullptr;
}

void Light::Init(VulkanDevices* devices, VulkanRenderer* renderer)
{
	devices_ = devices;

	// create the shadow map render target
	shadow_map_ = new VulkanRenderTarget();
	shadow_map_->Init(devices, VK_FORMAT_R32_SFLOAT, SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, (type_ == 1.0f) ? 6 : 1, true);

	// create the shadow mapping pipeline
	VkBuffer matrix_buffer;
	renderer->GetMatrixBuffer(matrix_buffer, matrix_buffer_memory_);
	if (type_ != 1.0f)
	{
		shadow_map_pipelines_.resize(1);
		shadow_map_pipelines_[0] = new ShadowMapPipeline();
		shadow_map_pipelines_[0]->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer, sizeof(UniformBufferObject));
		shadow_map_pipelines_[0]->SetImageViews(shadow_map_->GetRenderTargetFormat(), shadow_map_->GetRenderTargetDepthFormat(), shadow_map_->GetImageViews()[0], shadow_map_->GetDepthImageView());
		shadow_map_pipelines_[0]->SetShader(renderer->GetShadowMapShader());
		shadow_map_pipelines_[0]->Init(devices, renderer->GetSwapChain(), renderer->GetPrimitiveBuffer());
	}
	else
	{
		shadow_map_pipelines_.resize(6);
		for (int i = 0; i < 6; i++)
		{
			shadow_map_pipelines_[i] = new ShadowMapPipeline();
			shadow_map_pipelines_[i]->SetShader(renderer->GetShadowMapShader());
			shadow_map_pipelines_[i]->AddUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_buffer, sizeof(UniformBufferObject));
			shadow_map_pipelines_[i]->SetImageViews(shadow_map_->GetRenderTargetFormat(), shadow_map_->GetRenderTargetDepthFormat(), shadow_map_->GetImageViews()[i], shadow_map_->GetDepthImageView());
			shadow_map_pipelines_[i]->Init(devices, renderer->GetSwapChain(), renderer->GetPrimitiveBuffer());
		}
	}
	
	renderer->AddLight(this);
}

void Light::Cleanup()
{
	// clean up the shadow map
	shadow_map_->Cleanup();
	delete shadow_map_;
	shadow_map_ = nullptr;

	// clean up the shadow map pipeline
	for (int i = 0; i < shadow_map_pipelines_.size(); i++)
	{
		shadow_map_pipelines_[i]->CleanUp();
		delete shadow_map_pipelines_[i];
		shadow_map_pipelines_[i] = nullptr;
	}
	shadow_map_pipelines_.clear();
}

void Light::GenerateShadowMap()
{
	// send transform data to the gpu
	for (int i = 0; i < shadow_map_->GetRenderTargetCount(); i++)
	{
		// send the correct view projection matrices to the buffer
		UniformBufferObject ubo = {};
		ubo.model = glm::mat4(1.0f);
		ubo.view = GetViewMatrix(i);
		ubo.proj = GetProjectionMatrix();

		// clear the shadow map and depth resources
		VkClearColorValue clear_color = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
		shadow_map_->ClearImage(clear_color, i);
		shadow_map_->ClearDepth();

		devices_->CopyDataToBuffer(matrix_buffer_memory_, &ubo, sizeof(UniformBufferObject));

		// submit the shadow map command buffer
		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = nullptr;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &shadow_map_command_buffers_[i];
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = nullptr;

		VkQueue graphics_queue;
		vkGetDeviceQueue(devices_->GetLogicalDevice(), devices_->GetQueueFamilyIndices().graphics_family, 0, &graphics_queue);

		VkResult result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		vkQueueWaitIdle(graphics_queue);
	}
}

void Light::SendLightData(VulkanDevices* devices, VkDeviceMemory light_buffer_memory)
{
	LightData light_data = {};
	light_data.position = glm::vec4(position_);
	light_data.direction = glm::vec4(direction_);
	light_data.color = glm::vec4(color_);
	light_data.intensity = intensity_;
	light_data.range = range_;
	light_data.light_type = type_;
	light_data.shadows_enabled = (shadows_enabled_) ? 1.0f : 0.0f;
	for (int i = 0; i < 6; i++)
	{
		light_data.view_proj_matrices[i] = proj_matrix_ * view_matrices_[i];
	}
	light_data.shadow_map_index = shadow_map_index_;
	light_data.padding[0] = 0;
	light_data.padding[1] = 0;
	light_data.padding[2] = 0;
	VkDeviceSize offset = sizeof(SceneLightData) + (light_buffer_index_ * sizeof(LightData));

	devices_->CopyDataToBuffer(light_buffer_memory, &light_data, sizeof(LightData), offset);
}

glm::mat4 Light::GetViewMatrix(int index)
{
	CalculateViewMatrices();
	return view_matrices_[index];
}

glm::mat4 Light::GetProjectionMatrix()
{
	CalculateProjectionMatrix();
	return proj_matrix_;
}

void Light::CalculateViewMatrices()
{
	if (type_ == 0.0f)
	{
		view_matrices_[0] = glm::lookAt(glm::vec3(direction_ * -1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		view_matrices_[1] = glm::mat4(1.0f);
		view_matrices_[2] = glm::mat4(1.0f);
		view_matrices_[3] = glm::mat4(1.0f);
		view_matrices_[4] = glm::mat4(1.0f);
		view_matrices_[5] = glm::mat4(1.0f);
	}
	else if (type_ == 1.0f)
	{
		view_matrices_[0] = glm::lookAt(glm::vec3(position_), glm::vec3(position_) + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		view_matrices_[1] = glm::lookAt(glm::vec3(position_), glm::vec3(position_) + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		view_matrices_[2] = glm::lookAt(glm::vec3(position_), glm::vec3(position_) + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		view_matrices_[3] = glm::lookAt(glm::vec3(position_), glm::vec3(position_) + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		view_matrices_[4] = glm::lookAt(glm::vec3(position_), glm::vec3(position_) + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		view_matrices_[5] = glm::lookAt(glm::vec3(position_), glm::vec3(position_) + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	else if (type_ == 2.0f)
	{
		view_matrices_[0] = glm::lookAt(glm::vec3(position_), glm::vec3(position_) + glm::vec3(direction_), glm::vec3(0.0f, 0.0f, 1.0f));
		view_matrices_[1] = glm::mat4(1.0f);
		view_matrices_[2] = glm::mat4(1.0f);
		view_matrices_[3] = glm::mat4(1.0f);
		view_matrices_[4] = glm::mat4(1.0f);
		view_matrices_[5] = glm::mat4(1.0f);
	}
}

void Light::CalculateProjectionMatrix()
{
	if (type_ == 0.0f)
	{
		glm::mat4 clip =
		glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, -1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.5f, 0.0f,
				0.0f, 0.0f, 0.5f, 1.0f);

		// transform the scene bounds to light view space
		glm::vec4 scene_light_min = view_matrices_[0] * glm::vec4(scene_min_vertex_, 1.0f);
		scene_light_min /= scene_light_min.w;

		glm::vec4 scene_light_max = view_matrices_[0] * glm::vec4(scene_max_vertex_, 1.0f);
		scene_light_max /= scene_light_max.w;

		float l = -scene_max_vertex_.x - (scene_max_vertex_.x / 5.0f);
		float r = scene_max_vertex_.x + (scene_max_vertex_.x / 5.0f);
		float b = scene_max_vertex_.y + (scene_max_vertex_.y / 5.0f);
		float t = -scene_max_vertex_.y - (scene_max_vertex_.y / 5.0f);
		float n = -scene_max_vertex_.z - (scene_max_vertex_.z / 5.0f);
		float f = scene_max_vertex_.z + (scene_max_vertex_.z / 5.0f);

		proj_matrix_ = clip * glm::ortho<float>(-scene_max_vertex_.x, scene_max_vertex_.x, scene_max_vertex_.y, -scene_max_vertex_.y, -scene_max_vertex_.z, scene_max_vertex_.z);
		proj_matrix_ = clip * glm::ortho<float>(l, r, b, t, n, f);
	}
	else
	{
		glm::mat4 clip =
		glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, -1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.5f, 0.0f,
				0.0f, 0.0f, 0.5f, 1.0f);

		proj_matrix_ = clip * glm::perspectiveFov<float>(glm::radians(91.0), SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, 1.0f, 1000.0f);
	}
}

void Light::RecordShadowMapCommands(VkCommandPool command_pool, std::vector<Mesh*>& meshes)
{
	shadow_map_command_buffers_.resize((type_ == 1.0f) ? 6 : 1);

	// use this access to mesh data to set the scene size vertices
	scene_min_vertex_ = glm::vec3(1e8f, 1e8f, 1e8f);
	scene_max_vertex_ = glm::vec3(-1e8f, -1e8f, -1e8f);
	for (Mesh* mesh : meshes)
	{
		float scene_min_len = scene_min_vertex_.x + scene_min_vertex_.y + scene_min_vertex_.z;
		float scene_max_len = scene_max_vertex_.x + scene_max_vertex_.y + scene_max_vertex_.z;

		glm::vec3 mesh_min = mesh->GetMinVertex();
		glm::vec3 mesh_max = mesh->GetMaxVertex();

		float mesh_min_len = mesh_min.x + mesh_min.y + mesh_min.z;
		float mesh_max_len = mesh_max.x + mesh_max.y + mesh_max.z;

		if (mesh_min_len < scene_min_len)
			scene_min_vertex_ = mesh_min;
		
		if (mesh_max_len > scene_max_len)
			scene_max_vertex_ = mesh_max;
	}

	vkFreeCommandBuffers(devices_->GetLogicalDevice(), command_pool, shadow_map_command_buffers_.size(), shadow_map_command_buffers_.data());


	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = shadow_map_command_buffers_.size();

	if (vkAllocateCommandBuffers(devices_->GetLogicalDevice(), &allocate_info, shadow_map_command_buffers_.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate render command buffers!");
	}

	for (int i = 0; i < shadow_map_command_buffers_.size(); i++)
	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(shadow_map_command_buffers_[i], &begin_info);

		if (shadow_map_pipelines_[i])
		{
			// bind pipeline
			shadow_map_pipelines_[i]->RecordCommands(shadow_map_command_buffers_[i], 0);

			for (Mesh* mesh : meshes)
			{
				mesh->RecordRenderCommands(shadow_map_command_buffers_[i], RenderStage::GENERIC);
			}

			vkCmdEndRenderPass(shadow_map_command_buffers_[i]);
		}

		if (vkEndCommandBuffer(shadow_map_command_buffers_[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record shadow map command buffer!");
		}
	}
}