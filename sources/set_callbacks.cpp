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

	if (action == GLFW_PRESS)
	{
		rage->user_input->keyboard.signal((void *)rage, key);
		rage->user_input->keyboard.pressed_keys[key] = true;
	}
	else if (action == GLFW_RELEASE)
	{
		rage->user_input->keyboard.pressed_keys[key] = false;
	}
}

void set_callbacks(RAGE *rage)
{
	glfwSetWindowUserPointer(rage->window->glfw_window, rage);
	glfwSetErrorCallback(error_callback);
	glfwSetKeyCallback(rage->window->glfw_window, key_callback);
	glfwSetCursorPosCallback(rage->window->glfw_window, cursor_position_callback);
	glfwSetMouseButtonCallback(rage->window->glfw_window, mouse_button_callback);
}