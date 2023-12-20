#include "RAGE_object.hpp"
#include "RAGE_mesh.hpp"

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

RAGE_object::~RAGE_object()
{
	if (this->mesh != NULL)
		delete this->mesh;
}