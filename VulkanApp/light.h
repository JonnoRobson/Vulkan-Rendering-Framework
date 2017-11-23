#ifndef _LIGHT_H_
#define _LIGHT_H_

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class VulkanDevices;

struct LightData
{
	glm::vec4 position;
	glm::vec4 direction;
	glm::vec4 color;
	float range;
	float intensity;
	float light_type;
	float shadows_enabled;
};

class Light
{
public:
	Light();

	inline void SetPosition(glm::vec4 position) { position_ = position; }
	inline glm::vec4 GetPosition() { return position_; }

	inline void SetDirection(glm::vec4 direction) { direction_ = direction; }
	inline glm::vec4 GetDirection() { return direction_; }

	inline void SetColor(glm::vec4 color) { color_ = color; }
	inline glm::vec4 GetColor() { return color_; }

	inline void SetRange(float range) { range_ = range; }
	inline float GetRange() { return range_; }

	inline void SetIntensity(float intensity) { intensity_ = intensity; }
	inline float GetIntensity() { return intensity_; }

	inline void SetType(float type) { type_ = type; }
	inline float GetType() { return type_; }

	inline void SetShadowsEnabled(bool shadows_enabled) { shadows_enabled_ = shadows_enabled; }
	inline bool GetShadowsEnabled() { return shadows_enabled_; }

	void SendLightData(VulkanDevices* devices, VkDeviceMemory light_buffer_memory);

	void SetLightBufferIndex(uint16_t index) { light_buffer_index_ = index; }
	uint16_t GetLightBufferIndex() { return light_buffer_index_; }

protected:
	glm::vec4 position_;
	glm::vec4 direction_;
	glm::vec4 color_;
	
	float range_;
	float intensity_;
	float type_;
	bool shadows_enabled_;
	
	uint16_t light_buffer_index_;
};

#endif