#include "gui/RAGE_menu_bar.hpp"
#include "RAGE_primitive_objects.hpp"
#include "RAGE.hpp"

static void exit_callback(RAGE *rage, menu_bar_item *item)
{
	if (ImGui::MenuItem(item->name.c_str()))
	{
		glfwSetWindowShouldClose(rage->window->glfw_window, GLFW_TRUE);
	}
}

static void add_cube_callback(RAGE *rage, menu_bar_item *item)
{
	if (ImGui::MenuItem(item->name.c_str()))
	{
		rage->scene->add_object(RAGE_primitive_objects::create_cube(10.0f, 10.0f, 10.0f));
	}
}

static void show_performance_callback(RAGE *rage, menu_bar_item *item)
{
	if (ImGui::MenuItem(item->name.c_str()))
	{
		rage->gui->show_performance_window = !rage->gui->show_performance_window;
	}
}

static void toggle_vsync_callback(RAGE *rage, menu_bar_item *item)
{
	if (ImGui::MenuItem(item->name.c_str(), NULL, &rage->window->vsync))
	{
		printf("VSync: %d\n", rage->window->vsync);
		glfwSwapInterval(rage->window->vsync);
	}
}

RAGE_menu_bar::RAGE_menu_bar()
{
	this->items = {
		menu_bar_item{
			"File",
			NULL,
			{
				menu_bar_item{
					"Exit",
					exit_callback,
					{}
				}
			}
		},
		menu_bar_item{
			"Object",
			NULL,
			{
				menu_bar_item{
					"Add object",
					NULL,
					{
						menu_bar_item{
							"Add cube",
							add_cube_callback,
							{}
						}
					}
				}
			}
		},
		menu_bar_item{
			"View",
			NULL,
			{
				menu_bar_item{
					"Performance",
					NULL,
					{
						menu_bar_item{
							"Show performance",
							show_performance_callback,
							{}
						},
						menu_bar_item{
							"VSync",
							toggle_vsync_callback,
							{}
						}
					}
				}
			}
		}
	};
}

void RAGE_menu_bar::draw_menu_item(const menu_bar_item &item)
{
	if (item.children.size() == 0)
	{
		if (item.callback)
			item.callback(this->rage, (menu_bar_item *)&item);
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