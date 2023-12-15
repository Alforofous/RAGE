#include "RAGE.hpp"
#include "RAGE_window.hpp"

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
	this->window_pixel_size = glm::ivec2(mode->width / 2, mode->height / 2);
	glfw_window = glfwCreateWindow(this->window_pixel_size.x, this->window_pixel_size.y, "RAGE", NULL, NULL);
	if (glfw_window == NULL)
	{
		std::cerr << "Failed to create window" << std::endl;
		return (-1);
	}
	return (1);
}

glm::ivec2 RAGE_window::get_window_pixel_size() const
{
	return (this->window_pixel_size);
}

int RAGE_window::get_window_pixel_width() const
{
	return (this->window_pixel_size.x);
}

int RAGE_window::get_window_pixel_height() const
{
	return (this->window_pixel_size.y);
}

RAGE_window::~RAGE_window()
{
	glfwDestroyWindow(glfw_window);
}
