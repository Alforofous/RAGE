#pragma once

#define IMGUI_HAS_DOCK
#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <vector>
#include "glad/glad.h"
#include "RAGE_menu_bar.hpp"
#include "RAGE_scene_view.hpp"
#include "RAGE_inspector.hpp"

class RAGE;

class RAGE_gui
{
public:
	RAGE_gui(RAGE *rage);
	~RAGE_gui();
	void draw(RAGE *rage);
	bool show_performance_window;
	RAGE_menu_bar menu_bar;
	RAGE_scene_view scene_view;
	RAGE_inspector inspector;
	glm::ivec2 dockspace_size;
	ImGuiIO *get_imgui_io();
private:
	void draw_performance_window(RAGE *rage);
	void draw_dockspace(RAGE *rage);
	void reset_dockings();
	std::vector<float> frames;
	ImGuiIO *io;
	ImGuiID dockspace_id;
	ImGuiViewport *viewport;
};