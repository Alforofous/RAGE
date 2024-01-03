#include "RAGE_scene.hpp"
#include "RAGE.hpp"
#include <iostream>

RAGE_scene::RAGE_scene(const char *name)
{
	this->name = name;
}

void RAGE_scene::draw()
{
	RAGE_object::draw_objects(this->objects.data(), this->objects.size());
}

std::vector<RAGE_object *> *RAGE_scene::get_objects()
{
	return &this->objects;
}

std::vector<RAGE_object *> *RAGE_scene::get_selected_objects()
{
	return &this->selected_objects;
}

bool RAGE_scene::add_object(RAGE_object *object)
{
	this->objects.push_back(object);
	return true;
}

void RAGE_scene::delete_objects()
{
	for (int i = 0; i < this->objects.size(); i++)
	{
		RAGE_object *object = this->objects[i];
		RAGE_object::delete_object(object);
	}
	this->objects.clear();
}

void RAGE_scene::select_all_objects()
{
	this->selected_objects = this->objects;
}