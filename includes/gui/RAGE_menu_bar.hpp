#pragma once

class RAGE;

#include <string>
#include <vector>
#include "imgui.h"

struct menu_bar_item
{
	std::string name;
	std::function<void()> callback;
	std::vector<menu_bar_item> children;
};

class RAGE_menu_bar
{
public:
	RAGE_menu_bar();
	void draw(RAGE *rage);
	void draw_menu_item(const menu_bar_item& item);
private:
	std::vector<menu_bar_item> items;
	RAGE *rage;
};