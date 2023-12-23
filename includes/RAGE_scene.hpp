#pragma once

#include "RAGE_object.hpp"
#include <vector>

class RAGE;

class RAGE_scene
{
public:
	RAGE_scene(const char *name = "New Scene");

	void draw(RAGE *rage);
	bool add_object(RAGE_object *object);
	std::vector<RAGE_object *> *get_objects();
	std::vector<RAGE_object *> *get_selected_objects();
	void print_objects();
	void select_all_objects();
private:
	std::string name;
	std::vector<RAGE_object *> objects;
	std::vector<RAGE_object *> selected_objects;
};