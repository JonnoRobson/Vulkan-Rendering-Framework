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

	standalone_shape_ = true;
}

void Shape::InitShape(VulkanDevices* devices, VulkanRenderer* renderer, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, bool transparency_enabled)
{
	devices_ = devices;
	transparency_enabled_ = transparency_enabled;

	if (renderer)
		standalone_shape_ = false;

	CreateVertexBuffer(vertices);
	CreateIndexBuffer(indices);

	// add mesh to renderer primitve buffer - standalone meshes do not need to be added
	if (renderer)
	{
		renderer->GetPrimitiveBuffer()->AddPrimitiveData(devices, vertex_count_, index_count_, vertex_buffer_, index_buffer_, vertex_buffer_offset_, index_buffer_offset_);

		// free the vertex and index buffers now that they have been added to the primitive buffer
		vkDestroyBuffer(devices_->GetLogicalDevice(), vertex_buffer_, nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_, nullptr);

		vkDestroyBuffer(devices_->GetLogicalDevice(), index_buffer_, nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), index_buffer_memory_, nullptr);
	}
}

void Shape::CleanUp()
{
	if (standalone_shape_)
	{
		// free the vertex and index buffers
		vkDestroyBuffer(devices_->GetLogicalDevice(), vertex_buffer_, nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_, nullptr);

		vkDestroyBuffer(devices_->GetLogicalDevice(), index_buffer_, nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), index_buffer_memory_, nullptr);
	}
}

void Shape::CreateVertexBuffer(std::vector<Vertex>& vertices)
{
	vertex_count_ = static_cast<uint32_t>(vertices.size());
	VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

	if (standalone_shape_)
	{
		// create the vertex buffer
		devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer_, vertex_buffer_memory_);

		// create a staging buffer to copy the data from
		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_memory;

		devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging_buffer, staging_buffer_memory);
		
		// copy the data to the staging buffer
		void* data;
		vkMapMemory(devices_->GetLogicalDevice(), staging_buffer_memory, 0, buffer_size, 0, &data);
		memcpy(data, vertices.data(), (size_t)buffer_size);
		vkUnmapMemory(devices_->GetLogicalDevice(), staging_buffer_memory);

		// copy the data from the staging buffer to the vertex buffer
		devices_->CopyBuffer(staging_buffer, vertex_buffer_, buffer_size);

		// clean up the staging buffer now that it is no longer needed
		vkDestroyBuffer(devices_->GetLogicalDevice(), staging_buffer, nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), staging_buffer_memory, nullptr);
	}
	else
	{
		// create the vertex buffer as a staging buffer
		devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertex_buffer_, vertex_buffer_memory_);

		// copy the data to the staging buffer
		void* data;
		vkMapMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_, 0, buffer_size, 0, &data);
		memcpy(data, vertices.data(), (size_t)buffer_size);
		vkUnmapMemory(devices_->GetLogicalDevice(), vertex_buffer_memory_);
	}
}

void Shape::CreateIndexBuffer(std::vector<uint32_t>& indices)
{
	index_count_ = static_cast<uint32_t>(indices.size());
	VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

	if (standalone_shape_)
	{
		// create the index buffer
		devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer_, index_buffer_memory_);

		// create a staging buffer to copy the data from
		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_memory;

		devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging_buffer, staging_buffer_memory);

		// copy the data to the staging buffer
		void* data;
		vkMapMemory(devices_->GetLogicalDevice(), staging_buffer_memory, 0, buffer_size, 0, &data);
		memcpy(data, indices.data(), (size_t)buffer_size);
		vkUnmapMemory(devices_->GetLogicalDevice(), staging_buffer_memory);

		// copy the data from the staging buffer to the vertex buffer
		devices_->CopyBuffer(staging_buffer, index_buffer_, buffer_size);

		// clean up the staging buffer now that it is no longer needed
		vkDestroyBuffer(devices_->GetLogicalDevice(), staging_buffer, nullptr);
		vkFreeMemory(devices_->GetLogicalDevice(), staging_buffer_memory, nullptr);
	}
	else
	{
		// create index buffer as a staging buffer
		devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, index_buffer_, index_buffer_memory_);

		// copy the data to the staging buffer
		void* data;
		vkMapMemory(devices_->GetLogicalDevice(), index_buffer_memory_, 0, buffer_size, 0, &data);
		memcpy(data, indices.data(), (size_t)buffer_size);
		vkUnmapMemory(devices_->GetLogicalDevice(), index_buffer_memory_);
	}
}

void Shape::RecordRenderCommands(VkCommandBuffer& command_buffer)
{
	if (standalone_shape_)
	{
		// bind the vertex and index buffers
		VkBuffer vertex_buffers[] = { vertex_buffer_ };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
		
		// execute a draw command
		vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);
	}
	else
	{
		// shapes that are added to a primitive buffer simply need to execute their draw command
		vkCmdDrawIndexed(command_buffer, index_count_, 1, index_buffer_offset_, vertex_buffer_offset_, 0);
	}
}