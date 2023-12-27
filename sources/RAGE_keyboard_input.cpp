#include "RAGE.hpp"

void RAGE_keyboard_input::signal(void *param, int key)
{
	RAGE *rage = (RAGE *)param;

	if (key == GLFW_KEY_F3)
		rage->gui->show_performance_window = !rage->gui->show_performance_window;
	if (key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(rage->window->glfw_window, GLFW_TRUE);
#if __APPLE__
	if (rage->user_input->keyboard.pressed_keys[GLFW_KEY_LEFT_SUPER] == true && key == GLFW_KEY_A)
		rage->scene.select_all_objects();
#else
	if (rage->user_input->keyboard.pressed_keys[GLFW_KEY_LEFT_CONTROL] == true && key == GLFW_KEY_A)
		rage->scene.select_all_objects();
#endif
}