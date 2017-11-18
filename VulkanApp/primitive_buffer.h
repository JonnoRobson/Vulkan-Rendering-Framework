#ifndef _PRIMITIVE_BUFFER_H_
#define _PRIMITIVE_BUFFER_H_

#include <vulkan/vulkan.h>
#include <vector>

#include "device.h"

#define MAX_PRIMITIVE_VERTICES 50000000
#define MAX_PRIMITIVE_INDICES 50000000

class VulkanPrimitiveBuffer
{
public:
	VulkanPrimitiveBuffer();
	~VulkanPrimitiveBuffer();

	void Init(VulkanDevices* devices, VkVertexInputBindingDescription binding_description, std::vector<VkVertexInputAttributeDescription> attribute_descriptions);
	void Cleanup();

	void AddPrimitiveData(VulkanDevices* devices, uint32_t vertex_count, uint32_t index_count, VkBuffer vertices, VkBuffer indices, uint32_t& vertex_offset, uint32_t& index_offset);

	void RecordBindingCommands(VkCommandBuffer& command_buffer);

protected:
	VkDevice device_handle_;

	VkBuffer vertex_buffer_;
	VkDeviceMemory vertex_buffer_memory_;

	VkBuffer index_buffer_;
	VkDeviceMemory index_buffer_memory_;

	uint32_t last_vertex_;
	uint32_t last_index_;
	uint32_t vertex_count_;
	uint32_t index_count_;

};

#endif