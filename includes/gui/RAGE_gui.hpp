#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <vector>
#include "glad/glad.h"
#include "RAGE_menu_bar.hpp"
#include "RAGE_scene_view.hpp"

class RAGE_scene_view;
class RAGE;

class RAGE_gui
{
public:
	RAGE_gui(RAGE *rage);
	~RAGE_gui();
	void draw(RAGE *rage);
	bool show_performance_window;
private:
	void draw_performance_window(RAGE *rage);
	void draw_inspector(RAGE *rage);
	std::vector<float> frames;
	RAGE_menu_bar menu_bar;
	RAGE_scene_view scene_view;
};