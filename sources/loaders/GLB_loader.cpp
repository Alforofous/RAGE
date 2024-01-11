#include "loaders/GLB_loader.hpp"
#include "loaders/GLB_utilities.hpp"
#include "loaders/GLB_attribute_buffer.hpp"
#include "buffer_object.hpp"

#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <queue>

static bool check_glb_header(std::ifstream &file)
{
	GLB_header header;

	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (header.magic != 0x46546C67)
		return (false);
	return (true);
}

static bool check_glb_chunk_header(std::ifstream &file, std::vector<char> &header_buffer, uint32_t type)
{
	GLB_chunk_header chunk_header;

	file.read(reinterpret_cast<char *>(&chunk_header), sizeof(chunk_header));
	if (chunk_header.type != type)
		return (false);
	header_buffer.resize(chunk_header.length);
	file.read(header_buffer.data(), header_buffer.size());
	return (true);
}

void GLB_loader::load_scene_info(RAGE_scene *scene, size_t scene_index)
{
	nlohmann::json json_scene = this->json["scenes"][scene_index];
	if (json_scene["nodes"].is_null())
		throw std::runtime_error("No nodes present in scene at index: " + std::to_string(scene_index) + ".");
	this->load_nodes(scene, json_scene);
}

RAGE_scene *GLB_loader::load_scene_at_index(size_t scene_index)
{
	nlohmann::json json_scene = this->json["scenes"][scene_index];
	std::string scene_name = "GLB Scene";
	if (json_scene["name"].is_null() == false)
		scene_name = json_scene["name"];
	RAGE_scene *scene = new RAGE_scene(scene_name.c_str());
	if (scene == NULL)
		throw std::runtime_error("Failed to allocate memory for scene.");
	return (scene);
}

void GLB_loader::load_glb_attribute_buffers(nlohmann::json &primitive, RAGE_object *object, int primitive_index)
{
	if (primitive["attributes"].is_null())
		return;
	nlohmann::json &attributes = primitive["attributes"];

	std::vector<RAGE_primitive *> *primitives = &object->mesh->primitives;
	if (primitives->size() <= primitive_index)
	{
		RAGE_primitive *primitive = new RAGE_primitive();
		primitives->push_back(primitive);
	}
	RAGE_primitive *current_primitive = (*primitives)[primitive_index];
	if (current_primitive->attribute_buffers.size() > 0)
	{
		for (size_t count = 0; count < current_primitive->attribute_buffers.size(); count += 1)
		{
			if (current_primitive->attribute_buffers[count] != NULL)
				delete current_primitive->attribute_buffers[count];
		}
		current_primitive->attribute_buffers.clear();
	}

	for (nlohmann::json::iterator it = attributes.begin(); it != attributes.end(); ++it)
	{
		std::string key = it.key();
		int value = it.value();

		if (this->json["accessors"][value].is_null())
			continue;
		nlohmann::json &accessor = this->json["accessors"][value];

		if (accessor["bufferView"].is_null())
			continue;
		int buffer_view_index = accessor["bufferView"];

		if (this->json["bufferViews"][buffer_view_index].is_null())
			continue;
		nlohmann::json &buffer_view = this->json["bufferViews"][buffer_view_index];

		if (accessor["componentType"].is_null())
			continue;
		int componentType = accessor["componentType"];

		if (accessor["type"].is_null())
			continue;
		std::string attribute_type = accessor["type"];

		if (accessor["count"].is_null())
			continue;
		size_t count = accessor["count"];

		int byte_offset = 0;
		if (accessor["byteOffset"].is_null() == false)
			byte_offset += accessor["byteOffset"].get<int>();
		if (buffer_view["byteOffset"].is_null() == false)
			byte_offset += buffer_view["byteOffset"].get<int>();

		GLenum gl_data_type = GLB_utilities::component_type_to_gl_type(componentType);
		if (gl_data_type == GL_NONE)
			continue;

		GLsizeiptr component_count = GLB_utilities::get_attribute_component_count(attribute_type);
		GLB_attribute_buffer *attribute_buffer = new GLB_attribute_buffer(this->binary_buffer.data(), byte_offset, count / component_count, key, component_count, gl_data_type);
		if (attribute_buffer == NULL)
			throw std::runtime_error("Failed to allocate memory for attribute buffer.");
		current_primitive->attribute_buffers.push_back(attribute_buffer);
	}
	if (current_primitive->interleave_vbos() == false)
		throw std::runtime_error("Failed to interleave vbos for primitive: " + std::to_string(primitive_index) + ".");
}

