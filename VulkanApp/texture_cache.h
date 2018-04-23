#ifndef _TEXTURE_CACHE_H_
#define _TEXTURE_CACHE_H_

#include <vector>

#include "texture.h"

/**
* Stores all loaded textures to reduce instances of duplicate textures being loaded
*/
class VulkanTextureCache
{
public:
	VulkanTextureCache(VulkanDevices*);

	void Cleanup();

	Texture* LoadTexture(std::string texture_filepath);
	void ReleaseTexture(Texture*& texture);

protected:
	VulkanDevices* devices_;
	VkDeviceSize loaded_texture_size_;

	std::vector<Texture*> textures_;
};

#endif