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
	std::function<void(void *content)> per_frame_callback = NULL;
};

class RAGE_shader
{
public:
	RAGE_shader(const std::string &vertex_path, const std::string &fragment_path);
	~RAGE_shader();

	GLint init_uniform(const char *variable_name, GLenum type, std::function<void(void *content)> set_value_per_frame);
	void update_uniforms(void *content);
	static void init_RAGE_shaders(void *content);

	GLuint program;
	std::unordered_map<std::string, RAGE_uniform> uniforms;
private:
	GLint init_uniform_location(const char *variable_name);
	std::string read_file(const std::string &file_path);
	GLuint compile(GLuint type, const std::string &source);
	void create(const std::string &vertex_string, const std::string &fragment_string);

	std::string vertex_path;
	std::string fragment_path;
};

void update_view_matrix(void *content);
void update_perspective_matrix(void *content);