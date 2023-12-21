#include "RAGE_keyboard_input.hpp"
#include "RAGE.hpp"

void RAGE_keyboard_input::signal(void *param, int key)
{
	RAGE *rage = (RAGE *)param;

	if (key == GLFW_KEY_F1)
		rage->scene.print_objects();
	if (key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(rage->window->glfw_window, GLFW_TRUE);
}