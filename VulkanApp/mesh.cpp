#include "mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <iostream>
#include <thread>
#include "renderer.h"

Mesh::Mesh()
{
	world_matrix_ = glm::mat4(1.0f);
	vk_device_handle_ = VK_NULL_HANDLE;
	most_complex_shape_size_ = 0;
	min_vertex_ = glm::vec3(1e9f, 1e9f, 1e9f);
	max_vertex_ = glm::vec3(-1e9f, -1e9f, -1e9f);
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

	
	size_t index_count = 0;
	for (const tinyobj::shape_t& shape : shapes)
	{
		index_count += shape.mesh.indices.size();
	}

	std::cout << "Model contains " << index_count << " indices" << std::endl;

	if(renderer)
		mat_dir = renderer->GetTextureDirectory();

	// create the mesh materials
	for (tinyobj::material_t material : materials)
	{
		if (mesh_materials_.find(material.name) == mesh_materials_.end())
		{
			mesh_materials_[material.name] = new Material();
			mesh_materials_[material.name]->InitMaterial(devices, renderer, material, mat_dir);
		}
	}

	// multithread shape loading
	const int thread_count = 10;
	std::thread shape_threads[thread_count];

	// determine how many shapes are calculated per thread
	const int shapes_per_thread = shapes.size() / thread_count;
	int leftover_shapes = shapes.size() % thread_count;

	// create mutex for sending shape data to the renderer
	std::mutex shape_mutex;

	int shape_index = 0;

	// send the shapes to the threads
	for (int thread_index = 0; thread_index < thread_count; thread_index++)
	{
		std::vector<tinyobj::shape_t*> thread_shapes;

		
		// determine the shapes that will be sent to this thread
		for (int i = 0; i < shapes_per_thread; i++)
		{
			thread_shapes.push_back(&shapes[shape_index]);
			shape_index++;
		}

		// if there are still leftover shapes add one to this vector
		if (leftover_shapes > 0)
		{
			thread_shapes.push_back(&shapes[shape_index]);
			shape_index++;
			leftover_shapes--;
		}

		// start the thread for this set of shapes
		shape_threads[thread_index] = std::thread(&Mesh::LoadShapeThreaded, this, &shape_mutex, devices, renderer, &attrib, &materials, thread_shapes);
	}

	for (int i = 0; i < thread_count; i++)
	{
		shape_threads[i].join();
	}

	std::cout << "The most complex shape contains " << most_complex_shape_size_ << " triangles.\n";
}

glm::vec2 Mesh::SpheremapEncode(glm::vec3 normal)
{
	glm::vec2 enc = (normal.x == 0 && normal.y == 0) ? glm::vec2(0.0f, 0.0f) : glm::normalize(glm::vec2(normal.x, normal.y));
	enc = enc * (sqrt(-normal.z * 0.5f + 0.5f));
	enc = enc * 0.5f + glm::vec2(0.5f, 0.5f);
	return enc;
}

