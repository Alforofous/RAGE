#pragma once

#include "RAGE_scene.hpp"
#include <nlohmann/json.hpp>
#include <vector>

class GLB_loader
{
public:
	// Load a GLB file from the given path. Returns a pointer to default scene.
	// If default scene is not found, returns first available scene.
	// On fail, it returns NULL.
	RAGE_scene *load(const char *path);
	static GLenum component_type_to_gl_type(int glb_component_type);
	static int gl_type_to_component_type(GLenum gl_type);
	static GLsizeiptr sizeof_gl_data_type(GLenum gl_type);
	static GLsizeiptr get_attribute_type_size(std::string attribute_type);

private:
	void delete_scenes();
	RAGE_scene *load_scene_at_index(size_t scene_index);
	void load_scene_info(RAGE_scene *scene, size_t scene_index);
	void load_nodes(RAGE_scene *scene, nlohmann::json &json_scene);
	RAGE_object *load_node(nlohmann::json &node);
	void load_node_mesh(nlohmann::json &node, nlohmann::json &json, RAGE_object *object);
	void load_primitive_ebo(nlohmann::json &primitive, RAGE_object *object, int primitive_index);
	void load_primitive_vbo_and_vao(nlohmann::json &primitive, RAGE_object *object, int primitive_index);
	void print_info();

	std::vector<RAGE_scene *> scenes;
	std::vector<char> binary_buffer;
	std::vector<char> json_header_buffer;
	nlohmann::json json;
};

struct GLB_header
{
	uint32_t magic;
	uint32_t version;
	uint32_t length;
};

struct GLB_chunk_header
{
	uint32_t length;
	uint32_t type;
};