#include "RAGE.hpp"
#include "RAGE_window.hpp"

bool RAGE_window::init()
{
	try
	{
		primary_monitor = glfwGetPrimaryMonitor();
		if (primary_monitor == NULL)
			throw std::runtime_error("Failed to get primary monitor");

		mode = glfwGetVideoMode(primary_monitor);
		if (mode == NULL)
			throw std::runtime_error("Failed to get video mode");
		
		this->window_pixel_size = glm::ivec2(mode->width / 2, mode->height / 2);
		glfw_window = glfwCreateWindow(this->window_pixel_size.x, this->window_pixel_size.y, "RAGE", NULL, NULL);
		if (glfw_window == NULL)
			throw std::runtime_error("Failed to create window");
		
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "RAGE_window error: " << e.what() << std::endl;
		return false;
	}
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
