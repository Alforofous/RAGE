#include "RAGE.hpp"

RAGE::RAGE()
{
	window = new RAGE_window();
	camera = new RAGE_camera();
	nanogui_screen = NULL;
}

void RAGE::exit_program(char *exit_message, int exit_code)
{
}

RAGE::~RAGE()
{
	delete window;
	delete camera;
	glfwTerminate();
}
