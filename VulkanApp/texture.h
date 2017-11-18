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

protected:
	void InitSampler(VulkanDevices* devices);

protected:
	VkImage texture_image_;
	VkDeviceMemory texture_image_memory_;
	VkImageView texture_image_view_;
	VkSampler texture_sampler_;

	VkDevice vk_device_handle_;
};

#endif