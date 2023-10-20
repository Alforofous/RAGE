#include "RAGE.hpp"

RAGE_gui::RAGE_gui(GLFWwindow *glfw_window)
{
	nano_screen = new Screen();
	nano_screen->initialize(glfw_window, true);

	scene_view = new RAGE_scene_view(nano_screen);
	int	width;
	int	height;
	glfwGetWindowSize(glfw_window, &width, &height);
	scene_view->setSize({ width / 2, height / 2});
	scene_view->setPosition(Vector2i(50, 350));
	scene_view->setBackgroundColor(Vector4i(0, 0, 0, 250));
	scene_view->setDrawBorder(true);

	bool bvar = true;
	FormHelper *gui = new FormHelper(nano_screen);

	Window *nanoguiWindow = gui->addWindow(Vector2i(10, 10), "Form helper example");
	gui->addGroup("Basic types");
	gui->addVariable("bool", bvar)->setTooltip("Test tooltip.");
	gui->addGroup("TEST");

	debug_text_box = new TextBox(nano_screen, "00000ms");

	nano_screen->performLayout();
}