#ifndef _SHAPE_H_
#define _SHAPE_H_


#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vector>

#include "material.h"

struct Vertex;

struct BoundingBox
{
	glm::vec4 min_vertex;
	glm::vec4 max_vertex;
};

class Shape
{
public:
	Shape();

	void InitShape(VulkanDevices* devices, VulkanRenderer* renderer, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, bool transparency_enabled);
	void RecordRenderCommands(VkCommandBuffer& command_buffer, VkPipelineLayout pipeline_layout = VK_NULL_HANDLE);
	void CleanUp();
	
	inline bool GetTransparencyEnabled() { return transparency_enabled_; }
	inline uint32_t GetShapeIndex() { return shape_index_; }
protected:
	
	void CreateVertexBuffer(std::vector<Vertex>& vertices);
	void CreateIndexBuffer(std::vector<uint32_t>& indices);

protected:
	VulkanDevices* devices_;
	Material* mesh_material_;

	VkBuffer vertex_buffer_;
	VkDeviceMemory vertex_buffer_memory_;

	VkBuffer index_buffer_;
	VkDeviceMemory index_buffer_memory_;

	uint32_t vertex_count_;
	uint32_t index_count_;

	uint32_t vertex_buffer_offset_;
	uint32_t index_buffer_offset_;
	uint32_t shape_index_;

	bool standalone_shape_;
	bool transparency_enabled_;
};
#endif