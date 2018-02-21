#ifndef _PRIMITIVE_BUFFER_H_
#define _PRIMITIVE_BUFFER_H_

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

#include "device.h"

#define MAX_PRIMITIVE_VERTICES 15000000
#define MAX_PRIMITIVE_INDICES 30000000

class Shape;

struct ShapeData
{
	uint32_t offsets[4];
	glm::vec4 min_bounding_vertex;
	glm::vec4 max_bounding_vertex;
};

struct IndirectDrawCommand
{
	uint32_t index_count;
	uint32_t instance_count;
	uint32_t first_index;
	int32_t vertex_offset;
	uint32_t first_instance;
	uint32_t padding[3];
};

class VulkanPrimitiveBuffer
{
public:
	VulkanPrimitiveBuffer();
	~VulkanPrimitiveBuffer();

	void Init(VulkanDevices* devices, VkVertexInputBindingDescription binding_description, std::vector<VkVertexInputAttributeDescription> attribute_descriptions);
	void InitShapeBuffer(VulkanDevices* devices);

	void Cleanup();

	void AddPrimitiveData(VulkanDevices* devices, uint32_t vertex_count, uint32_t index_count, VkBuffer vertices, VkBuffer indices, uint32_t& vertex_offset, uint32_t& index_offset, uint32_t& shape_index);
	void AddPrimitiveData(VulkanDevices* devices, Shape* shape);

	void RecordBindingCommands(VkCommandBuffer& command_buffer);
	void RecordIndirectDrawCommands(VkCommandBuffer& command_buffer);

	inline uint32_t GetVertexCount() { return vertex_count_; }
	inline uint32_t GetIndexCount() { return index_count_; }
	inline uint32_t GetShapeCount() { return shape_data_.size(); }
	inline VkBuffer GetVertexBuffer() { return vertex_buffer_; }
	inline VkBuffer GetIndexBuffer() { return index_buffer_; }
	inline VkBuffer GetShapeBuffer() { return shape_buffer_; }
	inline VkBuffer GetIndirectDrawBuffer() { return indirect_draw_buffer_; }

protected:
	VkDevice device_handle_;

	VkBuffer vertex_buffer_;
	VkDeviceMemory vertex_buffer_memory_;

	VkBuffer index_buffer_;
	VkDeviceMemory index_buffer_memory_;

	VkBuffer indirect_draw_buffer_;
	VkDeviceMemory indirect_draw_buffer_memory_;

	// shape buffer components
	VkBuffer shape_buffer_;
	VkDeviceMemory shape_buffer_memory_;
	std::vector<ShapeData> shape_data_;

	uint32_t last_vertex_;
	uint32_t last_index_;
	uint32_t vertex_count_;
	uint32_t index_count_;

};

#endif