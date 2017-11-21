#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include <vulkan/vulkan.h>
#include <stdexcept>

#include "device.h"

class Texture
{
public:
	Texture();
	~Texture();

	void Init(VulkanDevices* devices, std::string filename);
	void Cleanup();

	VkImageView GetImageView() { return texture_image_view_; }
	VkSampler GetSampler() { return texture_sampler_; }

	void SetTextureIndex(uint32_t index) { texture_index_ = index; }
	uint32_t GetTextureIndex() { return texture_index_; }

protected:
	void InitSampler(VulkanDevices* devices);

protected:
	VkImage texture_image_;
	VkDeviceMemory texture_image_memory_;
	VkImageView texture_image_view_;
	VkSampler texture_sampler_;
	uint32_t texture_index_;

	VkDevice vk_device_handle_;
};

#endif