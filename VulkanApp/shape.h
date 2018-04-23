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

/**
* Stores indexes into the primitive buffer for a single mesh shape
*/
class Shape
{
public:
	Shape();

	void InitShape(VulkanDevices* devices, VulkanRenderer* renderer, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, BoundingBox bounding_box, bool transparency_enabled);
	void RecordRenderCommands(VkCommandBuffer& command_buffer);
	void CleanUp();

	inline bool GetTransparencyEnabled() { return transparency_enabled_; }
	inline uint32_t GetShapeIndex() { return shape_index_; }
	inline uint32_t GetVertexCount() { return vertex_count_; }
	inline uint32_t GetIndexCount() { return index_count_; }
	inline VkBuffer GetVertexBuffer() { return vertex_buffer_; }
	inline VkBuffer GetIndexBuffer() { return index_buffer_; }
	inline BoundingBox GetBoundingBox() { return bounding_box_; }

	inline void SetShapeIndex(uint32_t shape_index) { shape_index_ = shape_index; }
	inline void SetVertexBufferOffset(uint32_t vertex_offset) { vertex_buffer_offset_ = vertex_offset; }
	inline void SetIndexBufferOffset(uint32_t index_offset) { index_buffer_offset_ = index_offset; }

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

	BoundingBox bounding_box_;

	bool standalone_shape_;
	bool transparency_enabled_;
};
#endif