#include "mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <iostream>
#include "renderer.h"

Mesh::Mesh()
{
	world_matrix_ = glm::mat4(1.0f);
	vk_device_handle_ = VK_NULL_HANDLE;
}

Mesh::~Mesh()
{
	// clean up  shapes
	for (Shape* shape : mesh_shapes_)
	{
		shape->CleanUp();
		delete shape;
	}
	mesh_shapes_.clear();

	// clean up materials
	for (auto& material_pair : mesh_materials_)
	{
		material_pair.second->CleanUp();
		delete material_pair.second;
		material_pair.second = nullptr;
	}
	mesh_materials_.clear();

	vk_device_handle_ = VK_NULL_HANDLE;
}

void Mesh::UpdateWorldMatrix(glm::mat4 world_matrix)
{
	world_matrix_ = world_matrix;
}

void Mesh::CreateModelMesh(VulkanDevices* devices, VulkanRenderer* renderer, std::string filename)
{
	vk_device_handle_ = devices->GetLogicalDevice();

	renderer->GetMatrixBuffer(matrix_buffer_, matrix_buffer_memory_);

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	std::cout << "Loading model file: " << filename << std::endl;

	std::string mat_dir = "../res/materials/";

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str(), mat_dir.c_str()))
	{
		throw std::runtime_error(err);
	}

	std::cout << "Model contains " << shapes.size() << " shapes" << std::endl;

	std::cout << "Model contains " << materials.size() << " unique materials" << std::endl;

	// create the mesh materials
	for (tinyobj::material_t material : materials)
	{
		if (mesh_materials_.find(material.name) == mesh_materials_.end())
		{
			mesh_materials_[material.name] = new Material();
			mesh_materials_[material.name]->InitMaterial(devices, renderer, material);
		}
	}

	size_t index_count = 0;
	for (const tinyobj::shape_t& shape : shapes)
	{
		index_count += shape.mesh.indices.size();
	}

	std::cout << "Model contains " << index_count << " indices" << std::endl;

	int vertex_size = 0;

	for (const auto& shape : shapes)
	{
		std::unordered_map<Vertex, uint32_t> unique_vertices = {};
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		
		Shape* mesh_shape = new Shape();

		int face_index = 0;

		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex = {};


			if (index.vertex_index >= 0)
			{
				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 2],
					attrib.vertices[3 * index.vertex_index + 1]
				};
			}
			else
			{
				vertex.pos = { 0.0f, 0.0f, 0.0f };
			}

			if (index.texcoord_index >= 0)
			{
				vertex.tex_coord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};
			}
			else
			{
				vertex.tex_coord = { 0.0f, 0.0f };
			}

			if (index.normal_index >= 0)
			{
				vertex.normal = {
					-attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 2],
					attrib.normals[3 * index.normal_index + 1]
				};
			}
			else
			{
				vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f);
			}

			// material index
			vertex.mat_index = 0;
			Material* face_material = mesh_materials_[materials[shape.mesh.material_ids[face_index]].name];
			if (face_material)
			{
				if (face_material->GetMaterialIndex() >= 0)
					vertex.mat_index = face_material->GetMaterialIndex();
			}

			if (unique_vertices.count(vertex) == 0)
			{
				unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(unique_vertices[vertex]);

			if (indices.size() % 3 == 0)
				face_index++;
		}

		vertex_size += vertices.size();

		mesh_shape->InitShape(devices, renderer, vertices, indices);
		mesh_shapes_.push_back(mesh_shape);
	}

	std::cout << "Mesh contains " << vertex_size << " unique vertices" << std::endl;

}

void Mesh::RecordRenderCommands(VkCommandBuffer& command_buffer)
{
	for (Shape* shape : mesh_shapes_)
	{
		shape->RecordRenderCommands(command_buffer);
	}
}