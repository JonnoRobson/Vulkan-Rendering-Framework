#include "material.h"
#include "renderer.h"
#include "texture_cache.h"
#include <chrono>

Material::Material()
{
	material_properties_.ambient = glm::vec4(0.0f);
	material_properties_.diffuse = glm::vec4(0.0f);
	material_properties_.specular = glm::vec4(0.0f);
	material_properties_.transmittance = glm::vec4(0.0f);
	material_properties_.emissive = glm::vec4(0.0f);
	material_properties_.shininess = 0.0f;
	material_properties_.dissolve = 0.0f;
	material_properties_.ior = 0.0f;
	material_properties_.illum = 0;
	material_properties_.ambient_map_index = 0;
	material_properties_.diffuse_map_index = 0;
	material_properties_.specular_map_index = 0;
	material_properties_.specular_highlight_map_index = 0;
	material_properties_.emissive_map_index = 0;
	material_properties_.bump_map_index = 0;
	material_properties_.alpha_map_index = 0;
	material_properties_.reflection_map_index = 0;

	ambient_texture_ = nullptr;
	diffuse_texture_ = nullptr;
	specular_texture_ = nullptr;
	specular_highlight_texture_ = nullptr;
	emissive_texture_ = nullptr;
	bump_texture_ = nullptr;
	displacement_texture_ = nullptr;
	alpha_texture_ = nullptr;
	reflection_texture_ = nullptr;

	material_buffer_index_ = 0;
	transparency_enabled_ = false;
}

void Material::CleanUp()
{
	// clean up textures
	if (ambient_texture_ && ambient_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(ambient_texture_);
	}

	if (diffuse_texture_ && diffuse_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(diffuse_texture_);
	}

	if (specular_texture_ && specular_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(specular_texture_);
	}

	if (specular_highlight_texture_ && specular_highlight_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(specular_highlight_texture_);
	}

	if (emissive_texture_ && emissive_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(emissive_texture_);
	}

	if (bump_texture_ && bump_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(bump_texture_);
	}

	if (displacement_texture_ && displacement_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(displacement_texture_);
	}

	if (alpha_texture_ && alpha_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(alpha_texture_);
	}

	if (reflection_texture_ && reflection_texture_ != default_texture_)
	{
		texture_cache_->ReleaseTexture(reflection_texture_);
	}
}

void Material::InitMaterial(VulkanDevices* devices, VulkanRenderer* renderer, tinyobj::material_t& material, std::string texture_path)
{
	VulkanSwapChain* swap_chain = renderer->GetSwapChain();
	VulkanPrimitiveBuffer* primitive_buffer = renderer->GetPrimitiveBuffer();
	default_texture_ = renderer->GetDefaultTexture();
	texture_cache_ = renderer->GetTextureCache();

	material_name_ = material.name;
	
	// load material properties
	material_properties_.ambient = glm::vec4(material.ambient[0], material.ambient[1], material.ambient[2], 1.0f);
	material_properties_.diffuse = glm::vec4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1.0f);
	material_properties_.specular = glm::vec4(material.specular[0], material.specular[1], material.specular[2], 1.0f);
	material_properties_.transmittance = glm::vec4(material.transmittance[0], material.transmittance[1], material.transmittance[2], 1.0f);
	material_properties_.emissive = glm::vec4(material.emission[0], material.emission[1], material.emission[2], 1.0f);
	material_properties_.shininess = material.shininess;
	material_properties_.ior = material.ior;
	material_properties_.dissolve = material.dissolve;
	material_properties_.illum = material.illum;

	if (material.dissolve < 1.0f)
		transparency_enabled_ = true;

	std::string tex_dir = texture_path;

	// load material textures
	if (!material.ambient_texname.empty())
	{
		ambient_texture_ = texture_cache_->LoadTexture(tex_dir + material.ambient_texname);
		material_properties_.ambient_map_index = renderer->AddTextureMap(ambient_texture_, Texture::MapType::AMBIENT);
	}
	else
	{
		ambient_texture_ = default_texture_;
	}

	if (!material.diffuse_texname.empty())
	{
		diffuse_texture_ = texture_cache_->LoadTexture(tex_dir + material.diffuse_texname);
		material_properties_.diffuse_map_index = renderer->AddTextureMap(diffuse_texture_, Texture::MapType::DIFFUSE);
	}
	else
	{
		diffuse_texture_ = default_texture_;
	}

	if (!material.specular_texname.empty())
	{
		specular_texture_ = texture_cache_->LoadTexture(tex_dir + material.specular_texname);
		material_properties_.specular_map_index = renderer->AddTextureMap(specular_texture_, Texture::MapType::SPECULAR);
	}
	else
	{
		specular_texture_ = default_texture_;
	}

	if (!material.specular_highlight_texname.empty())
	{
		specular_highlight_texture_ = texture_cache_->LoadTexture(tex_dir + material.specular_highlight_texname);
		material_properties_.specular_highlight_map_index = renderer->AddTextureMap(specular_highlight_texture_, Texture::MapType::SPECULAR_HIGHLIGHT);
	}
	else
	{
		specular_highlight_texture_ = default_texture_;
	}

	if (!material.emissive_texname.empty())
	{
		emissive_texture_ = texture_cache_->LoadTexture(tex_dir + material.emissive_texname);
		material_properties_.emissive_map_index = renderer->AddTextureMap(emissive_texture_, Texture::MapType::EMISSIVE);
	}
	else
	{
		emissive_texture_ = default_texture_;
	}

	if (!material.bump_texname.empty())
	{
		bump_texture_ = texture_cache_->LoadTexture(tex_dir + material.bump_texname);
		material_properties_.bump_map_index = renderer->AddTextureMap(bump_texture_, Texture::MapType::NORMAL);
	}
	else
	{
		bump_texture_ = default_texture_;
	}

	if (!material.displacement_texname.empty())
	{
		displacement_texture_ = texture_cache_->LoadTexture(tex_dir + material.displacement_texname);
	}
	else
	{
		displacement_texture_ = default_texture_;
	}

	if (!material.alpha_texname.empty())
	{
		alpha_texture_ = texture_cache_->LoadTexture(tex_dir + material.alpha_texname);
		material_properties_.alpha_map_index = renderer->AddTextureMap(alpha_texture_, Texture::MapType::ALPHA);
		transparency_enabled_ = true;
	}
	else
	{
		alpha_texture_ = default_texture_;
	}

	if (!material.reflection_texname.empty())
	{
		reflection_texture_ = texture_cache_->LoadTexture(tex_dir + material.reflection_texname);
		material_properties_.reflection_map_index = renderer->AddTextureMap(reflection_texture_, Texture::MapType::REFLECTION);
	}
	else
	{
		reflection_texture_ = default_texture_;
	}

	// add the material data to the material buffer
	renderer->GetMaterialBuffer()->AddMaterialData(&material_properties_, 1, material_buffer_index_);
}