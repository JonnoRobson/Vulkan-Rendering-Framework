#include "light.h"

#include "device.h"

Light::Light()
{
	position_ = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	direction_ = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	color_ = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	intensity_ = 0.0f;
	range_ = 0.0f;
	type_ = 0.0f;
	shadows_enabled_ = false;
	light_buffer_index_ = 0;
}

void Light::SendLightData(VulkanDevices* devices, VkDeviceMemory light_buffer_memory)
{
	LightData light_data = {};
	light_data.position = glm::vec4(position_);
	light_data.direction = glm::vec4(direction_);
	light_data.color = glm::vec4(color_);
	light_data.intensity = intensity_;
	light_data.range = range_;
	light_data.light_type = type_;
	light_data.shadows_enabled = (shadows_enabled_) ? 1.0f : 0.0f;

	VkDeviceSize offset = sizeof(glm::vec4) + (light_buffer_index_ * sizeof(LightData));

	void* mapped_data;
	vkMapMemory(devices->GetLogicalDevice(), light_buffer_memory, offset, sizeof(LightData), 0, &mapped_data);
	memcpy(mapped_data, &light_data, sizeof(LightData));
	vkUnmapMemory(devices->GetLogicalDevice(), light_buffer_memory);

}