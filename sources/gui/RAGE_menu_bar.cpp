#include "RAGE_menu_bar.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

RAGE_menu_bar::RAGE_menu_bar()
{
	menu_bar_item file;

	file.name = "File";

	menu_bar_item exit;
	exit.name = "Exit";
	exit.callback = []() { glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE); };

	file.children.push_back(exit);
	items.push_back(file);
}

void RAGE_menu_bar::draw_menu_item(const menu_bar_item& item)
{
	if (item.children.size() == 0)
	{
		if (ImGui::MenuItem(item.name.c_str()))
		{
			std::cout << "Menu item clicked: " << item.name << std::endl;
			if (item.callback)
				item.callback();
		}
	}
	else
	{
		if (ImGui::BeginMenu(item.name.c_str()))
		{
			for (const auto& child : item.children)
			{
				draw_menu_item(child);
			}
			ImGui::EndMenu();
		}
	}
}

void RAGE_menu_bar::draw(RAGE *rage)
{
	this->rage = rage;
	if (ImGui::BeginMainMenuBar())
	{
		for (const auto& item : items)
		{
			draw_menu_item(item);
		}
		ImGui::EndMainMenuBar();
	}
}