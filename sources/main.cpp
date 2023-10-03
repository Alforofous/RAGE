#include <stdio.h>
#include "RAGE.hpp"

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

int main(void)
{
	RAGE	*rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);
	if (rage->window->Init() == -1)
		return (1);
	glfwSetErrorCallback(error_callback);
	while (!glfwWindowShouldClose(rage->window->window))
	{
		glfwPollEvents();
	}
	glfwTerminate();
	return (0);
}
