#include "loaders/RAGE_GLB_loader.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <queue>
#include "buffer_object.hpp"

static bool check_glb_header(std::ifstream &file)
{
	GLBHeader header;

	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (header.magic != 0x46546C67)
		return (false);
	return (true);
}

static bool check_glb_chunk_header(std::ifstream &file, std::vector<char> &header_buffer, uint32_t type)
{
	GLBChunkHeader chunk_header;

	file.read(reinterpret_cast<char *>(&chunk_header), sizeof(chunk_header));
	if (chunk_header.type != type)
		return (false);
	header_buffer.resize(chunk_header.length);
	file.read(header_buffer.data(), header_buffer.size());
	return (true);
}

void RAGE_GLB_loader::load_scene_info(RAGE_scene *scene, size_t scene_index)
{
	nlohmann::json json_scene = this->json["scenes"][scene_index];
	if (json_scene["nodes"].is_null())
		throw std::runtime_error("No nodes present in scene at index: " + std::to_string(scene_index) + ".");
	this->load_nodes(scene, json_scene);
}

RAGE_scene *RAGE_GLB_loader::load_scene_at_index(size_t scene_index)
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

void RAGE_GLB_loader::load_mesh_primitive(nlohmann::json &primitive, RAGE_object *object)
{
	if (primitive["indices"].is_null())
		return;

	int indices_accessor_index = primitive["indices"];
	if (this->json["accessors"][indices_accessor_index].is_null())
		return;
	nlohmann::json &indices_accessor = this->json["accessors"][indices_accessor_index];
	printf("indices_accessor: %s\n", indices_accessor.dump().c_str());
	if (indices_accessor["bufferView"].is_null())
		return;
	int buffer_view_index = indices_accessor["bufferView"];
	if (this->json["bufferViews"][buffer_view_index].is_null())
		return;
	nlohmann::json &buffer_view = this->json["bufferViews"][buffer_view_index];
	if (buffer_view["buffer"].is_null())
		return;
	int buffer_index = buffer_view["buffer"];
	if (this->json["buffers"][buffer_index].is_null())
		return;
	nlohmann::json &buffer = this->json["buffers"][buffer_index];
	if (indices_accessor["componentType"].is_null())
		return;
	if (indices_accessor["count"].is_null())
		return;
	void *indices_data = NULL;
	size_t indices_data_size = 0;
	size_t count = indices_accessor["count"];
	int componentType = indices_accessor["componentType"];
	if (componentType == 5121)
	{
		indices_data = new unsigned char[indices_accessor["count"]];
		indices_data_size = sizeof(unsigned char);
	}
	else if (componentType == 5123)
	{
		indices_data = new unsigned short[indices_accessor["count"]];
		indices_data_size = sizeof(unsigned short);
	}
	else if (componentType == 5125)
	{
		indices_data = new unsigned int[indices_accessor["count"]];
		indices_data_size = sizeof(unsigned int);
	}
	else
		throw std::runtime_error("Indices error. Unsupported componentType.");
	if (indices_data == NULL)
		throw std::runtime_error("Indices error. Failed to allocate memory.");
	int accessors_byte_offset = 0;
	if (indices_accessor["byteOffset"].is_null() == false)
		accessors_byte_offset = indices_accessor["byteOffset"];
	int buffer_views_byte_offset = 0;
	if (buffer_view["byteOffset"].is_null() == false)
		buffer_views_byte_offset = buffer_view["byteOffset"];
	int byte_offset = accessors_byte_offset + buffer_views_byte_offset;
	std::memcpy(indices_data, &this->binary_buffer[byte_offset], count * indices_data_size);
	
}

void RAGE_GLB_loader::load_node_mesh(nlohmann::json &node, nlohmann::json &json, RAGE_object *object)
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
		load_mesh_primitive(primitive, object);
		if (!primitive["attributes"].is_null() && !primitive["attributes"]["POSITION"].is_null())
		{
			int position_accessor_index = primitive["attributes"]["POSITION"];
			nlohmann::json &position_accessor = json["accessors"][position_accessor_index];
			// Now you can use position_accessor to set your VBO
			printf("position_accessor: %s\n", position_accessor.dump().c_str());
		}
	}
}

RAGE_object *RAGE_GLB_loader::load_node(nlohmann::json &node)
{
	RAGE_object *object = new RAGE_object();
	if (object == NULL)
		throw std::runtime_error("Failed to allocate memory for object.");

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

void RAGE_GLB_loader::load_nodes(RAGE_scene *scene, nlohmann::json &json_scene)
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
			printf("	->child_index: %d\n", child_index);
		}
	}
}

void RAGE_GLB_loader::delete_scenes()
{
	for (size_t count = 0; count < this->scenes.size(); count += 1)
	{
		if (this->scenes[count] != NULL)
			delete this->scenes[count];
	}
	this->scenes.clear();
}

void RAGE_GLB_loader::print_info()
{
	std::string debug_string = "\n*** RAGE_GLB_loader::debug ***\n";
	debug_string += "json: " + this->json.dump() + "\n";
	debug_string += "json_header_buffer: " + std::string(this->json_header_buffer.begin(), this->json_header_buffer.end()) + "\n";
	debug_string += "binary_buffer size: " + std::to_string(this->binary_buffer.size()) + "\n";
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
		}
		if (scene_index == this->scenes.size() - 1)
			debug_string += "}\n";
		else
			debug_string += "},\n";
	}
	debug_string += "******************************\n";
	std::cout << debug_string;
}

RAGE_scene *RAGE_GLB_loader::load(const char *path)
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
		this->print_info();
		return (default_scene);
	}
	catch (const std::exception &e)
	{
		std::cerr << "RAGE_GLB_loader::load with path: \"" << path << "\", has error: " << e.what() << std::endl;
		return (NULL);
	}
}