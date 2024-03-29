#include "RAGE_object.hpp"
#include "RAGE.hpp"
#include "glm/gtc/type_ptr.hpp"

RAGE_object::RAGE_object(RAGE_mesh *mesh, const char *name)
{
	this->mesh = mesh;
	if (this->mesh == NULL)
		this->mesh = new RAGE_mesh();
	this->name = name;
	this->polygon_mode = polygon_mode::fill;
}

bool RAGE_object::init()
{
	return (true);
}

void RAGE_object::draw_objects(RAGE_object **objects, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		RAGE_object *object = objects[i];
		object->update_model_matrix();
		object->draw();
	}
}

void RAGE_object::draw()
{
	if (this->mesh == NULL || this->mesh->primitives.size() == 0 || this->mesh->material == NULL)
		return;
	if (this->polygon_mode == polygon_mode::fill)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else if (this->polygon_mode == polygon_mode::line)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else if (this->polygon_mode == polygon_mode::point)
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
	RAGE *rage = get_rage();

	rage->shader->update_uniform("u_model_matrix", this);
	
	this->mesh->material->use_shader();

	for (size_t i = 0; i < this->mesh->primitives.size(); i++)
	{
		RAGE_primitive *primitive = this->mesh->primitives[i];
		primitive->draw();
	}
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void RAGE_object::delete_object(RAGE_object *object)
{
	if (object != NULL)
		delete object;
	object = NULL;
}

void RAGE_object::remove_mesh()
{
	if (this->mesh != NULL)
		delete this->mesh;
	this->mesh = NULL;
}

RAGE_object::~RAGE_object()
{
	this->remove_mesh();
}