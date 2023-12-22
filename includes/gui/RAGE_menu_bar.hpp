#pragma once

class RAGE;

#include <string>
#include <vector>
#include "imgui.h"

struct menu_bar_item
{
	std::string name;
	std::function<void(RAGE *rage)> callback;
	std::vector<menu_bar_item> children;
};

class RAGE_menu_bar
{
public:
	RAGE_menu_bar();
	void draw(RAGE *rage);
	void draw_menu_item(const menu_bar_item& item);
	ImVec2 get_window_size();
private:
	std::vector<menu_bar_item> items;
	RAGE *rage;
	ImVec2 window_size;
};