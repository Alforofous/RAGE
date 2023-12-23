#include "RAGE_scene.hpp"
#include "RAGE.hpp"
#include <iostream>

RAGE_scene::RAGE_scene(const char *name)
{
	this->name = name;
}

void RAGE_scene::draw(RAGE *rage)
{
	for (int i = 0; i < this->objects.size(); i++)
		this->objects[i]->update_model_matrix();
	for (int i = 0; i < this->objects.size(); i++)
	{
		if (this->objects[i]->has_mesh() == true)
		{
			RAGE_object *rage_object = this->objects[i];
			GLobject *gl_object = rage_object->get_gl_object();

			if (gl_object->is_initialized() == false)
				gl_object->init(*rage_object->get_mesh());

			glUniformMatrix4fv(rage->shader->variable_location["u_model_matrix"], 1,
							   GL_FALSE, glm::value_ptr(rage_object->get_model_matrix()));
			(rage_object->get_gl_object())->draw();
		}
	}
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

void RAGE_scene::print_objects()
{
	for (int i = 0; i < this->objects.size(); i++)
	{
		std::cout << this->objects[i]->get_name() << std::endl;
	}
}

void RAGE_scene::select_all_objects()
{
	this->selected_objects = this->objects;
}