#pragma once

#include "nanogui/nanogui.h"

using namespace nanogui;
class RAGE_scene_view;

class RAGE_gui
{
public:
	Screen			*nano_screen;
	TextBox			*debug_text_box;
	RAGE_scene_view	*scene_view;

	RAGE_gui(GLFWwindow *glfw_window);
private:
};