void Mesh::LoadShapeThreaded(std::mutex* shape_mutex, VulkanDevices* devices, VulkanRenderer* renderer, tinyobj::attrib_t* attrib, std::vector<tinyobj::material_t>* materials, std::vector<tinyobj::shape_t*> shapes)
{
	for (const auto& shape : shapes)
	{
		std::unordered_map<Vertex, uint32_t> unique_vertices = {};
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices; 
		glm::vec4 shape_min_vertex = glm::vec4(1e9f, 1e9f, 1e9f, 0.0f);
		glm::vec4 shape_max_vertex = glm::vec4(-1e9f, -1e9f, -1e9f, 0.0f);

		Shape* mesh_shape = new Shape();

		int face_index = 0;
		bool transparency_enabled = false;

		for (const auto& index : shape->mesh.indices)
		{
			Vertex vertex = {};

			if (index.vertex_index >= 0)
			{
				vertex.pos_mat_index.x = attrib->vertices[3 * index.vertex_index + 0];
				vertex.pos_mat_index.y = attrib->vertices[3 * index.vertex_index + 2];
				vertex.pos_mat_index.z = attrib->vertices[3 * index.vertex_index + 1];
			}
			else
			{
				vertex.pos_mat_index.x = 0;
				vertex.pos_mat_index.y = 0;
				vertex.pos_mat_index.z = 0;
			}

			if (index.texcoord_index >= 0)
			{
				vertex.encoded_normal_tex.z = attrib->texcoords[2 * index.texcoord_index + 0];
				vertex.encoded_normal_tex.w = 1.0f - attrib->texcoords[2 * index.texcoord_index + 1];
			}
			else
			{
				vertex.encoded_normal_tex.z = 0;
				vertex.encoded_normal_tex.w = 0;
			}

			if (index.normal_index >= 0)
			{
				glm::vec3 normal = {
					-attrib->normals[3 * index.normal_index + 0],
					attrib->normals[3 * index.normal_index + 2],
					attrib->normals[3 * index.normal_index + 1]
				};

				glm::vec2 encoded_normal = SpheremapEncode(normal);
				vertex.encoded_normal_tex.x = encoded_normal.x;
				vertex.encoded_normal_tex.y = encoded_normal.y;
			}
			else
			{
				vertex.encoded_normal_tex.x = 0;
				vertex.encoded_normal_tex.y = 0;
			}

			// material index
			vertex.pos_mat_index.w = 0;
			if (mesh_materials_.size() > 0)
			{
				Material* face_material = mesh_materials_[(*materials)[shape->mesh.material_ids[face_index]].name];
				if (face_material)
				{
					if (face_material->GetMaterialIndex() >= 0)
					{
						vertex.pos_mat_index.w = face_material->GetMaterialIndex();
						if (!transparency_enabled)
							transparency_enabled = face_material->GetTransparencyEnabled();
					}
				}
			}

			if (unique_vertices.count(vertex) == 0)
			{
				unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);

				// test to see if this vertex is outside the current shape bounds
				if (vertex.pos_mat_index.x < shape_min_vertex.x)
					shape_min_vertex.x = vertex.pos_mat_index.x;
				else if (vertex.pos_mat_index.x > shape_max_vertex.x)
					shape_max_vertex.x = vertex.pos_mat_index.x;

				if (vertex.pos_mat_index.y < shape_min_vertex.y)
					shape_min_vertex.y = vertex.pos_mat_index.y;
				else if (vertex.pos_mat_index.y > shape_max_vertex.y)
					shape_max_vertex.y = vertex.pos_mat_index.y;

				if (vertex.pos_mat_index.z < shape_min_vertex.z)
					shape_min_vertex.z = vertex.pos_mat_index.z;
				else if (vertex.pos_mat_index.z > shape_max_vertex.z)
					shape_max_vertex.z = vertex.pos_mat_index.z;
			}

			indices.push_back(unique_vertices[vertex]);

			if (indices.size() % 3 == 0)
				face_index++;
		}

		// test to see if this shape is outside the current mesh bounds
		if (shape_min_vertex.x < min_vertex_.x)
			min_vertex_.x = shape_min_vertex.x;
		
		if (shape_max_vertex.x > max_vertex_.x)
			max_vertex_.x = shape_max_vertex.x;

		if (shape_min_vertex.y < min_vertex_.y)
			min_vertex_.y = shape_min_vertex.y;
		
		if (shape_max_vertex.y > max_vertex_.y)
			max_vertex_.y = shape_max_vertex.y;

		if (shape_min_vertex.z < min_vertex_.z)
			min_vertex_.z = shape_min_vertex.z;
		
		if (shape_max_vertex.z > max_vertex_.z)
			max_vertex_.z = shape_max_vertex.z;


		most_complex_shape_size_ = std::max(most_complex_shape_size_, (uint32_t)indices.size() / 3);
		BoundingBox shape_bounding_box = {shape_min_vertex, shape_max_vertex};
		std::unique_lock<std::mutex> shape_lock(*shape_mutex);
		mesh_shape->InitShape(devices, renderer, vertices, indices, shape_bounding_box, transparency_enabled);
		mesh_shapes_.push_back(mesh_shape);
		shape_lock.unlock();
	}
}

void Mesh::RecordRenderCommands(VkCommandBuffer& command_buffer, RenderStage render_stage)
{
	switch (render_stage)
	{
	case RenderStage::OPAQUE:
	{
		for (Shape* shape : mesh_shapes_)
		{
			if(shape->GetTransparencyEnabled() == false)
				shape->RecordRenderCommands(command_buffer);
		}
		break;
	}
	case RenderStage::TRANSPARENT:
	{
		for (Shape* shape : mesh_shapes_)
		{
			if (shape->GetTransparencyEnabled() == true)
				shape->RecordRenderCommands(command_buffer);
		}
		break;
	}
	case RenderStage::GENERIC:
	{
		for (Shape* shape : mesh_shapes_)
		{
			shape->RecordRenderCommands(command_buffer);
		}
		break;
	}
	}

}