void GLB_loader::load_primitive_ebo(nlohmann::json &primitive, RAGE_object *object, int primitive_index)
{
	if (primitive["indices"].is_null())
		return;
	int indices_accessor_index = primitive["indices"];

	if (this->json["accessors"][indices_accessor_index].is_null())
		return;
	nlohmann::json &indices_accessor = this->json["accessors"][indices_accessor_index];

	if (indices_accessor["bufferView"].is_null())
		return;
	int buffer_view_index = indices_accessor["bufferView"];

	if (this->json["bufferViews"][buffer_view_index].is_null())
		return;
	nlohmann::json &buffer_view = this->json["bufferViews"][buffer_view_index];

	if (indices_accessor["componentType"].is_null())
		return;
	int componentType = indices_accessor["componentType"];

	if (indices_accessor["count"].is_null())
		return;
	size_t count = indices_accessor["count"];

	int byte_offset = 0;
	if (indices_accessor["byteOffset"].is_null() == false)
		byte_offset += indices_accessor["byteOffset"].get<int>();
	if (buffer_view["byteOffset"].is_null() == false)
		byte_offset += buffer_view["byteOffset"].get<int>();

	std::vector<RAGE_primitive *> *primitives = &object->mesh->primitives;
	if (primitives->size() <= primitive_index)
	{
		RAGE_primitive *primitive = new RAGE_primitive();
		primitives->push_back(primitive);
	}
	RAGE_primitive *current_primitive = (*primitives)[primitive_index];

	if (primitive["name"].is_null() == false)
		current_primitive->name = primitive["name"];
	GLenum gl_data_type = GLB_utilities::component_type_to_gl_type(componentType);
	if (gl_data_type == GL_NONE)
		return;
	current_primitive->element_buffer_object = buffer_object::create_from_glb_buffer(GL_ELEMENT_ARRAY_BUFFER, this->binary_buffer, byte_offset, count, gl_data_type);
	current_primitive->indices_count = count;
	if (current_primitive->element_buffer_object == NULL)
		throw std::runtime_error("Indices error. Failed to allocate memory.");
}

void GLB_loader::load_node_mesh(nlohmann::json &node, nlohmann::json &json, RAGE_object *object)
{
	if (node["mesh"].is_null())
		return;

	int mesh_index = node["mesh"];
	nlohmann::json &mesh = json["meshes"][mesh_index];
	if (mesh["primitives"].is_null())
		return;
	if (json["accessors"].is_null())
		return;

	for (int i = 0; i < mesh["primitives"].size(); i += 1)
	{
		nlohmann::json &primitive = mesh["primitives"][i];
		load_primitive_ebo(primitive, object, i);
		load_glb_attribute_buffers(primitive, object, i);
	}
}

RAGE_object *GLB_loader::load_node(nlohmann::json &node)
{
	RAGE_object *object = new RAGE_object();
	if (object == NULL)
		throw std::runtime_error("Failed to allocate memory for object.");
	object->mesh = new RAGE_mesh();
	if (object->mesh == NULL)
		throw std::runtime_error("Failed to allocate memory for object mesh.");

	if (node["name"].is_null() == false)
		object->name = node["name"];
	if (node["mesh"].is_null())
		int mesh_index = node["mesh"];

	if (node["translation"].is_null() == false)
		object->position = glm::vec3(node["translation"][0], node["translation"][1], node["translation"][2]);
	if (node["rotation"].is_null() == false)
		object->rotation = glm::vec3(node["rotation"][1], node["rotation"][0], node["rotation"][2]);
	if (node["scale"].is_null() == false)
		object->scale = glm::vec3(node["scale"][0], node["scale"][1], node["scale"][2]);
	return (object);
}

void GLB_loader::load_nodes(RAGE_scene *scene, nlohmann::json &json_scene)
{
	std::vector<RAGE_object *> *objects = scene->get_objects();
	for (int i = 0; i < json["nodes"].size(); i++)
	{
		nlohmann::json &node = json["nodes"][i];
		RAGE_object *object = load_node(node);
		objects->push_back(object);
		load_node_mesh(node, json, object);
	}
	for (int i = 0; i < objects->size(); i++)
	{
		if (json["nodes"][i]["children"].is_null())
			continue;
		for (int j = 0; j < json["nodes"][i]["children"].size(); j++)
		{
			int child_index = json["nodes"][i]["children"][j];
			(*objects)[i]->children_indices.push_back(child_index);
		}
	}
	for (int i = 0; i < objects->size(); i++)
	{
		RAGE_object *object = (*objects)[i];
		object->print_name(object);
		for (int j = 0; j < object->children_indices.size(); j++)
		{
			int child_index = object->children_indices[j];
		}
	}
}

