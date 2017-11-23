#include "material.h"
#include "renderer.h"
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
}

void Material::CleanUp()
{
	// clean up textures
	if (ambient_texture_ && ambient_texture_ != default_texture_)
	{
		ambient_texture_->Cleanup();
		delete ambient_texture_;
		ambient_texture_ = nullptr;
	}

	if (diffuse_texture_ && diffuse_texture_ != default_texture_)
	{
		diffuse_texture_->Cleanup();
		delete diffuse_texture_;
		diffuse_texture_ = nullptr;
	}

	if (specular_texture_ && specular_texture_ != default_texture_)
	{
		specular_texture_->Cleanup();
		delete specular_texture_;
		specular_texture_ = nullptr;
	}

	if (specular_highlight_texture_ && specular_highlight_texture_ != default_texture_)
	{
		specular_highlight_texture_->Cleanup();
		delete specular_highlight_texture_;
		specular_highlight_texture_ = nullptr;
	}

	if (emissive_texture_ && emissive_texture_ != default_texture_)
	{
		emissive_texture_->Cleanup();
		delete emissive_texture_;
		emissive_texture_ = nullptr;
	}

	if (bump_texture_ && bump_texture_ != default_texture_)
	{
		bump_texture_->Cleanup();
		delete bump_texture_;
		bump_texture_ = nullptr;
	}

	if (displacement_texture_ && displacement_texture_ != default_texture_)
	{
		displacement_texture_->Cleanup();
		delete displacement_texture_;
		displacement_texture_ = nullptr;
	}

	if (alpha_texture_ && alpha_texture_ != default_texture_)
	{
		alpha_texture_->Cleanup();
		delete alpha_texture_;
		alpha_texture_ = nullptr;
	}

	if (reflection_texture_ && reflection_texture_ != default_texture_)
	{
		reflection_texture_->Cleanup();
		delete reflection_texture_;
		reflection_texture_ = nullptr;
	}
}

void Material::InitMaterial(VulkanDevices* devices, VulkanRenderer* renderer, tinyobj::material_t& material)
{
	VulkanSwapChain* swap_chain = renderer->GetSwapChain();
	VulkanPrimitiveBuffer* primitive_buffer = renderer->GetPrimitiveBuffer();
	default_texture_ = renderer->GetDefaultTexture();

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

	std::string tex_dir = "../res/materials/";

	// load material textures
	if (!material.ambient_texname.empty())
	{
		ambient_texture_ = new Texture();
		ambient_texture_->Init(devices, tex_dir + material.ambient_texname);
	}
	else
	{
		ambient_texture_ = default_texture_;
	}

	if (!material.diffuse_texname.empty())
	{
		diffuse_texture_ = new Texture();
		diffuse_texture_->Init(devices, tex_dir + material.diffuse_texname);
		renderer->AddDiffuseTexture(diffuse_texture_);
		material_properties_.diffuse_map_index = diffuse_texture_->GetTextureIndex();
	}
	else
	{
		diffuse_texture_ = default_texture_;
	}

	if (!material.specular_texname.empty())
	{
		specular_texture_ = new Texture();
		specular_texture_->Init(devices, tex_dir + material.specular_texname);
	}
	else
	{
		specular_texture_ = default_texture_;
	}

	if (!material.specular_highlight_texname.empty())
	{
		specular_highlight_texture_ = new Texture();
		specular_highlight_texture_->Init(devices, tex_dir + material.specular_highlight_texname);
	}
	else
	{
		specular_highlight_texture_ = default_texture_;
	}

	if (!material.emissive_texname.empty())
	{
		emissive_texture_ = new Texture();
		emissive_texture_->Init(devices, tex_dir + material.emissive_texname);
	}
	else
	{
		emissive_texture_ = default_texture_;
	}

	if (!material.bump_texname.empty())
	{
		bump_texture_ = new Texture();
		bump_texture_->Init(devices, tex_dir + material.bump_texname); 
		renderer->AddNormalTexture(bump_texture_);
		material_properties_.bump_map_index = bump_texture_->GetTextureIndex();
	}
	else
	{
		bump_texture_ = default_texture_;
	}

	if (!material.displacement_texname.empty())
	{
		displacement_texture_ = new Texture();
		displacement_texture_->Init(devices, tex_dir + material.displacement_texname);
	}
	else
	{
		displacement_texture_ = default_texture_;
	}

	if (!material.alpha_texname.empty())
	{
		alpha_texture_ = new Texture();
		alpha_texture_->Init(devices, tex_dir + material.alpha_texname);
	}
	else
	{
		alpha_texture_ = default_texture_;
	}

	if (!material.reflection_texname.empty())
	{
		reflection_texture_ = new Texture();
		reflection_texture_->Init(devices, tex_dir + material.reflection_texname);
	}
	else
	{
		reflection_texture_ = default_texture_;
	}

	// add the material data to the material buffer
	renderer->GetMaterialBuffer()->AddMaterialData(&material_properties_, 1, material_buffer_index_);
}