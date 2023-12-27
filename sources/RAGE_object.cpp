#include "RAGE_object.hpp"
#include "RAGE_mesh.hpp"
#include "RAGE.hpp"
#include "glm/gtc/type_ptr.hpp"

RAGE_object::RAGE_object(RAGE_mesh *mesh, const char *name)
{
	this->mesh = mesh;
	this->name = name;
}

bool RAGE_object::init()
{
	if (this->gl_object.init(*this->mesh) == false)
		return (false);
	return (true);
}

bool RAGE_object::load_GLB_mesh(const char *path)
{
	this->mesh = new RAGE_mesh();
	if (this->mesh->LoadGLB(path) == false)
		return (false);
	if (this->gl_object.init(*this->mesh) == false)
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

GLobject *RAGE_object::get_gl_object()
{
	return (&this->gl_object);
}

void RAGE_object::draw_objects(RAGE_object **objects, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		RAGE_object *object = objects[i];
		object->draw();
	}
}

void RAGE_object::draw()
{
	if (this->has_mesh() == false)
		return;
	GLobject *gl_object = this->get_gl_object();
	RAGE *rage = get_rage();

	const glm::f32 *model_matrix = glm::value_ptr(this->get_model_matrix());
	glUniformMatrix4fv(rage->shader->variable_location["u_model_matrix"], 1, GL_FALSE, glm::value_ptr(this->get_model_matrix()));
	gl_object->draw();
}

RAGE_object::~RAGE_object()
{
	if (this->mesh != NULL)
		delete this->mesh;
}