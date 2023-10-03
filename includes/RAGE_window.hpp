#include "GLFW/glfw3.h"

class RAGE_window
{
public:
	~RAGE_window();
	int Init();
	GLFWwindow *window;
	GLFWmonitor	*primary_monitor;
	const GLFWvidmode *mode;
private:
};
