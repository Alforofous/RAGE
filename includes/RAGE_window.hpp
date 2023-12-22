#pragma once

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

class RAGE_window
{
public:
	GLFWwindow *glfw_window;
	~RAGE_window();
	bool init();
	glm::ivec2 pixel_size;
	glm::ivec2 pixel_position;
	void resize_callback(GLFWwindow *window, int width, int height);
	void reposition_callback(GLFWwindow *window, int x, int y);
private:
	GLFWmonitor *primary_monitor;
	const GLFWvidmode *mode;
};
