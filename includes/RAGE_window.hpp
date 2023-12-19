#pragma once
#include "GLFW/glfw3.h"

class RAGE_window
{
public:
	GLFWwindow *glfw_window;
	~RAGE_window();
	bool init();
	glm::ivec2 get_window_pixel_size() const;
	int get_window_pixel_width() const;
	int get_window_pixel_height() const;

private:
	GLFWmonitor *primary_monitor;
	const GLFWvidmode *mode;
	glm::ivec2 window_pixel_size;
};
