#pragma once

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "RAGE_window.hpp"
#include "RAGE_camera.hpp"
#include "RAGE_shader.hpp"
#include "RAGE_scene.hpp"
#include "RAGE_user_input.hpp"
#include "RAGE_mesh.hpp"

#include "gui/RAGE_gui.hpp"
#include "gui/RAGE_scene_view.hpp"

#include <iostream>
#include <filesystem>
#include <chrono>

class RAGE
{
public:
	RAGE_window *window;
	RAGE_camera camera;
	RAGE_shader *shader;
	RAGE_shader *skybox_shader;
	RAGE_gui *gui;
	RAGE_user_input *user_input;
	RAGE_scene *scene;
	double delta_time;
	std::string executable_path;

	RAGE();
	~RAGE();
	bool create_template_objects();
private:
	std::vector<RAGE_object *> template_objects;
};

RAGE *get_rage();
void set_callbacks(GLFWwindow *window, RAGE *rage);
std::string getExecutableDir();