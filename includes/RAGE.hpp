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

#include <iostream>

class RAGE
{
public:
	RAGE_window	*window;
	RAGE_camera	*camera;
	RAGE_shader	*shader;
	RAGE_gui	*gui;
	double		deltaTime;

	RAGE();
	~RAGE();
private:
	void exit_program(char *exit_message, int exit_code);
};

void set_callbacks(RAGE *rage);
void set_shader_variable_values(void *content);
