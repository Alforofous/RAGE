#include "RAGE_window.hpp"
#include "GLFW/glfw3.h"

int RAGE_window::Init()
{
	primary_monitor = glfwGetPrimaryMonitor();
	if (primary_monitor == NULL)
		return (-1);
	mode = glfwGetVideoMode(primary_monitor);
	if (mode == NULL)
		return (-1);
	window = glfwCreateWindow(mode->width, mode->height, "RAGE", NULL, NULL);
	if (window == NULL)
		return (-1);
	return (1);
}

RAGE_window::~RAGE_window()
{
	glfwDestroyWindow(window);
}
