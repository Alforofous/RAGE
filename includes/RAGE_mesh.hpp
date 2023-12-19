#pragma once

#include "RAGE.hpp"
#include "nlohmann/json.hpp"

#define VERTEX_ARRAY_ELEMENT_COUNT 7

class RAGE_mesh
{
public:
	int LoadGLB(const char *path);
	GLfloat *vertices;
	GLuint *indices;
	GLsizeiptr vertices_size;
	GLsizeiptr indices_size;
private:
	void load_model_vertex_positions(nlohmann::json &json_scene, std::vector<char> &binary_buffer);
	void load_model_vertex_colors(nlohmann::json &json_scene, std::vector<char> &binary_buffer);
	void load_model_indices(nlohmann::json &json_scene, std::vector<char> &binary_buffer);
	GLfloat *vertex_colors;
	GLuint vertices_count;
	GLuint vertex_color_count;
	GLuint indices_count;
};

struct GLBHeader
{
	uint32_t magic;
	uint32_t version;
	uint32_t length;
};

struct GLBChunkHeader
{
	uint32_t length;
	uint32_t type;
};