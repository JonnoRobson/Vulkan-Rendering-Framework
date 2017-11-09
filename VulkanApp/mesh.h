#ifndef _MESH_H_
#define _MESH_H_

#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vector>
#include <array>
#include <string>

#include "device.h"
#include "primitive_buffer.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 tex_coord;
	glm::vec3 normal;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription binding_description = {};
		binding_description.binding = 0;
		binding_description.stride = sizeof(Vertex);
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return binding_description;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions = {};

		// position
		attribute_descriptions[0].binding = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, pos);

		// texcoord
		attribute_descriptions[1].binding = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, tex_coord);

		// normal
		attribute_descriptions[2].binding = 0;
		attribute_descriptions[2].location = 2;
		attribute_descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[2].offset = offsetof(Vertex, normal);

		return attribute_descriptions;
	}

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && tex_coord == other.tex_coord && normal == other.normal;
	}
};

namespace std
{
	template<> struct hash<Vertex>
	{
		size_t operator()(Vertex const& vertex) const
		{
			return (hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec2>()(vertex.tex_coord) << 1));
		}
	};
}
class Mesh
{
public:
	Mesh();
	~Mesh();
	
	void CreateTriangleMesh(VulkanDevices* devices);
	void CreatePlaneMesh(VulkanDevices* devices);
	void CreateModelMesh(VulkanDevices* devices, std::string filename);

	void UpdateWorldMatrix(glm::mat4 world_matrix);

	void AddToPrimitiveBuffer(VulkanDevices* devices, VulkanPrimitiveBuffer* primitive_buffer);
	void RecordDrawCommands(VkCommandBuffer& command_buffer, VkBuffer mvp_uniform_buffer);

protected:
	void CreateBuffers(VulkanDevices* devices, std::vector<Vertex> vertices, std::vector<uint32_t> indices);

	void CreateVertexBuffer(VulkanDevices* devices, std::vector<Vertex> vertices);
	void CreateIndexBuffer(VulkanDevices* devices, std::vector<uint32_t> indices);
	void CreateWorldMatrixBuffer(VulkanDevices* devices);

protected:
	VkDevice vk_device_handle_;

	VkBuffer vertex_buffer_;
	VkDeviceMemory vertex_buffer_memory_;

	VkBuffer index_buffer_;
	VkDeviceMemory index_buffer_memory_;

	VkBuffer world_matrix_buffer_;
	VkDeviceMemory world_matrix_buffer_memory_;

	uint32_t vertex_count_;
	uint32_t index_count_;

	uint32_t vertex_buffer_offset_;
	uint32_t index_buffer_offset_;
};
#endif