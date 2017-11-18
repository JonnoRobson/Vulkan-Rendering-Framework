#ifndef _MATERIAL_BUFFER_H_
#define _MATERIAL_BUFFER_H_

#include "device.h"

#define MAX_MATERIAL_COUNT 500

class VulkanMaterialBuffer
{
public:
	
	void InitMaterialBuffer(VulkanDevices* devices, uint32_t material_size);
	void CleanUp();
	void AddMaterialData(void* material_data, uint32_t material_count, uint32_t& material_offset);
	
	VkBuffer GetBuffer() { return material_buffer_; }

protected:
	VulkanDevices* devices_;

	uint32_t material_size_;
	uint32_t material_count_;

	VkBuffer material_buffer_;
	VkDeviceMemory material_buffer_memory_;
};

#endif