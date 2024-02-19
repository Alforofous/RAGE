#pragma once

#include "glad/glad.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <any>

#define POSITION_LAYOUT 0
#define NORMAL_LAYOUT 1
#define TEXCOORD_0_LAYOUT 2
#define COLOR_0_LAYOUT 3
#define TEXCOORD_1_LAYOUT 4
#define JOINTS_0_LAYOUT 5
#define WEIGHTS_0_LAYOUT 6
#define TANGENT_LAYOUT 7

struct RAGE_uniform
{
	GLint location = -1;
	GLenum type = GL_NONE;
	std::function<void(GLint location, void *content)> update = NULL;
};

class RAGE_shader
{
public:
	RAGE_shader(const char *vertex_path, const char *fragment_path);
	~RAGE_shader();

	GLint init_uniform(const char *variable_name, GLenum type, std::function<void(GLint location, void *content)> update);
	void update_uniforms(void *content);
	void update_uniform(const char *variable_name, void *content);
	static void init_RAGE_shaders(void *content);

	GLuint program;
	std::unordered_map<std::string, RAGE_uniform> uniforms;
private:
	GLint init_uniform_location(const char *variable_name);
	std::string read_file(const char *file_path);
	GLuint compile(GLuint type, const char *source);
	void create(const char *vertex_string, const char *fragment_string);

	const char *vertex_path;
	const char *fragment_path;
};

void update_view_matrix(GLint location, void *content);
void update_perspective_matrix(GLint location, void *content);
void update_model_matrix(GLint location, void *content);