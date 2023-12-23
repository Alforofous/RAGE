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
		
		this->pixel_size = glm::ivec2(mode->width, mode->height);
		glfw_window = glfwCreateWindow(this->pixel_size.x, this->pixel_size.y, "RAGE", NULL, NULL);
		if (glfw_window == NULL)
			throw std::runtime_error("Failed to create window");
		glfwGetWindowPos(glfw_window, &this->pixel_position.x, &this->pixel_position.y);
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "RAGE_window error: " << e.what() << std::endl;
		return false;
	}
}

RAGE_window::~RAGE_window()
{
	glfwDestroyWindow(glfw_window);
}
