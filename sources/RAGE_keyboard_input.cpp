#include "RAGE.hpp"

void RAGE_keyboard_input::signal(void *param, int key)
{
	RAGE *rage = (RAGE *)param;

	if (key == GLFW_KEY_F1)
		rage->scene.print_objects();
	if (key == GLFW_KEY_F3)
		rage->gui->show_performance_window = !rage->gui->show_performance_window;
	if (key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(rage->window->glfw_window, GLFW_TRUE);
}