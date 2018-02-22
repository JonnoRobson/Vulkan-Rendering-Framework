#include "primitive_buffer.h"
#include "mesh.h"
#include "shape.h"

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
	
	devices->CreateBuffer(vertex_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer_, vertex_buffer_memory_);
	devices->CreateBuffer(index_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer_, index_buffer_memory_);
}

void VulkanPrimitiveBuffer::InitShapeBuffer(VulkanDevices* devices)
{
	// create the shape buffer
	VkDeviceSize shape_buffer_size = shape_data_.size() * sizeof(ShapeData);
	devices->CreateBuffer(shape_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shape_buffer_, shape_buffer_memory_);

	// use a staging buffer to copy shapes to the shape buffer
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	devices->CreateBuffer(shape_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);
	devices->CopyDataToBuffer(staging_buffer_memory, shape_data_.data(), shape_buffer_size);
	devices->CopyBuffer(staging_buffer, shape_buffer_, shape_buffer_size);

	// delete the staging buffer now it is no longer needed
	vkDestroyBuffer(devices->GetLogicalDevice(), staging_buffer, nullptr);
	vkFreeMemory(devices->GetLogicalDevice(), staging_buffer_memory, nullptr);

	// create the indirect draw buffer
	VkDeviceSize indirect_buffer_size = shape_data_.size() * sizeof(IndirectDrawCommand);
	devices->CreateBuffer(indirect_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indirect_draw_buffer_, indirect_draw_buffer_memory_);
	
	// use a staging buffer to copy indirect draw data to the shape buffer
	std::vector<IndirectDrawCommand> indirect_draw_commands;
	for (ShapeData shape_data : shape_data_)
	{
		IndirectDrawCommand indirect_draw_command = {};
		indirect_draw_command.vertex_offset = shape_data.offsets[0];
		indirect_draw_command.first_index = shape_data.offsets[1];
		indirect_draw_command.index_count = shape_data.offsets[3];
		indirect_draw_command.first_instance = shape_data.offsets[2];
		indirect_draw_command.instance_count = 1;
		indirect_draw_command.padding[0] = 0;
		indirect_draw_command.padding[1] = 0;
		indirect_draw_command.padding[2] = 0;
		indirect_draw_commands.push_back(indirect_draw_command);
	}
	
	devices->CreateBuffer(indirect_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);
	devices->CopyDataToBuffer(staging_buffer_memory, indirect_draw_commands.data(), indirect_buffer_size);
	devices->CopyBuffer(staging_buffer, indirect_draw_buffer_, indirect_buffer_size);

	// delete the staging buffer now it is no longer needed
	vkDestroyBuffer(devices->GetLogicalDevice(), staging_buffer, nullptr);
	vkFreeMemory(devices->GetLogicalDevice(), staging_buffer_memory, nullptr);
}

void VulkanPrimitiveBuffer::Cleanup()
{
	// cleanup vertex buffer
	vkDestroyBuffer(device_handle_, vertex_buffer_, nullptr);
	vkFreeMemory(device_handle_, vertex_buffer_memory_, nullptr);

	// cleanup index buffer
	vkDestroyBuffer(device_handle_, index_buffer_, nullptr);
	vkFreeMemory(device_handle_, index_buffer_memory_, nullptr);

	// cleanup shape buffer
	vkDestroyBuffer(device_handle_, shape_buffer_, nullptr);
	vkFreeMemory(device_handle_, shape_buffer_memory_, nullptr);

	// cleanup indirect draw buffer
	vkDestroyBuffer(device_handle_, indirect_draw_buffer_, nullptr);
	vkFreeMemory(device_handle_, indirect_draw_buffer_memory_, nullptr);
}

void VulkanPrimitiveBuffer::RecordBindingCommands(VkCommandBuffer& command_buffer)
{
	VkBuffer vertex_buffers[] = { vertex_buffer_ };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
	vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanPrimitiveBuffer::RecordIndirectDrawCommands(VkCommandBuffer& command_buffer)
{
	// bind the vertex and index buffers
	VkBuffer vertex_buffers[] = { vertex_buffer_ };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
	vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);

	// issue the multi draw indirect command
	vkCmdDrawIndexedIndirect(command_buffer, indirect_draw_buffer_, 0, shape_data_.size(), sizeof(IndirectDrawCommand));
}

void VulkanPrimitiveBuffer::AddPrimitiveData(VulkanDevices* devices, uint32_t vertex_count, uint32_t index_count, VkBuffer vertices, VkBuffer indices, uint32_t& vertex_offset, uint32_t& index_offset, uint32_t& shape_index)
{
	VkDeviceSize vertex_size = vertex_count * sizeof(Vertex);
	VkDeviceSize index_size = index_count * sizeof(uint32_t);

	// return the vertex and index offsets for this primitive
	vertex_offset = last_vertex_;
	index_offset = last_index_;
	shape_index = shape_data_.size();

	// store a new entry in the shape buffer for this shape
	ShapeData shape = {
		vertex_offset,
		index_offset,
		0,
		0,
		glm::vec4(0.0, 0.0, 0.0, 0.0),
		glm::vec4(0.0, 0.0, 0.0, 0.0)
	};
	shape_data_.push_back(shape);
	
	// copy the vertex and index buffers
	devices->CopyBuffer(vertices, vertex_buffer_, vertex_size, last_vertex_ * sizeof(Vertex));
	devices->CopyBuffer(indices, index_buffer_, index_size, last_index_ * sizeof(uint32_t));

	// increment the vertex and index counts
	last_vertex_ += vertex_count;
	last_index_ += index_count;
	vertex_count_ += vertex_count;
	index_count_ += index_count;
}

void VulkanPrimitiveBuffer::AddPrimitiveData(VulkanDevices* devices, Shape* shape)
{
	VkDeviceSize vertex_size = shape->GetVertexCount() * sizeof(Vertex);
	VkDeviceSize index_size = shape->GetIndexCount() * sizeof(uint32_t);

	// return the vertex and index offsets for this primitive
	uint32_t vertex_offset = last_vertex_;
	uint32_t index_offset = last_index_;
	uint32_t shape_index = shape_data_.size();

	shape->SetVertexBufferOffset(vertex_offset);
	shape->SetIndexBufferOffset(index_offset);
	shape->SetShapeIndex(shape_index);

	// store a new entry in the shape buffer for this shape
	ShapeData shape_data = {
		vertex_offset,
		index_offset,
		shape_index,
		shape->GetIndexCount(),
		shape->GetBoundingBox().min_vertex,
		shape->GetBoundingBox().max_vertex
	};
	shape_data_.push_back(shape_data);

	// copy the vertex and index buffers
	devices->CopyBuffer(shape->GetVertexBuffer(), vertex_buffer_, vertex_size, last_vertex_ * sizeof(Vertex));
	devices->CopyBuffer(shape->GetIndexBuffer(), index_buffer_, index_size, last_index_ * sizeof(uint32_t));

	// increment the vertex and index counts
	last_vertex_ += shape->GetVertexCount();
	last_index_ += shape->GetIndexCount();
	vertex_count_ += shape->GetVertexCount();
	index_count_ += shape->GetIndexCount();
}