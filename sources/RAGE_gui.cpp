#include "RAGE_gui.hpp"

RAGE_gui::RAGE_gui(GLFWwindow *glfw_window)
{
	nano_screen = new Screen();
	nano_screen->initialize(glfw_window, true);
}
