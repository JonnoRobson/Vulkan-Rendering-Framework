#ifndef _SAMPLER_H_
#define _SAMPLER_H_

#include <vulkan/vulkan.h>

#include "device.h"

class Sampler
{
public:
	void Init(VulkanDevices* devices);
};

#endif