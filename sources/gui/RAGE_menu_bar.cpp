#include "RAGE_menu_bar.hpp"
#include "RAGE_primitive_objects.hpp"
#include "RAGE.hpp"

RAGE_menu_bar::RAGE_menu_bar()
{
	menu_bar_item file;

	file.name = "File";

	menu_bar_item exit;
	exit.name = "Exit";
	exit.callback = [](RAGE *rage) { glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE); };

	menu_bar_item object;
	object.name = "Object";

	menu_bar_item add_object;
	add_object.name = "Add object";

	std::vector<menu_bar_item> object_children;
	object_children.push_back({ "Add cube", [](RAGE *rage) { rage->scene.add_object(RAGE_primitive_objects::create_cube(10.0f, 10.0f, 10.0f)); } });

	add_object.children = object_children;

	object.children.push_back(add_object);

	file.children.push_back(exit);
	
	this->items.push_back(file);
	this->items.push_back(object);
}

void RAGE_menu_bar::draw_menu_item(const menu_bar_item& item)
{
	if (item.children.size() == 0)
	{
		if (ImGui::MenuItem(item.name.c_str()))
		{
			std::cout << "Menu item clicked: " << item.name << std::endl;
			if (item.callback)
				item.callback(this->rage);
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
		for (const auto& item : this->items)
		{
			draw_menu_item(item);
		}
		this->window_size = ImGui::GetWindowSize();
		ImGui::EndMainMenuBar();
	}
}

ImVec2 RAGE_menu_bar::get_window_size() { return (this->window_size); }