#include "RAGE.hpp"
#include "RAGE_window.hpp"

static RAGE *get_rage_from_user_data(GLFWwindow *window)
{
	//For main window
	RAGE *rage = (RAGE *)glfwGetWindowUserPointer(window);

	if (rage == NULL)
	{
		//For ImGui viewport windows
		ImGuiIO *io = &ImGui::GetIO();
		if (io == NULL)
			return (NULL);
		rage = (RAGE *)io->UserData;
		if (rage == NULL)
			return (NULL);
	}
	return (rage);
}

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int modifiers)
{
}

static void cursor_position_callback(GLFWwindow *window, double x, double y)
{
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	RAGE *rage = get_rage_from_user_data(window);

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

static void window_resize_callback(GLFWwindow *window, int width, int height)
{
	RAGE *rage = get_rage_from_user_data(window);

	rage->window->pixel_size = glm::ivec2(width, height);
}

static void window_reposition_callback(GLFWwindow *window, int x, int y)
{
	RAGE *rage = get_rage_from_user_data(window);

	rage->window->pixel_position = glm::ivec2(x, y);
}

void set_callbacks(GLFWwindow *window, RAGE *rage)
{
	glfwSetWindowUserPointer(window, rage);
	glfwSetErrorCallback(error_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetFramebufferSizeCallback(window, window_resize_callback);
	glfwSetWindowPosCallback(window, window_reposition_callback);
}