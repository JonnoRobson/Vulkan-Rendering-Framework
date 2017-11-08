#include "mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>

Mesh::Mesh()
{
	vk_device_handle_ = VK_NULL_HANDLE;
	vertex_buffer_ = VK_NULL_HANDLE;
}

Mesh::~Mesh()
{
	vkDestroyBuffer(vk_device_handle_, vertex_buffer_, nullptr);
	vkFreeMemory(vk_device_handle_, vertex_buffer_memory_, nullptr);

	vkDestroyBuffer(vk_device_handle_, index_buffer_, nullptr);
	vkFreeMemory(vk_device_handle_, index_buffer_memory_, nullptr);

	vk_device_handle_ = VK_NULL_HANDLE;
}

void Mesh::UpdateWorldMatrix(glm::mat4 world_matrix)
{
	// copy the world matrix data to the staging buffer
	void* data;
	vkMapMemory(vk_device_handle_, world_matrix_buffer_memory_, 0, sizeof(glm::mat4), 0, &data);
	memcpy(data, &world_matrix, sizeof(glm::mat4));
	vkUnmapMemory(vk_device_handle_, world_matrix_buffer_memory_);
}

void Mesh::CreateBuffers(VulkanDevices* devices, std::vector<Vertex> vertices, std::vector<uint32_t> indices)
{
	vk_device_handle_ = devices->GetLogicalDevice();

	CreateVertexBuffer(devices, vertices);
	CreateIndexBuffer(devices, indices);
	CreateWorldMatrixBuffer(devices);
}

void Mesh::CreateTriangleMesh(VulkanDevices* devices)
{
	const std::vector<Vertex> vertices = 
	{
		{{0.0f, -0.5f, 0.0f}, {0.5f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}}
	};

	const std::vector<uint32_t> indices =
	{
		0, 1, 2
	};

	CreateVertexBuffer(devices, vertices);
	CreateIndexBuffer(devices, indices);
}

void Mesh::CreatePlaneMesh(VulkanDevices* devices)
{
	const std::vector<Vertex> vertices = 
	{
		{ { -0.5f, -0.5f, 0.0f },{ 1.0f, 0.0f} },
		{ { 0.5f, -0.5f, 0.0f },{ 0.0f, 0.0f } },
		{ { 0.5f, 0.5f, 0.0f },{ 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.0f },{ 1.0f, 1.0f} }
	};

	const std::vector<uint32_t> indices =
	{
		0, 1, 2, 2, 3, 0
	};

	CreateBuffers(devices, vertices, indices);
}

void Mesh::CreateDualPlaneMesh(VulkanDevices* devices)
{
	const std::vector<Vertex> vertices =
	{
		// 1st plane
		{ { -0.5f, -0.5f, 0.0f },{ 1.0f, 0.0f } },
		{ { 0.5f, -0.5f, 0.0f },{ 0.0f, 0.0f } },
		{ { 0.5f, 0.5f, 0.0f },{ 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.0f },{ 1.0f, 1.0f } },

		// 2nd plane
		{ { -0.5f, -0.5f, -0.5f },{ 1.0f, 0.0f } },
		{ { 0.5f, -0.5f, -0.5f },{ 0.0f, 0.0f } },
		{ { 0.5f, 0.5f, -0.5f },{ 0.0f, 1.0f } },
		{ { -0.5f, 0.5f, -0.5f },{ 1.0f, 1.0f } }
	};

	const std::vector<uint32_t> indices =
	{
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4
	};

	CreateBuffers(devices, vertices, indices);
}

void Mesh::CreateModelMesh(VulkanDevices* devices, std::string filename)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str()))
	{
		throw std::runtime_error(err);
	}

	std::unordered_map<Vertex, uint32_t> unique_vertices = {};

	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex = {};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.tex_coord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			if (unique_vertices.count(vertex) == 0)
			{
				unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(unique_vertices[vertex]);
		}
	}

	CreateBuffers(devices, vertices, indices);
}

void Mesh::CreateVertexBuffer(VulkanDevices* devices, std::vector<Vertex> vertices)
{
	vertex_count_ = static_cast<uint32_t>(vertices.size());

	VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();
	
	devices->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertex_buffer_, vertex_buffer_memory_);

	// copy the data to the staging buffer
	void* data;
	vkMapMemory(vk_device_handle_, vertex_buffer_memory_, 0, buffer_size, 0, &data);
	memcpy(data, vertices.data(), (size_t)buffer_size);
	vkUnmapMemory(vk_device_handle_, vertex_buffer_memory_);
}

void Mesh::CreateIndexBuffer(VulkanDevices* devices, std::vector<uint32_t> indices)
{
	index_count_ = static_cast<uint32_t>(indices.size());

	VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

	devices->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, index_buffer_, index_buffer_memory_);

	// copy the data to the staging buffer
	void* data;
	vkMapMemory(vk_device_handle_, index_buffer_memory_, 0, buffer_size, 0, &data);
	memcpy(data, indices.data(), (size_t)buffer_size);
	vkUnmapMemory(vk_device_handle_, index_buffer_memory_);
}

void Mesh::CreateWorldMatrixBuffer(VulkanDevices* devices)
{
	VkDeviceSize buffer_size = sizeof(glm::mat4);

	devices->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, world_matrix_buffer_, world_matrix_buffer_memory_);
}

void Mesh::RecordDrawCommands(VkCommandBuffer& command_buffer, VkBuffer mvp_uniform_buffer)
{
	// create the world matrix copy command for this model
	VkBufferCopy copy_region = {};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = sizeof(glm::mat4);
	vkCmdCopyBuffer(command_buffer, world_matrix_buffer_, mvp_uniform_buffer, 1, &copy_region);

	// create the draw command for this model
	vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(index_count_), 1, index_buffer_offset_, vertex_buffer_offset_, 0);
}

void Mesh::AddToPrimitiveBuffer(VulkanDevices* devices, VulkanPrimitiveBuffer* primitive_buffer)
{
	primitive_buffer->AddPrimitiveData(devices, vertex_count_, index_count_, vertex_buffer_, index_buffer_, vertex_buffer_offset_, index_buffer_offset_);
}