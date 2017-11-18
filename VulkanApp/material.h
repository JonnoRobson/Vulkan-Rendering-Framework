#ifndef _MATERIAL_H_
#define _MATERIAL_H_

#include <tiny_obj_loader.h>
#include <glm/glm.hpp>

#include "texture.h"
#include "pipeline.h"

class VulkanRenderer;

struct MaterialData
{
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
	glm::vec4 transmittance;
	glm::vec4 emissive;
	float shininess;
	float ior;
	float dissolve;
	float illum;
};

class Material
{
public:
	Material();

	virtual void InitMaterial(VulkanDevices* devices, VulkanRenderer* renderer, tinyobj::material_t& material);	// init material from tinyobj material struct (virtual to allow pbr extension)
	virtual void CleanUp();

	uint32_t GetMaterialIndex() { return material_buffer_index_; }

protected:
	// material name
	std::string material_name_;
	
	// default texture for any missing textures
	Texture* default_texture_;

	// material buffer index
	uint32_t material_buffer_index_;

	// material properties
	MaterialData material_properties_;

	// material textures
	Texture* ambient_texture_;
	Texture* diffuse_texture_;
	Texture* specular_texture_;
	Texture* specular_highlight_texture_;
	Texture* emissive_texture_;
	Texture* bump_texture_;
	Texture* displacement_texture_;
	Texture* alpha_texture_;
	Texture* reflection_texture_;
};

#endif