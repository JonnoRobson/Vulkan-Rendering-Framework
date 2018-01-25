#ifndef _MESH_H_
#define _MESH_H_

#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vector>
#include <array>
#include <string>
#include <map>
#include <mutex>

#include "device.h"
#include "primitive_buffer.h"
#include "shape.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 tex_coord;
	glm::vec3 normal;
	uint32_t mat_index;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription binding_description = {};
		binding_description.binding = 0;
		binding_description.stride = sizeof(Vertex);
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return binding_description;
	}

	static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions = {};

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

		// mat_index
		attribute_descriptions[3].binding = 0;
		attribute_descriptions[3].location = 3;
		attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_UINT;
		attribute_descriptions[3].offset = offsetof(Vertex, mat_index);

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
	
	void CreateModelMesh(VulkanDevices* devices, VulkanRenderer* renderer, std::string filename);

	void UpdateWorldMatrix(glm::mat4 world_matrix);
	
	void RecordRenderCommands(VkCommandBuffer& command_buffer, RenderStage render_stage = RenderStage::OPAQUE);

	inline glm::vec3 GetMinVertex() { return min_vertex_; }
	inline glm::vec3 GetMaxVertex() { return max_vertex_;}

protected:
	void LoadShapeThreaded(std::mutex* shape_mutex, VulkanDevices* devices, VulkanRenderer* renderer, tinyobj::attrib_t* attrib, std::vector<tinyobj::material_t>* materials, std::vector<tinyobj::shape_t*> shapes);

protected:
	VkDevice vk_device_handle_;

	glm::mat4 world_matrix_;

	glm::vec3 min_vertex_;
	glm::vec3 max_vertex_;
	uint32_t most_complex_shape_size_;

	std::vector<Shape*> mesh_shapes_;
	std::map<std::string, Material*> mesh_materials_;
};
#endif