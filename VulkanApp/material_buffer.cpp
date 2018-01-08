#include "material_buffer.h"
#include "material.h"

void VulkanMaterialBuffer::InitMaterialBuffer(VulkanDevices* devices, uint32_t material_size)
{
	devices_ = devices;
	material_count_ = 0;

	// calculate buffer size
	material_size_ = material_size;
	VkDeviceSize buffer_size = material_size * MAX_MATERIAL_COUNT;

	// create the material buffer
	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, material_buffer_, material_buffer_memory_);
}

void VulkanMaterialBuffer::CleanUp()
{
	// clean up the buffer
	vkDestroyBuffer(devices_->GetLogicalDevice(), material_buffer_, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), material_buffer_memory_, nullptr);
}

void VulkanMaterialBuffer::AddMaterialData(void* material_data, uint32_t material_count, uint32_t& material_offset)
{
	// create a temp staging buffer for the material data
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	VkDeviceSize buffer_size = material_count * material_size_;

	devices_->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

	// copy the data to the staging buffer
	devices_->CopyDataToBuffer(staging_buffer_memory, material_data, buffer_size, 0);

	// copy the staging buffer to the material buffer
	devices_->CopyBuffer(staging_buffer, material_buffer_, buffer_size, material_count_ * material_size_);

	// delete the staging buffer now it is no longer needed
	vkDestroyBuffer(devices_->GetLogicalDevice(), staging_buffer, nullptr);
	vkFreeMemory(devices_->GetLogicalDevice(), staging_buffer_memory, nullptr);

	// increment material count
	material_offset = material_count_;
	material_count_ += material_count;
}