#include "primitive_buffer.h"
#include "mesh.h"

VulkanPrimitiveBuffer::VulkanPrimitiveBuffer()
{
	last_vertex_ = 0;
	last_index_ = 0;
	vertex_count_ = 0;
	index_count_ = 0;
}

VulkanPrimitiveBuffer::~VulkanPrimitiveBuffer()
{

}

void VulkanPrimitiveBuffer::Init(VulkanDevices* devices, VkVertexInputBindingDescription binding_description, std::vector<VkVertexInputAttributeDescription> attribute_descriptions)
{
	device_handle_ = devices->GetLogicalDevice();

	VkDeviceSize vertex_buffer_size = MAX_PRIMITIVE_VERTICES * sizeof(Vertex);
	VkDeviceSize index_buffer_size = MAX_PRIMITIVE_INDICES * sizeof(uint32_t);
	
	devices->CreateBuffer(vertex_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer_, vertex_buffer_memory_);
	devices->CreateBuffer(index_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer_, index_buffer_memory_);
}

void VulkanPrimitiveBuffer::Cleanup()
{
	// cleanup vertex buffer
	vkDestroyBuffer(device_handle_, vertex_buffer_, nullptr);
	vkFreeMemory(device_handle_, vertex_buffer_memory_, nullptr);

	// cleanup index buffer
	vkDestroyBuffer(device_handle_, index_buffer_, nullptr);
	vkFreeMemory(device_handle_, index_buffer_memory_, nullptr);
}

void VulkanPrimitiveBuffer::RecordBindingCommands(VkCommandBuffer& command_buffer)
{
	VkBuffer vertex_buffers[] = { vertex_buffer_ };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
	vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanPrimitiveBuffer::AddPrimitiveData(VulkanDevices* devices, uint32_t vertex_count, uint32_t index_count, VkBuffer vertices, VkBuffer indices, uint32_t& vertex_offset, uint32_t& index_offset)
{
	VkDeviceSize vertex_size = vertex_count * sizeof(Vertex);
	VkDeviceSize index_size = index_count * sizeof(uint32_t);

	// return the vertex and index offsets for this primitive
	vertex_offset = last_vertex_;
	index_offset = last_index_;

	// copy the vertex and index buffers
	devices->CopyBuffer(vertices, vertex_buffer_, vertex_size, last_vertex_ * sizeof(Vertex));
	devices->CopyBuffer(indices, index_buffer_, index_size, last_index_ * sizeof(uint32_t));

	// increment the vertex and index counts
	last_vertex_ += vertex_count;
	last_index_ += index_count;
	vertex_count_ += vertex_count;
	index_count_ += index_count;
}