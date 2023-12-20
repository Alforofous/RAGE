#include "RAGE_scene.hpp"
#include <iostream>

RAGE_scene::RAGE_scene(const char *name)
{
	this->name = name;
}

void RAGE_scene::draw()
{
	for (int i = 0; i < this->objects.size(); i++)
	{
		if (this->objects[i]->has_mesh() == true)
		{
			GLobject *gl_object = this->objects[i]->get_gl_object();

			if (gl_object->is_initialized() == false)
				gl_object->init(*this->objects[i]->get_mesh());
			(this->objects[i]->get_gl_object())->draw();
		}
	}
}

bool RAGE_scene::add_object(RAGE_object *object)
{
	this->objects.push_back(object);
	return true;
}

void RAGE_scene::print_objects()
{
	for (int i = 0; i < this->objects.size(); i++)
	{
		std::cout << this->objects[i]->get_name() << std::endl;
	}
}