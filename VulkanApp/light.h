#ifndef _LIGHT_H_
#define _LIGHT_H_

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "shadow_map_pipeline.h"
#include "render_target.h"
#include "mesh.h"

class VulkanDevices;
class VulkanRenderer;

#define SHADOW_MAP_RESOLUTION 1024.0

struct SceneLightData
{
	glm::vec4 scene_data; // xyz - ambient color, w - light count
	glm::vec4 camera_pos;
};


struct LightData
{
	glm::vec4 position;
	glm::vec4 direction;
	glm::vec4 color;
	float range;
	float intensity;
	float light_type;
	float shadows_enabled;
	glm::mat4 view_proj_matrices[6];
	uint32_t shadow_map_index;
	uint32_t padding[3];
};

class Light
{
public:
	Light();

	void Init(VulkanDevices* devices, VulkanRenderer* renderer);
	void Cleanup();

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
	
	inline void SetLightStationary(bool stationary) { stationary_ = stationary; }
	inline bool GetLightStationary() { return stationary_; }

	inline void SetShadowMapIndex(uint32_t index) { shadow_map_index_ = index; }

	void SendLightData(VulkanDevices* devices, VkDeviceMemory light_buffer_memory);

	glm::mat4 GetViewMatrix(int index = 0);
	glm::mat4 GetProjectionMatrix();

	void SetLightBufferIndex(uint16_t index) { light_buffer_index_ = index; }
	uint16_t GetLightBufferIndex() { return light_buffer_index_; }

	void GenerateShadowMap();
	inline VulkanRenderTarget* GetShadowMap() { return shadow_map_; }

	void RecordShadowMapCommands(VkCommandPool command_pool, std::vector<Mesh*>& meshes);


protected:
	void CalculateViewMatrices();
	void CalculateProjectionMatrix();

protected:
	VulkanDevices* devices_;

	glm::vec4 position_;
	glm::vec4 direction_;
	glm::vec4 color_;

	glm::mat4 view_matrices_[6];
	glm::mat4 proj_matrix_;

	float range_;
	float intensity_;
	float type_;
	bool shadows_enabled_;
	bool stationary_;

	// shadow mapping data
	std::vector<ShadowMapPipeline*> shadow_map_pipelines_;
	VulkanRenderTarget* shadow_map_;
	uint32_t shadow_map_index_;

	std::vector<VkCommandBuffer> shadow_map_command_buffers_;
	VkDeviceMemory matrix_buffer_memory_;
	glm::vec3 scene_min_vertex_;
	glm::vec3 scene_max_vertex_;

	uint16_t light_buffer_index_;
};

#endif