#include "RAGE.hpp"

RAGE::RAGE()
{
	window = new RAGE_window();
	user_input = new RAGE_user_input();
	scene = new RAGE_scene();
}

bool RAGE::create_template_objects()
{
	return false;
}

RAGE::~RAGE()
{
	glfwDestroyWindow(window->glfw_window);
	glfwTerminate();
}