#pragma once

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "glad/glad.h"
#include "glm/glm.hpp"

#include "RAGE_window.hpp"
#include "RAGE_camera.hpp"
#include "RAGE_shader.hpp"
#include "RAGE_gui.hpp"
#include "RAGE_scene_view.hpp"

#include "vertex_array.hpp"
#include "vertex_buffer.hpp"
#include "element_buffer.hpp"

#include <iostream>
#include <filesystem>
#include <chrono>

class RAGE
{
public:
	RAGE_window				*window;
	RAGE_camera				*camera;
	RAGE_shader				*shader;
	RAGE_gui				*gui;
	vertex_array			*vertex_array_object;
	vertex_buffer		*vertex_buffer_object;
	element_buffer	*element_buffer_object;
	double					delta_time;
	std::string				executable_path;

	RAGE();
	~RAGE();

	void	init_gl_objects(GLfloat *vertices, GLuint *indices, GLsizeiptr vertice_size, GLsizeiptr indices_size);
private:
};

void		set_callbacks(RAGE *rage);
void		set_shader_variable_values(void *content);
std::string	getExecutableDir();