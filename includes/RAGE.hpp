#pragma once

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "nanogui/nanogui.h"

#include "RAGE_window.hpp"
#include "RAGE_camera.hpp"

#include "ShaderLoader.hpp"

#include <iostream>

using namespace nanogui;

class RAGE
{
public:
	RAGE_window	*window;
	RAGE_camera	*camera;

	RAGE();
	~RAGE();
	Screen *nanogui_screen;
private:
	void exit_program(char *exit_message, int exit_code);
};

void set_callbacks(RAGE *rage);