#pragma once

#include "RAGE_object.hpp"
#include <vector>

class RAGE_scene
{
public:
	RAGE_scene(const char *name = "New Scene");

	void draw();
	bool add_object(RAGE_object *object);
	void print_objects();
private:
	std::string name;
	std::vector<RAGE_object *> objects;
};