#include "RAGE.hpp"

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int modifiers)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
}

static void cursor_position_callback(GLFWwindow *window, double x, double y)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
//	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
		rage->camera->translate(rage->camera->get_forward());
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
		rage->camera->translate(rage->camera->get_forward() * -1.0f);
	if (key == GLFW_KEY_A && action == GLFW_PRESS)
		rage->camera->translate(rage->camera->get_right() * -1.0f);
	if (key == GLFW_KEY_D && action == GLFW_PRESS)
		rage->camera->translate(rage->camera->get_right());
	if (key == GLFW_KEY_Q && action == GLFW_PRESS)
		rage->camera->translate(rage->camera->get_up() * -1.0f);
	if (key == GLFW_KEY_E && action == GLFW_PRESS)
		rage->camera->translate(rage->camera->get_up());
}

void set_callbacks(RAGE *rage)
{
	glfwSetWindowUserPointer(rage->window->glfw_window, rage);
	glfwSetErrorCallback(error_callback);
	glfwSetKeyCallback(rage->window->glfw_window, key_callback);
	glfwSetCursorPosCallback(rage->window->glfw_window, cursor_position_callback);
	glfwSetMouseButtonCallback(rage->window->glfw_window, mouse_button_callback);
}