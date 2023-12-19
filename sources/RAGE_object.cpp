#include "RAGE_object.hpp"
#include "RAGE_mesh.hpp"

RAGE_object::RAGE_object()
{
	this->mesh = NULL;
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

RAGE_object::~RAGE_object()
{
	if (this->mesh != NULL)
		delete this->mesh;
}