void GLB_loader::delete_scenes()
{
	for (size_t count = 0; count < this->scenes.size(); count += 1)
	{
		if (this->scenes[count] != NULL)
			delete this->scenes[count];
	}
	this->scenes.clear();
}

void GLB_loader::print_info()
{
	std::string debug_string = "\n*** GLB_loader::debug ***\n";
	debug_string += "scenes: " + std::to_string(this->scenes.size()) + "\n";
	for (size_t scene_index = 0; scene_index < this->scenes.size(); scene_index += 1)
	{
		debug_string += "{\n";
		RAGE_scene *scene = this->scenes[scene_index];
		debug_string += "\tindex: " + std::to_string(scene_index) + "\n";
		debug_string += "\tname: " + std::string(scene->name) + "\n";
		debug_string += "\tobjects: " + std::to_string(scene->get_objects()->size()) + "\n";

		for (size_t object_index = 0; object_index < scene->get_objects()->size(); object_index += 1)
		{
			RAGE_object *object = (*scene->get_objects())[object_index];
			debug_string += "\t{\n";
			debug_string += "\t\tindex: " + std::to_string(object_index) + "\n";
			debug_string += "\t\tname: " + std::string(object->name) + "\n";
			std::stringstream stream;
			stream << std::fixed << std::setprecision(2);
			stream << "\t\tposition: " << object->position.x << ", " << object->position.y << ", " << object->position.z;
			stream << " rotation: " << object->rotation.x << ", " << object->rotation.y << ", " << object->rotation.z;
			stream << " scale: " << object->scale.x << ", " << object->scale.y << ", " << object->scale.z << "\n";
			debug_string += stream.str();
			debug_string += "\t\tchildren_indices: " + std::to_string(object->children_indices.size()) + "\n";
			for (size_t child_index = 0; child_index < object->children_indices.size(); child_index += 1)
			{
				debug_string += "\t\t{\n";
				int child = object->children_indices[child_index];
				debug_string += "\t\t\tchild index: " + std::to_string(child) + "\n";
				if (child_index == object->children_indices.size() - 1)
					debug_string += "\t\t}\n";
				else
					debug_string += "\t\t},\n";
			}
			if (object_index == scene->get_objects()->size() - 1)
				debug_string += "\t}\n";
			else
				debug_string += "\t},\n";
			RAGE_mesh *mesh = object->mesh;
			for (size_t primitive_index = 0; primitive_index < mesh->primitives.size(); primitive_index += 1)
			{
				RAGE_primitive *primitive = mesh->primitives[primitive_index];
				debug_string += "\t\tprimitive_index: " + std::to_string(primitive_index) + "\n";
				debug_string += primitive->vertex_array_object->get_linked_attributes_data();
			}
		}
		if (scene_index == this->scenes.size() - 1)
			debug_string += "}\n";
		else
			debug_string += "},\n";
	}
	debug_string += "******************************\n";
	std::cout << debug_string;
}

RAGE_scene *GLB_loader::load(const char *path)
{
	std::ifstream file;

	try
	{
		file.open(path, std::ios::binary);
		if (!file)
			throw std::runtime_error("Failed to open file.");
		if (check_glb_header(file) == false)
			throw std::runtime_error("Invalid GLB file.");
		if (check_glb_chunk_header(file, this->json_header_buffer, 0x4E4F534A) == false)
			throw std::runtime_error("Invalid GLB json header.");
		if (check_glb_chunk_header(file, this->binary_buffer, 0x004E4942) == false)
			throw std::runtime_error("Invalid GLB binary buffer.");

		this->json = nlohmann::json::parse(this->json_header_buffer.begin(), this->json_header_buffer.end());

		if (this->json["scenes"].is_null())
			throw std::runtime_error("No scenes present.");
		this->delete_scenes();
		this->scenes.clear();
		size_t scene_count = json["scenes"].size();
		for (size_t scene_index = 0; scene_index < scene_count; scene_index += 1)
		{
			RAGE_scene *scene = this->load_scene_at_index(scene_index);
			this->load_scene_info(scene, scene_index);
			this->scenes.push_back(scene);
		}
		int default_scene_index = 0;
		if (this->json["scene"].is_null() == false)
			default_scene_index = this->json["scene"];
		RAGE_scene *default_scene = this->scenes[default_scene_index];
		//this->print_info();
		return (default_scene);
	}
	catch (const std::exception &e)
	{
		std::cerr << "GLB_loader::load with path: \"" << path << "\", has error: " << e.what() << std::endl;
		return (NULL);
	}
}