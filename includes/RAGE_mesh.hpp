#pragma once

#include "nlohmann/json.hpp"
#include "glad/glad.h"
#include <vector>

#define VERTEX_POSITION_ELEMENT_COUNT 3
#define VERTEX_COLOR_ELEMENT_COUNT 4
#define VERTEX_ARRAY_ELEMENT_COUNT (VERTEX_POSITION_ELEMENT_COUNT + VERTEX_COLOR_ELEMENT_COUNT)

class RAGE_mesh
{
public:
	RAGE_mesh();
	bool LoadGLB(const char *path);
	GLfloat *vertices;
	GLuint *indices;
	GLsizeiptr vertices_size;
	GLsizeiptr indices_size;
	std::vector<GLfloat> vertex_positions;
	std::vector<GLfloat> vertex_colors;
	void set_vertex_positions(GLfloat *vertex_positions, unsigned int count);
	void set_vertex_colors(GLfloat *vertex_colors, unsigned int count);
	void set_indices(GLuint *indices, unsigned int count);
	GLuint vertices_count;
	GLuint indices_count;
private:
	void load_model_vertex_positions(nlohmann::json &json_scene, std::vector<char> &binary_buffer);
	void load_model_vertex_colors(nlohmann::json &json_scene, std::vector<char> &binary_buffer);
	void load_model_indices(nlohmann::json &json_scene, std::vector<char> &binary_buffer);
	void set_vertex_positions();
	void set_vertex_colors();
	int vertex_color_channel_count;
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