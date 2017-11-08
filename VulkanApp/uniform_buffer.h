#ifndef _UNIFORM_BUFFER_H_
#define _UNIFORM_BUFFER_H_

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>

#include "device.h"

class UniformBuffer
{
public:
	void Init(VulkanDevices* devices, int float_count, int vec2_count, int vec3_count, int vec4_count, int mat4_count);
	void Cleanup(VulkanDevices* devices);

	void UpdateBufferContents(VkDevice device, void* data = nullptr);

	VkBuffer GetBuffer() { return uniform_buffer_; }
	VkDeviceMemory GetBufferMemory() { return uniform_buffer_memory_; }
	VkDeviceSize GetBufferSize() { return uniform_buffer_size_; }

protected:
	VkBuffer uniform_buffer_;
	VkDeviceMemory uniform_buffer_memory_;
	VkDeviceSize uniform_buffer_size_;
	int uniform_buffer_float_count_;

	std::vector<float> uniform_buffer_float_;
	std::vector<glm::vec2> uniform_buffer_vec2_;
	std::vector<glm::vec3> uniform_buffer_vec3_;
	std::vector<glm::vec4> uniform_buffer_vec4_;
	std::vector<glm::mat4> uniform_buffer_mat4_;
};

#endif