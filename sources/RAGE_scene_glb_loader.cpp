#include "RAGE_scene.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>
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

nlohmann::json RAGE_scene::read_scene_info_GLB(nlohmann::json &json)
{
	if (json["scene"].is_null() || json["scenes"].is_null())
		throw std::runtime_error("No default scene or scenes array.");
	int scene_index = json["scene"];
	if (scene_index < 0 || scene_index >= json["scenes"].size())
		throw std::runtime_error("Invalid scene index.");
	nlohmann::json json_scene = json["scenes"][scene_index];
	std::string scene_name = "GLB Scene";
	if (json_scene["name"].is_null())
		this->name = scene_name;
	else
		this->name = json_scene["name"];
	return (json_scene);
}

void RAGE_scene::read_indices_GLB(nlohmann::json &json, nlohmann::json &primitive, RAGE_object *object)
{
	if (primitive["indices"].is_null())
		return;

	int indices_accessor_index = primitive["indices"];
	if (json["accessors"][indices_accessor_index].is_null())
		return;
	nlohmann::json &indices_accessor = json["accessors"][indices_accessor_index];
	if (indices_accessor["componentType"].is_null())
		return;
	int componentType = indices_accessor["componentType"];
	if (componentType == 5123)
	{
		printf("UNSIGNED_SHORT\n");
	}
	else if (componentType == 5125)
	{
		printf("UNSIGNED_INT\n");
	}
	else if (componentType == 5121)
	{
		printf("UNSIGNED_BYTE\n");
	}
	else
	{
		throw std::runtime_error("Indices error. Unsupported componentType.");
	}
	buffer_object *indices_buffer_object = new buffer_object();
	// Now you can use indices_accessor to set your EBO
	printf("indices_accessor: %s\n", indices_accessor.dump().c_str());
}

void RAGE_scene::read_node_mesh_GLB(nlohmann::json &node, nlohmann::json &json, RAGE_object *object)
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
		read_indices_GLB(json, primitive, object);
		if (!primitive["attributes"].is_null() && !primitive["attributes"]["POSITION"].is_null())
		{
			int position_accessor_index = primitive["attributes"]["POSITION"];
			nlohmann::json &position_accessor = json["accessors"][position_accessor_index];
			// Now you can use position_accessor to set your VBO
			printf("position_accessor: %s\n", position_accessor.dump().c_str());
		}
	}
}

RAGE_object *RAGE_scene::read_scene_node_GLB(nlohmann::json &node)
{
	RAGE_object *object = new RAGE_object();
	if (object == NULL)
		throw std::runtime_error("Failed to allocate memory for object.");

	if (node["name"].is_null() == false)
		object->name = node["name"];
	if (node["mesh"].is_null())
	{
		int mesh_index = node["mesh"];
	}

	if (node["translation"].is_null() == false)
		object->position = glm::vec3(node["translation"][0], node["translation"][1], node["translation"][2]);
	if (node["rotation"].is_null() == false)
		object->rotation = glm::vec3(node["rotation"][1], node["rotation"][0], node["rotation"][2]);
	if (node["scale"].is_null() == false)
		object->scale = glm::vec3(node["scale"][0], node["scale"][1], node["scale"][2]);

	printf("object->name: %s\n", object->name.c_str());
	printf("object->position: %f, %f, %f\n", object->position.x, object->position.y, object->position.z);
	printf("object->rotation: %f, %f, %f\n", object->rotation.x, object->rotation.y, object->rotation.z);
	printf("object->scale: %f, %f, %f\n", object->scale.x, object->scale.y, object->scale.z);
	return (object);
}

void RAGE_scene::read_scene_nodes_GLB(nlohmann::json &json, nlohmann::json &json_scene)
{
	if (json["nodes"].is_null())
		throw std::runtime_error("No nodes present.");

	for (int i = 0; i < json["nodes"].size(); i++)
	{
		nlohmann::json &node = json["nodes"][i];
		RAGE_object *object = read_scene_node_GLB(node);
		this->objects.push_back(object);
		read_node_mesh_GLB(node, json, object);
	}
	for (int i = 0; i < this->objects.size(); i++)
	{
		if (json["nodes"][i]["children"].is_null())
			continue;
		for (int j = 0; j < json["nodes"][i]["children"].size(); j++)
		{
			int child_index = json["nodes"][i]["children"][j];
			this->objects[i]->children_indices.push_back(child_index);
		}
	}
	printf("OBJECT COUNT: %zu\n", this->objects.size());
	for (int i = 0; i < this->objects.size(); i++)
	{
		RAGE_object *object = this->objects[i];
		object->print_name(object);
		for (int j = 0; j < object->children_indices.size(); j++)
		{
			int child_index = object->children_indices[j];
			printf("	->child_index: %d\n", child_index);
		}
	}
}

bool RAGE_scene::load_from_GLB(const char *path)
{
	std::ifstream file;
	std::vector<char> json_header_buffer;
	std::vector<char> binary_buffer;

	try
	{
		this->delete_objects();
		file.open(path, std::ios::binary);
		if (!file)
			throw std::runtime_error("Failed to open file.");
		if (check_glb_header(file) == false)
			throw std::runtime_error("Invalid GLB file.");
		if (check_glb_chunk_header(file, json_header_buffer, 0x4E4F534A) == false)
			throw std::runtime_error("Invalid GLB json header.");
		if (check_glb_chunk_header(file, binary_buffer, 0x004E4942) == false)
			throw std::runtime_error("Invalid GLB binary buffer.");

		printf("\n**********%s**********\n", path);
		printf("***json_header:\n %s\n", json_header_buffer.data());
		printf("binary_buffer[%zu]:\n %s\n", binary_buffer.size(), binary_buffer.data());

		nlohmann::json json = nlohmann::json::parse(json_header_buffer.begin(), json_header_buffer.end());
		nlohmann::json json_scene = read_scene_info_GLB(json);
		read_scene_nodes_GLB(json, json_scene);
		for (int i = 0; i < json["bufferViews"].size(); i++)
		{
			nlohmann::json &buffer_view = json["bufferViews"][i];
			printf("buffer_view[%d]: %s\n", i, buffer_view.dump().c_str());
		}
		return (true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "RAGE_scene::load_from_GLB at \"" << path << "\" has error: " << e.what() << std::endl;
		return (false);
	}
}