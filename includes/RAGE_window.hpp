#include "GLFW/glfw3.h"

class RAGE_window
{
public:
	GLFWwindow			*glfw_window;
	GLFWmonitor			*primary_monitor;
	const GLFWvidmode	*mode;
	~RAGE_window();
	int Init();
private:
};
