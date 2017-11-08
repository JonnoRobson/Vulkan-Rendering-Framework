#include "uniform_buffer.h"

void UniformBuffer::Init(VulkanDevices* devices, int float_count, int vec2_count, int vec3_count, int vec4_count, int mat4_count)
{
	// create the arrays for the buffer
	uniform_buffer_float_.resize(float_count);
	uniform_buffer_vec2_.resize(vec2_count);
	uniform_buffer_vec3_.resize(vec3_count);
	uniform_buffer_vec4_.resize(vec4_count);
	uniform_buffer_mat4_.resize(mat4_count);

	// define the size of the buffer
	uniform_buffer_size_ = 0;
	uniform_buffer_size_ = uniform_buffer_size_ + (float_count * sizeof(float));
	uniform_buffer_size_ = uniform_buffer_size_ + (vec2_count * sizeof(glm::vec2));
	uniform_buffer_size_ = uniform_buffer_size_ + (vec3_count * sizeof(glm::vec3));
	uniform_buffer_size_ = uniform_buffer_size_ + (vec4_count * sizeof(glm::vec4));
	uniform_buffer_size_ = uniform_buffer_size_ + (mat4_count * sizeof(glm::mat4));

	// store the number of floats used by the buffer
	uniform_buffer_float_count_ = uniform_buffer_size_ / sizeof(float);

	// create the buffer
	devices->CreateBuffer(uniform_buffer_size_, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniform_buffer_, uniform_buffer_memory_);
}

void UniformBuffer::Cleanup(VulkanDevices* devices)
{
	// destroy the buffer resources
	vkFreeMemory(devices->GetLogicalDevice(), uniform_buffer_memory_, nullptr);
	vkDestroyBuffer(devices->GetLogicalDevice(), uniform_buffer_, nullptr);

	// destroy the buffer data
	uniform_buffer_float_.clear();
	uniform_buffer_vec2_.clear();
	uniform_buffer_vec3_.clear();
	uniform_buffer_vec4_.clear();
	uniform_buffer_mat4_.clear();
}

void UniformBuffer::UpdateBufferContents(VkDevice device, void* data)
{
	void* mapped_data;

	// map buffer memory cpu side
	vkMapMemory(device, uniform_buffer_memory_, 0, uniform_buffer_size_, 0, &mapped_data);

	if (data != nullptr)
	{
		// update buffer contents using provided data
		memcpy(mapped_data, data, uniform_buffer_size_);
	}
	else
	{
		// create a temp buffer to copy the stored data from
		int buffer_location = 0;
		float* buffer_data = new float[uniform_buffer_float_count_];
		// floats
		memcpy(buffer_data + buffer_location, uniform_buffer_float_.data(), uniform_buffer_float_.size() * sizeof(float));
		buffer_location = buffer_location + uniform_buffer_float_.size();
		// vec2s
		memcpy(buffer_data + buffer_location, uniform_buffer_vec2_.data(), uniform_buffer_vec2_.size() * sizeof(glm::vec2));
		buffer_location = buffer_location + uniform_buffer_vec2_.size();
		// vec3s
		memcpy(buffer_data + buffer_location, uniform_buffer_vec3_.data(), uniform_buffer_vec3_.size() * sizeof(glm::vec3));
		buffer_location = buffer_location + uniform_buffer_vec3_.size();
		// vec4s
		memcpy(buffer_data + buffer_location, uniform_buffer_vec4_.data(), uniform_buffer_vec4_.size() * sizeof(glm::vec4));
		buffer_location = buffer_location + uniform_buffer_vec4_.size();
		// mat4s
		memcpy(buffer_data + buffer_location, uniform_buffer_mat4_.data(), uniform_buffer_mat4_.size() * sizeof(glm::mat4));
		buffer_location = buffer_location + uniform_buffer_mat4_.size();

		// update buffer contents using stored buffer data
		memcpy(mapped_data, buffer_data, uniform_buffer_size_);

		// delete the temp buffer now it has been used
		delete[] buffer_data;
	}

	// send the buffer data to the gpu
	vkUnmapMemory(device, uniform_buffer_memory_);
}