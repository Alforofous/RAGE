#include "RAGE_mesh.hpp"

RAGE_mesh::RAGE_mesh(RAGE_material *material)
{
	this->material = material;
}

RAGE_mesh::~RAGE_mesh()
{
	if (this->material != NULL)
		delete this->material;
}