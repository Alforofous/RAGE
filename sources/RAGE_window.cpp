#include "RAGE.hpp"

int RAGE_window::Init()
{
	primary_monitor = glfwGetPrimaryMonitor();
	if (primary_monitor == NULL)
	{
		std::cerr << "Failed to get primary monitor" << std::endl;
		return (-1);
	}
	mode = glfwGetVideoMode(primary_monitor);
	if (mode == NULL)
	{
		std::cerr << "Failed to get video mode" << std::endl;
		return (-1);
	}
	glfw_window = glfwCreateWindow(mode->width / 2, mode->height / 2, "RAGE", NULL, NULL);
	if (glfw_window == NULL)
	{
		std::cerr << "Failed to create window" << std::endl;
		return (-1);
	}
	return (1);
}

RAGE_window::~RAGE_window()
{
	glfwDestroyWindow(glfw_window);
}
