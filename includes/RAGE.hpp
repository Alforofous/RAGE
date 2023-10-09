#pragma once

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "glad/glad.h"
#include "glm/glm.hpp"

#include "RAGE_window.hpp"
#include "RAGE_camera.hpp"

#include "ShaderLoader.hpp"

#include <iostream>

class RAGE
{
public:
	RAGE_window	*window;
	RAGE_camera	*camera;

	RAGE();
	~RAGE();
private:
	void exit_program(char *exit_message, int exit_code);
};