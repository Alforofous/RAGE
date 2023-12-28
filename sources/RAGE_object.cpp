#include "RAGE_object.hpp"
#include "RAGE_mesh.hpp"
#include "RAGE.hpp"
#include "glm/gtc/type_ptr.hpp"

RAGE_object::RAGE_object(RAGE_mesh *mesh, const char *name)
{
	this->mesh = mesh;
	this->name = name;
	this->polygon_mode = polygon_mode::fill;
}

bool RAGE_object::init()
{
	return (true);
}

bool RAGE_object::load_GLB_mesh(const char *path)
{
	this->mesh = new RAGE_mesh();
	if (this->mesh->LoadGLB(path) == false)
		return (false);
	return (true);
}

bool RAGE_object::has_mesh()
{
	if (this->mesh == NULL)
		return (false);
	return (true);
}

RAGE_mesh *RAGE_object::get_mesh()
{
	return (this->mesh);
}

std::string RAGE_object::get_name()
{
	return (this->name);
}

RAGE_geometry *RAGE_object::get_geometry()
{
	return (&this->geometry);
}

void RAGE_object::draw_objects(RAGE_object **objects, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		RAGE_object *object = objects[i];
		object->draw();
	}
}

void RAGE_object::init_objects(RAGE_object **objects, size_t count)
{
	for (int i = 0; i < count; i++)
	{
		RAGE_object *rage_object = objects[i];
		RAGE_geometry *geometry = rage_object->get_geometry();

		rage_object->update_model_matrix();
	}
}

void RAGE_object::draw()
{
	if (this->has_mesh() == false)
		return;
	if (this->polygon_mode == polygon_mode::fill)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else if (this->polygon_mode == polygon_mode::line)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else if (this->polygon_mode == polygon_mode::point)
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
	RAGE_geometry *geometry = this->get_geometry();
	RAGE *rage = get_rage();

	const glm::f32 *model_matrix = glm::value_ptr(this->get_model_matrix());
	glUniformMatrix4fv(rage->shader->variable_location["u_model_matrix"], 1, GL_FALSE, glm::value_ptr(this->get_model_matrix()));
	geometry->draw();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

RAGE_object::~RAGE_object()
{
	if (this->mesh != NULL)
		delete this->mesh;
}