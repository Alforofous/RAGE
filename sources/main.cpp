#include "GLFW/glfw3.h"
#include <stdio.h>

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

class RAGE_window
{
public:
	RAGE_window();
private:

};

int main(void)
{
	if (glfwInit() == GLFW_FALSE)
		return (1);
	GLFWmonitor	*primary_monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode *mode = glfwGetVideoMode(primary_monitor);
	GLFWwindow	*glfw_window = glfwCreateWindow(mode->width, mode->height, "RAGE", NULL, NULL);
	glfwSetErrorCallback(error_callback);
	while (!glfwWindowShouldClose(glfw_window))
	{
		glfwPollEvents();
	}
	glfwTerminate();
	return (0);
}
