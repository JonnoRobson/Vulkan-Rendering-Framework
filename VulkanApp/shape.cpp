#include "shape.h"
#include "mesh.h"
#include "renderer.h"

Shape::Shape()
{
	devices_ = nullptr;
	mesh_material_ = nullptr;

	vertex_buffer_ = VK_NULL_HANDLE;
	vertex_buffer_memory_ = VK_NULL_HANDLE;

	index_buffer_ = VK_NULL_HANDLE;
	index_buffer_memory_ = VK_NULL_HANDLE;
}

void Shape::InitShape(VulkanDevices* devices, VulkanRenderer* renderer, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	devices_ = devices;

	CreateVertexBuffer(vertices);
	CreateIndexBuffer(indices);
	renderer->GetPrimitiveBuffer()->AddPrimitiveData(devices, vertex_count_, index_count_, vertex_buffer_, index_buffer_, vertex_buffer_offset_, index_buffer_offset_);

	// free the vertex and index buffers now that they have been added to the primitive buffer
	vkDestroyBuffer(devices_->GetLogicalDevice(), vertex_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_, nullptr);

	vkDestroyBuffer(devices_->GetLogicalDevice(), index_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), index_buffer_memory_, nullptr);
}

void Shape::CleanUp()
{

}

void Shape::CreateVertexBuffer(std::vector<Vertex>& vertices)
{
	vertex_count_ = static_cast<uint32_t>(vertices.size());

	VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertex_buffer_, vertex_buffer_memory_);

	// copy the data to the staging buffer
	void* data;
	vkMapMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_, 0, buffer_size, 0, &data);
	memcpy(data, vertices.data(), (size_t)buffer_size);
	vkUnmapMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_);
}

void Shape::CreateIndexBuffer(std::vector<uint32_t>& indices)
{
	index_count_ = static_cast<uint32_t>(indices.size());

	VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, index_buffer_, index_buffer_memory_);

	// copy the data to the staging buffer
	void* data;
	vkMapMemory(devices_->GetLogicalDevice(), index_buffer_memory_, 0, buffer_size, 0, &data);
	memcpy(data, indices.data(), (size_t)buffer_size);
	vkUnmapMemory(devices_->GetLogicalDevice(), index_buffer_memory_);
}

void Shape::RecordRenderCommands(VkCommandBuffer& command_buffer)
{
	// create the draw command for this shape
	vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(index_count_), 1, index_buffer_offset_, vertex_buffer_offset_, 0);
}