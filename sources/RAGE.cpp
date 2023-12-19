#include "RAGE.hpp"

RAGE::RAGE()
{
	window = new RAGE_window();
	user_input = new RAGE_user_input();
}

RAGE::~RAGE()
{
	glfwDestroyWindow(window->glfw_window);
	glfwTerminate();
}