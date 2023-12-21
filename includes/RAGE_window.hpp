#pragma once
#include "GLFW/glfw3.h"

class RAGE_window
{
public:
	GLFWwindow *glfw_window;
	~RAGE_window();
	bool init();
	glm::ivec2 get_pixel_size() const;

private:
	GLFWmonitor *primary_monitor;
	const GLFWvidmode *mode;
	glm::ivec2 pixel_size;
};
