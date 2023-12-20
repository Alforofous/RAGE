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
#include "RAGE_scene.hpp"
#include "RAGE_user_input.hpp"
#include "RAGE_mesh.hpp"

#include <iostream>
#include <filesystem>
#include <chrono>

class RAGE
{
public:
	RAGE_window *window;
	RAGE_camera camera;
	RAGE_shader *shader;
	RAGE_gui *gui;
	RAGE_user_input *user_input;
	RAGE_scene scene;
	double delta_time;
	std::string executable_path;

	RAGE();
	~RAGE();

private:
};

void set_callbacks(RAGE *rage);
void set_shader_variable_values(void *content);
std::string getExecutableDir();