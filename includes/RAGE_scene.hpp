#pragma once

#include "RAGE_object.hpp"
#include <vector>

class RAGE;

class RAGE_scene
{
public:
	RAGE_scene(const char *name = "New Scene");
	~RAGE_scene();

	void draw();
	bool add_object(RAGE_object *object);
	std::vector<RAGE_object *> *get_objects();
	std::vector<RAGE_object *> *get_selected_objects();
	void select_all_objects();
	std::string name;

private:
	void delete_objects();
	std::vector<RAGE_object *> objects;
	std::vector<RAGE_object *> selected_objects;
};