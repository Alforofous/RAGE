#include "RAGE_menu_bar.hpp"
#include <iostream>

RAGE_menu_bar::RAGE_menu_bar()
{
	items.push_back(
		{"File",
		 {
			 {"New"},
			 {"Open"},
			 {"Save"},
			 {"Save as"},
			 {"Exit"},
		 }});
	items.push_back(
		{"Edit",
		 {
			 {"Undo"},
			 {"Redo"},
			 {"Cut"},
			 {"Copy"},
			 {"Paste"},
			 {"Delete"},
			 {"Select all"},
		 }});
}

void RAGE_menu_bar::draw()
{
	if (ImGui::BeginMainMenuBar())
	{
		for (auto &item : items)
		{
			if (item.children.size() == 0)
			{
				if (ImGui::MenuItem(item.name.c_str()))
					std::cout << "Menu item clicked: " << item.name << std::endl;
			}
			else
			{
				if (ImGui::BeginMenu(item.name.c_str()))
				{
					for (auto &child : item.children)
					{
						if (ImGui::MenuItem(child.name.c_str()))
							std::cout << "Menu item clicked: " << child.name << std::endl;
					}
					ImGui::EndMenu();
				}
			}
		}
		ImGui::EndMainMenuBar();
	}
}